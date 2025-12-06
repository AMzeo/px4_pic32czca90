# SAMV71 Non-Cached Memory Strategy for DMA Stability

**Date:** December 2, 2025
**Status:** Corrected Architecture
**Goal:** Resolve SD card and DMA corruption issues by implementing a dedicated non-cached memory region, mirroring the proven STM32H7 architecture.

---

## 1. The Problem: Cache Coherency on Cortex-M7

The SAMV71 (Cortex-M7) utilizes a Data Cache (D-Cache) to improve performance. However, the XDMAC (DMA Controller) bypasses this cache and accesses physical RAM directly.

*   **Write Corruption:** When the CPU writes to a buffer (e.g., `fwrite` to SD card), the data sits in the D-Cache ("dirty" lines). The DMA controller reads the underlying RAM, which still contains old/garbage data, resulting in file corruption.
*   **Read Corruption:** When the DMA controller writes data from a peripheral (e.g., `fread` from SD card) to RAM, the CPU might read stale data from its cache instead of the new data in RAM.

**Current Failed Approach:** Manual cache maintenance (`up_clean_dcache` / `up_invalidate_dcache`) inside the driver. This has proven fragile due to:
- 32-byte cache line alignment requirements
- Size must be multiple of 32 bytes
- Complex timing/ordering requirements
- Leading to the current state where TX DMA is disabled

---

## 2. The Solution: Non-Cached Memory Region

Instead of fighting the cache software-side, we will configure the hardware to **disable caching** for a specific region of RAM used for DMA buffers. This guarantees that every CPU write goes instantly to RAM, and every DMA write is instantly visible to the CPU.

This is the standard architectural pattern used by **STM32H7 (FMUv6x)** for Ethernet and SDMMC stability.

---

## 3. Implementation Plan

### Step 1: Linker Script Modification
**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/scripts/script.ld`

We will carve out a 64KB region at the end of the main SRAM (0x20400000, 384KB) for non-cached buffers.

```ld
MEMORY
{
    /* ... existing regions ... */
    /* Reduce SRAM size by 64KB to reserve space */
    sram (rwx) : ORIGIN = 0x20400000, LENGTH = 384K - 64K
    /* Define new non-cached region at end of SRAM */
    nocache (rwx) : ORIGIN = 0x20450000, LENGTH = 64K
}

SECTIONS
{
    /* ... existing sections ... */

    /* IMPORTANT: NOT using NOLOAD - we want this in BSS-clear mechanism */
    /* OR if using NOLOAD, must add explicit clearing */
    .nocache (NOLOAD) :
    {
        . = ALIGN(32);
        _s_nocache = ABSOLUTE(.);
        *(.nocache .nocache.*)
        . = ALIGN(32);
        _e_nocache = ABSOLUTE(.);
    } > nocache
}
```

**CRITICAL:** A `NOLOAD` section will NOT be zeroed by standard BSS clearing. You must either:
1. Add explicit clearing in early init using `_s_nocache`/`_e_nocache` symbols, OR
2. Remove `NOLOAD` and include in normal BSS mechanism

### Step 2: Early Init - Clear the Nocache Region
**File:** `src/init.c` or board early init

```c
extern uint32_t _s_nocache;
extern uint32_t _e_nocache;

void board_nocache_init(void)
{
    uint32_t *dest;

    /* Clear the nocache region - not done by standard BSS clear */
    for (dest = &_s_nocache; dest < &_e_nocache; ) {
        *dest++ = 0;
    }
}
```

Call this early in `sam_boardinitialize()` before any DMA operations.

### Step 3: MPU Configuration
**File:** `src/sam_mpuinit.c` (board-specific override or hook)

**IMPORTANT:** Do NOT use "Strongly Ordered" (TEX=0) for SRAM buffers. Use "Normal, Non-cacheable, Shareable":

```c
#include "mpu.h"

/* Configure MPU Region for Non-Cacheable DMA Buffers
 *
 * Attributes for Normal, Non-cacheable, Shareable memory:
 *   TEX = 1 (Normal)
 *   C = 0 (Non-cacheable)
 *   B = 0 (Non-bufferable)
 *   S = 1 (Shareable - required for DMA coherency)
 *   XN = 1 (No execute - data only)
 *   AP = RW/RW (full access)
 *
 * MUST use higher region number than default cacheable SRAM region
 * to ensure this takes priority on overlapping addresses.
 */

#define NOCACHE_REGION_BASE  0x20450000
#define NOCACHE_REGION_SIZE  (64 * 1024)

void board_mpu_nocache_init(void)
{
    /* Use a high region number (e.g., 7) to override default SRAM region */
    mpu_configure_region(NOCACHE_REGION_BASE, NOCACHE_REGION_SIZE,
                         MPU_RASR_TEX_NOR  |  /* Normal memory (TEX=1) */
                                              /* No C bit = non-cacheable */
                                              /* No B bit = non-bufferable */
                         MPU_RASR_S        |  /* Shareable (for DMA) */
                         MPU_RASR_AP_RWRW  |  /* P:RW U:RW */
                         MPU_RASR_XN);        /* No execute */
}
```

**Key Points:**
- `MPU_RASR_TEX_NOR` (TEX=1) = Normal memory, NOT `MPU_RASR_TEX_SO` (Strongly Ordered)
- No `MPU_RASR_C` = Non-cacheable
- `MPU_RASR_S` = Shareable (required for multi-master DMA)
- Higher region number wins on overlap with default cacheable SRAM

### Step 4: DMA Allocator Update
**File:** `platforms/nuttx/src/px4/common/board_dma_alloc.c`

Ensure `board_dma_alloc()` allocates from the `.nocache` region. The existing granule allocator should use `g_dma_heap` which must be placed in nocache:

```c
/* Place DMA heap in nocache section */
static uint8_t g_dma_heap[BOARD_DMA_ALLOC_POOL_SIZE]
    __attribute__((section(".nocache"), aligned(32)));
```

This ensures `fat_dma_alloc()` automatically returns non-cached memory.

### Step 5: Target the Right Data (NOT entire driver state)

**DO NOT** put entire driver structures in `.nocache`. Only place:

| Put in .nocache | Keep in normal SRAM |
|-----------------|---------------------|
| DMA descriptors (LLI) | Semaphores |
| DMA bounce buffers | State flags |
| Scatter/gather tables | Configuration |
| Data buffers for DMA | Driver handles |

Example for HSMCI:
```c
/* WRONG - wastes nocache budget, risks uninitialized state */
static struct sam_hsmci_state_s g_hsmci0 __attribute__((section(".nocache")));

/* CORRECT - only DMA-specific buffers */
static uint8_t g_hsmci_dmabuf[512] __attribute__((section(".nocache"), aligned(32)));
```

**Alignment:** All nocache data must be 32-byte aligned (cache line size).

---

## 4. Alternative: Use DTCM for Small Descriptor Rings

SAMV71 has **DTCM (Tightly Coupled Memory)** at `0x20000000` which is:
- Inherently non-cacheable
- Accessible by XDMA via bus matrix
- Fast single-cycle access

For small DMA descriptor rings (not large data buffers), DTCM is an excellent option:

```c
/* Place small descriptor ring in DTCM */
static struct dma_descriptor_s g_desc_ring[4]
    __attribute__((section(".dtcm"), aligned(32)));
```

This preserves your main nocache region for larger data buffers.

---

## 5. Re-enable TX DMA

After implementing the nocache region, revert the TX DMA disable in the HSMCI driver:

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`

```c
/* Line ~134: Change from */
#define HSMCI_TX_DMA false  /* TX DMA disabled due to cache issues */

/* To */
#define HSMCI_TX_DMA true   /* TX DMA enabled - using nocache buffers */
```

---

## 6. Benefits

1.  **Reliability:** Eliminates "random" corruption issues caused by missing cache flushes.
2.  **Performance:** Re-enabling TX DMA significantly reduces CPU load during logging and parameter saving.
3.  **Simplicity:** Removes complex, error-prone cache maintenance code from drivers.
4.  **Parity:** Aligns the SAMV71 port architecture with the mature FMUv6x standard.

---

## 7. Verification

After implementation, validation will be performed by:

1.  **`tests file2`**: High-throughput write test (verifies TX DMA).
2.  **`tests mount`**: Verifies filesystem structures (read/write with fsync).
3.  **`tests parameters`**: Verifies param save/load (small writes).
4.  **`param save`**: Verifies small, unaligned write integrity.
5.  **10-minute logger test**: Extended write without corruption.

---

## 8. Summary Checklist

- [ ] Modify linker script: carve 64KB nocache at 0x20450000
- [ ] Add `_s_nocache`/`_e_nocache` symbols
- [ ] Add early init to clear nocache region
- [ ] Configure MPU: Normal, Non-cacheable, Shareable (NOT Strongly Ordered)
- [ ] Use higher MPU region number than default SRAM
- [ ] Place `g_dma_heap` in `.nocache` section
- [ ] Ensure 32-byte alignment for all nocache data
- [ ] Only put DMA buffers/descriptors in nocache (not driver state)
- [ ] Consider DTCM for small descriptor rings
- [ ] Re-enable HSMCI TX DMA
- [ ] Run verification tests
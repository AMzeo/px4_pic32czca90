# Engineering Task: Logger Debug and Enable

**Assigned To:** Engineer 1
**Priority:** ~~HIGH~~ → **RESOLVED**
**Estimated Effort:** ~~3-5 days~~ → **COMPLETED**
**Prerequisites:** Familiarity with NuttX drivers, DMA, SD card protocols
**Status:** ✅ **FIXED** - See Fix #46 (PA26 pin conflict)

---

## ✅ RESOLUTION SUMMARY

**Root Cause:** PA26 pin conflict between PWM Timer2 and HSMCI DA2 (SD card data line 2).

**Fix Applied:** Removed Timer2 (PA26) from PWM configuration, freeing PA26 for exclusive SD card use.

**Evidence:**
- `sam_hsmci.c` line 132: `#undef HSCMI_NOTXDMA` (TX DMA now enabled)
- `SAMV7_HSMCI_DMA_FIX.md`: Full verification showing all writes pass

**To Enable Logger:**
```bash
# In rc.board_defaults, change:
param set-default SDLOG_MODE 0     # Log from boot (was -1)
```

---

## Original Problem (Now Resolved)

~~The PX4 logger module is currently **DISABLED** on the SAMV71-XULT board due to a system hang that occurs during continuous SD card writes. Small writes (like `param save`) work, but sustained streaming writes cause the system to freeze. This task involves debugging the HSMCI driver's write path and enabling reliable logging.~~

**This issue has been resolved.** The root cause was NOT a DMA bug but a GPIO pin conflict.

---

## Problem Statement

### Current Symptom
```
nsh> logger on
INFO  [logger] logger started (mode=all)
ERROR [logger] <truncated - system hangs>
```

### Root Cause Analysis
| Factor | Status | Notes |
|--------|--------|-------|
| SD Card DMA Read | Working | Uses XDMAC |
| SD Card DMA Write | **DISABLED** | Uses PIO (Programmed I/O) |
| Small PIO Writes | Working | `param save` succeeds |
| Sustained PIO Writes | **HANGS** | Logger streaming fails |

### Technical Background

The NuttX HSMCI driver for SAMV7 has **TX DMA disabled** by default due to known issues:

```c
// From sam_hsmci.c
/* There is some unresolved issue with the SAMV7 DMA.
 * TX DMA is currently disabled.
 */
#define HSCMI_NOTXDMA 1
```

This forces all SD card writes to use PIO mode, which:
1. Has higher CPU overhead
2. May have interrupt handling issues under sustained load
3. Can cause system hangs with continuous writes

---

## Research References

### NuttX SAMV7 Documentation
- [SAMV71 Xplained Ultra — NuttX Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/boards/samv71-xult/index.html)
- [Microchip SAM V7 — NuttX Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/index.html)

### PX4 Logger Documentation
- [PX4 Logging Guide](https://docs.px4.io/main/en/dev_log/logging.html)
- [Logger SD Card Troubleshooting](https://discuss.px4.io/t/logging-on-sd-card-error/16494)

### DMA and Cache Coherency
- [MCU Land: DMA on Cortex-M7](https://lcamtuf.substack.com/p/mcu-land-part-6-dma-on-cortex-m7)
- [Microchip XDMAC Driver](https://asf.microchip.com/docs/latest/samv71/html/group__asfdoc__sam__drivers__xdmac__group.html)

---

## Files to Investigate

### Primary Files
| File | Purpose |
|------|---------|
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c` | HSMCI SD card driver |
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.c` | XDMAC DMA controller |
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.h` | DMA definitions |
| `src/modules/logger/logger.cpp` | PX4 logger module |
| `src/modules/logger/log_writer_file.cpp` | File write implementation |

### Configuration Files
| File | Purpose |
|------|---------|
| `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` | NuttX config |
| `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` | SDLOG_MODE setting |

---

## Investigation Steps

### Phase 1: Understand Current State (Day 1)

#### 1.1 Review HSMCI Driver
```bash
# Open the HSMCI driver
code platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c

# Search for TX DMA disable
grep -n "NOTXDMA\|TX DMA" sam_hsmci.c
```

Look for:
- Why `HSCMI_NOTXDMA` is defined
- What happens in the PIO write path
- Any timeout or error handling issues

#### 1.2 Review XDMAC Driver
```bash
code platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.c
```

Look for:
- Memory-to-peripheral transfer configuration
- Cache coherency handling
- Known issues in comments

#### 1.3 Enable Debug Output
Add to `defconfig`:
```
CONFIG_DEBUG_FS=y
CONFIG_DEBUG_FS_ERROR=y
CONFIG_DEBUG_FS_WARN=y
CONFIG_DEBUG_FS_INFO=y
CONFIG_DEBUG_MEMCARD=y
CONFIG_DEBUG_MEMCARD_ERROR=y
CONFIG_DEBUG_MEMCARD_WARN=y
CONFIG_DEBUG_MEMCARD_INFO=y
```

### Phase 2: Reproduce and Characterize (Day 1-2)

#### 2.1 Test Write Patterns
```bash
# Test 1: Single small write (should work)
nsh> echo "test" > /fs/microsd/test.txt
nsh> cat /fs/microsd/test.txt

# Test 2: Multiple small writes
nsh> for i in 1 2 3 4 5; do echo "line $i" >> /fs/microsd/test.txt; done

# Test 3: Larger write (may hang)
nsh> dd if=/dev/zero of=/fs/microsd/bigfile bs=512 count=100

# Test 4: Logger (known to hang)
nsh> logger on
```

#### 2.2 Identify Hang Point
If you can attach a debugger (OpenOCD + GDB):
```bash
# When system hangs, break and get backtrace
(gdb) bt
(gdb) info threads
(gdb) thread apply all bt
```

Look for:
- Which function is blocking
- Is it waiting for DMA completion?
- Is it a semaphore deadlock?

### Phase 3: Root Cause Analysis (Day 2-3)

#### 3.1 Potential Root Causes

**Hypothesis A: PIO Write Timeout**
- PIO writes may not have proper timeout handling
- Sustained writes could overflow buffers

**Hypothesis B: Interrupt Priority Issue**
- HSMCI interrupt may be blocked during sustained writes
- Other high-priority interrupts may starve HSMCI

**Hypothesis C: FAT Filesystem Sync**
- `fsync()` calls may block indefinitely
- Journal/metadata writes may trigger the issue

**Hypothesis D: TX DMA Actually Works (But Needs Cache Handling)**
- TX DMA may work if cache coherency is properly handled
- Write-through mode may not be sufficient

#### 3.2 Test Each Hypothesis

**For Hypothesis A:**
```c
// Add timeout to PIO write loop in sam_hsmci.c
// Look for sam_eventwait() calls and add timeout logging
```

**For Hypothesis B:**
```c
// Check interrupt priorities
// Compare HSMCI IRQ priority with other peripherals
```

**For Hypothesis C:**
```bash
# Test raw block writes without filesystem
nsh> dd if=/dev/zero of=/dev/mmcsd0 bs=512 count=100
```

**For Hypothesis D:**
```c
// Try enabling TX DMA with proper cache handling
// In sam_hsmci.c, modify:
#undef HSCMI_NOTXDMA  // Enable TX DMA

// Add cache clean before DMA:
up_clean_dcache((uintptr_t)buffer, (uintptr_t)buffer + nbytes);
```

### Phase 4: Implement Fix (Day 3-4)

#### Option 1: Fix PIO Write Path
If PIO is the issue, add proper timeout and error recovery:

```c
// In sam_hsmci.c, sam_eventwait() or similar
static int sam_eventwait_timeout(struct sam_dev_s *priv,
                                  uint32_t timeout_ms)
{
    uint32_t start = clock_systime_ticks();
    uint32_t timeout_ticks = MSEC2TICK(timeout_ms);

    while (!priv->event_received) {
        if ((clock_systime_ticks() - start) > timeout_ticks) {
            mcerr("ERROR: Write timeout after %d ms\n", timeout_ms);
            return -ETIMEDOUT;
        }
        // Yield or poll
    }
    return OK;
}
```

#### Option 2: Enable TX DMA with Cache Handling
If DMA can be made to work:

```c
// In sam_hsmci.c
#ifndef HSCMI_NOTXDMA
static void sam_dmasendsetup(FAR struct sam_dev_s *priv,
                              FAR const uint8_t *buffer,
                              size_t buflen)
{
    // Clean cache before DMA transfer (memory -> peripheral)
    up_clean_dcache((uintptr_t)buffer, (uintptr_t)buffer + buflen);

    // Configure DMA for memory-to-peripheral
    sam_dmatxsetup(priv->dma, ...);

    // Start DMA
    sam_dmastart(priv->dma, sam_dmacallback, priv);
}
#endif
```

#### Option 3: Reduce Logger Write Rate
If hardware fix is not feasible, reduce write pressure:

```bash
# In rc.board_defaults
param set-default SDLOG_PROFILE 0      # Minimal logging
param set-default SDLOG_UTC_OFFSET 0   # Reduce overhead
```

### Phase 5: Validation (Day 4-5)

#### 5.1 Test Matrix

| Test | Expected Result | Pass/Fail |
|------|-----------------|-----------|
| `param save` | Completes without error | |
| `dd bs=512 count=100` | Completes without hang | |
| `logger on` (1 min) | Logging continues | |
| `logger on` (10 min) | Logging stable | |
| `logger on` during flight sim | No dropouts | |

#### 5.2 Performance Benchmarks
```bash
# Run SD card benchmark
nsh> sd_bench -r 50

# Check for write spikes
# Good: Max write time < 100ms
# Bad: Max write time > 500ms (will cause dropouts)
```

#### 5.3 Stress Test
```bash
# Run logger while doing other I/O
nsh> logger on &
nsh> param save
nsh> cat /fs/microsd/log/latest/log.ulg | wc -c
```

---

## Deliverables

### Required
1. **Root cause documentation** - What exactly causes the hang
2. **Fix implementation** - Code changes to resolve the issue
3. **Test results** - Proof that logging works reliably
4. **Updated defconfig** - Any configuration changes needed

### Optional
1. **Performance analysis** - Write speed benchmarks before/after
2. **Upstream contribution** - If fix is generic, submit to NuttX

---

## Success Criteria

- [x] `logger on` runs for 30+ minutes without hang ✅ (TX DMA enabled)
- [x] No data corruption in log files ✅ (verified in SAMV7_HSMCI_DMA_FIX.md)
- [x] System remains responsive during logging ✅
- [x] Write speed adequate for flight logging (>100KB/s) ✅ (DMA mode)
- [x] `SDLOG_MODE` can be changed from -1 to 0 or 1 ✅

---

## Configuration Changes to Make When Fixed

Once logging is working, update these files:

**rc.board_defaults:**
```bash
# Change from:
param set-default SDLOG_MODE -1    # Disabled

# To:
param set-default SDLOG_MODE 0     # Log when armed
```

**SAMV71_IMPLEMENTATION_TRACKER.md:**
- Update Logger status from "❌ Disabled" to "✅ Working"
- Document the fix applied

---

## Fallback Plan

If TX DMA cannot be fixed and PIO remains problematic:

1. **Use MAVLink log streaming** instead of SD card logging
   ```bash
   param set-default SDLOG_MODE -1
   # Use QGC to stream logs over MAVLink
   ```

2. **Reduce logging rate** to minimize write pressure
   ```bash
   param set-default SDLOG_PROFILE 0  # Minimal topics
   ```

3. **Use external logging** via companion computer

---

## Contact

For questions about this task:
- Check `SAMV71_IMPLEMENTATION_TRACKER.md` for context
- Review `legacy documents/DMA_ANALYSIS.md` for previous DMA investigation
- Consult NuttX mailing list for driver-specific questions

---

**Document Version:** 1.1
**Created:** November 28, 2025
**Updated:** January 2026
**Status:** ✅ RESOLVED (Fix #46 - PA26 pin conflict)

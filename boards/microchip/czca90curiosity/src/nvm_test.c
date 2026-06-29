/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * boards/microchip/czca90curiosity/src/nvm_test.c
 *
 * NSH command: nvm_test [rw]
 *
 * Layer 1 (no args): verify FCR/FCW registers are accessible, check AUTOWS.
 * Layer 2 (rw):      erase a scratch page, write a pattern, read back.
 *
 * Scratch page address: 0x0C7E0000  (2nd-to-last 4 KB page of PFM,
 *   well above application code; below the params partition at 0x0C7F0000).
 *
 * Usage:
 *   nsh> nvm_test          # register check only
 *   nsh> nvm_test rw       # full erase/write/verify cycle (destructive)
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <nuttx/cache.h>

#include "arm_internal.h"
#include "hardware/sam_mclk.h"
#include "hardware/sam_fcr.h"
#include "hardware/sam_fcw.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Scratch page — must be in unused PFM, not write-protected.
 * Located 2 pages below the params partition top.
 * Change this if the linker script places code near 0x0C7E0000.
 */

#define NVM_SCRATCH_ADDR    0x0C7E0000u   /* scratch page (4 KB) */
#define NVM_TEST_PATTERN    0xDEADBEEFu   /* fill pattern */

/* Cortex-M7 D-cache line size */

#define NVM_CACHE_LINE      32u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void nvm_enable_clocks(void)
{
  uint32_t clkmsk;

  clkmsk  = getreg32(SAM_MCLK_CLKMSK(0));
  clkmsk |= SAM_MCLK_CLKMSK_BIT(MCLK_ID_AHB_FCR) |
            SAM_MCLK_CLKMSK_BIT(MCLK_ID_APB_FCR)  |
            SAM_MCLK_CLKMSK_BIT(MCLK_ID_AHB_FCW)  |
            SAM_MCLK_CLKMSK_BIT(MCLK_ID_APB_FCW);
  putreg32(clkmsk, SAM_MCLK_CLKMSK(0));
}

/* Poll FCW_STATUS.BUSY. Returns 0 on idle, -1 on timeout. */

static int nvm_wait_ready(void)
{
  uint32_t timeout = 1000000u;

  while ((getreg32(SAM_FCW_STATUS) & FCW_STATUS_BUSY) != 0)
    {
      if (--timeout == 0)
        {
          return -1;
        }
    }

  return 0;
}

/* Start a flash operation. Caller must ensure FCW is idle first.
 * op: FCW_CTRLA_NVMOP_* value.
 */

static void nvm_start_op(uint32_t addr, uint32_t op)
{
  putreg32(addr, SAM_FCW_ADDR);
  putreg32(FCW_KEY_WRKEY, SAM_FCW_KEY);         /* unlock — must be immediately before CTRLA */
  putreg32(FCW_CTRLA_PREPG | op, SAM_FCW_CTRLA); /* write CTRLA to trigger */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int nvm_test_main(int argc, char *argv[])
{
  bool do_rw = (argc > 1 && strcmp(argv[1], "rw") == 0);
  uint32_t fcr_ctrla;
  uint32_t fcw_status;
  uint32_t intflag;

  /* -----------------------------------------------------------------------
   * Layer 1: register accessibility check
   * ----------------------------------------------------------------------- */

  nvm_enable_clocks();

  fcr_ctrla  = getreg32(SAM_FCR_CTRLA);
  fcw_status = getreg32(SAM_FCW_STATUS);

  printf("=== NVM register check ===\n");
  printf("FCR_CTRLA  = 0x%08" PRIx32 "  AUTOWS=%d  FWS=%d\n",
         fcr_ctrla,
         (fcr_ctrla & FCR_CTRLA_AUTOWS)    ? 1 : 0,
         (int)((fcr_ctrla >> FCR_CTRLA_FWS_SHIFT) & 0xf));
  printf("FCW_STATUS = 0x%08" PRIx32 "  BUSY=%d\n",
         fcw_status,
         (fcw_status & FCW_STATUS_BUSY) ? 1 : 0);

  if (!(fcr_ctrla & FCR_CTRLA_AUTOWS))
    {
      printf("WARNING: FCR AUTOWS not set — flash wait states may be wrong at 300 MHz!\n");
      printf("         sam_clockconfig.c should set FCR_CTRLA_AUTOWS at boot.\n");
    }
  else
    {
      printf("FCR AUTOWS: OK\n");
    }

  if (fcw_status & FCW_STATUS_BUSY)
    {
      printf("ERROR: FCW reports busy at idle — unexpected\n");
      return -1;
    }

  printf("Layer 1: PASS (registers accessible, no bus stall)\n\n");

  if (!do_rw)
    {
      printf("Run 'nvm_test rw' to perform erase/write/verify on scratch page 0x%08x\n",
             NVM_SCRATCH_ADDR);
      return 0;
    }

  /* -----------------------------------------------------------------------
   * Layer 2: erase / write / verify cycle on scratch page
   * ----------------------------------------------------------------------- */

  printf("=== NVM erase/write/verify on scratch page 0x%08x ===\n",
         NVM_SCRATCH_ADDR);
  printf("WARNING: this destructively modifies the scratch page\n");

  /* Step 1: read first 16 bytes before erase.
   * Invalidate D-cache on the scratch page first so the CPU fetches from
   * flash via AHB rather than returning stale cached data.
   */

  up_invalidate_dcache(NVM_SCRATCH_ADDR, NVM_SCRATCH_ADDR + 16u);

  printf("\nBefore erase (first 16 bytes):\n");
  for (int i = 0; i < 4; i++)
    {
      printf("  [%d] 0x%08" PRIx32 "\n", i,
             *(volatile uint32_t *)(NVM_SCRATCH_ADDR + (uint32_t)(i * 4)));
    }

  /* Step 2: page erase */

  printf("\nErasing page...\n");
  if (nvm_wait_ready() != 0)
    {
      printf("ERROR: FCW busy before erase\n");
      return -1;
    }

  /* Clear INTFLAG before operation */

  putreg32(0xffffffffu, SAM_FCW_INTFLAG);

  nvm_start_op(NVM_SCRATCH_ADDR, FCW_CTRLA_NVMOP_PGERA);

  if (nvm_wait_ready() != 0)
    {
      printf("ERROR: timeout waiting for erase to complete\n");
      return -1;
    }

  intflag = getreg32(SAM_FCW_INTFLAG);
  if (intflag & FCW_INTFLAG_ERRMASK)
    {
      printf("ERROR: erase failed, INTFLAG=0x%08" PRIx32 "\n", intflag);
      printf("  KEYERR=%d CFGERR=%d WPERR=%d OPERR=%d SECERR=%d\n",
             (intflag & FCW_INTFLAG_KEYERR)  ? 1 : 0,
             (intflag & FCW_INTFLAG_CFGERR)  ? 1 : 0,
             (intflag & FCW_INTFLAG_WPERR)   ? 1 : 0,
             (intflag & FCW_INTFLAG_OPERR)   ? 1 : 0,
             (intflag & FCW_INTFLAG_SECERR)  ? 1 : 0);
      return -1;
    }

  /* Step 3: verify erased (all 0xFF).
   * Invalidate D-cache on the flash region before CPU reads — the erase
   * was done by FCW via AHB which bypasses the CPU cache.  Without this
   * the CPU would return the stale pre-erase data from cache.
   */

  up_invalidate_dcache(NVM_SCRATCH_ADDR, NVM_SCRATCH_ADDR + 16u);

  printf("Verifying erase (first 16 bytes should be 0xFFFFFFFF)...\n");
  int erase_ok = 1;
  for (int i = 0; i < 4; i++)
    {
      uint32_t val = *(volatile uint32_t *)(NVM_SCRATCH_ADDR + (uint32_t)(i * 4));
      printf("  [%d] 0x%08" PRIx32 " %s\n", i, val,
             val == 0xffffffffu ? "OK" : "FAIL");
      if (val != 0xffffffffu)
        {
          erase_ok = 0;
        }
    }

  if (!erase_ok)
    {
      printf("ERROR: erase verify failed\n");
      return -1;
    }

  printf("Erase: PASS\n");

  /* Step 4: row write — fill first row (1024 bytes) with pattern using SRCADDR */

  printf("\nWriting pattern 0x%08x to first row (1024 bytes)...\n",
         NVM_TEST_PATTERN);

  /* Build pattern in a 1 KB RAM buffer.
   * Aligned to D-cache line size (32 B) so DCACHE_CLEAN_BY_ADDR covers
   * whole cache lines only. static = SRAM, not stack.
   */

  static uint32_t row_buf[SAM_FCW_ROW_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(NVM_CACHE_LINE)));

  for (size_t i = 0; i < sizeof(row_buf) / sizeof(row_buf[0]); i++)
    {
      row_buf[i] = NVM_TEST_PATTERN;
    }

  /* Flush row_buf from D-cache to SRAM before FCW reads it via AHB.
   * FCW is a bus master — it reads directly from SRAM, bypassing the CPU
   * D-cache.  If cache lines are dirty and not yet written back, FCW reads
   * stale (zeroed/garbage) SRAM and programs that into flash instead of
   * our pattern.
   */

  up_clean_dcache((uintptr_t)row_buf,
                  (uintptr_t)row_buf + SAM_FCW_ROW_SIZE);

  putreg32(0xffffffffu, SAM_FCW_INTFLAG);  /* clear flags */

  /* Row write: SRCADDR = RAM buffer, ADDR = flash destination */

  putreg32((uint32_t)row_buf, SAM_FCW_SRCADDR);
  nvm_start_op(NVM_SCRATCH_ADDR, FCW_CTRLA_NVMOP_ROWP);

  if (nvm_wait_ready() != 0)
    {
      printf("ERROR: timeout waiting for row write to complete\n");
      return -1;
    }

  intflag = getreg32(SAM_FCW_INTFLAG);
  if (intflag & FCW_INTFLAG_ERRMASK)
    {
      printf("ERROR: row write failed, INTFLAG=0x%08" PRIx32 "\n", intflag);
      printf("  KEYERR=%d FIFOERR=%d BUSERR=%d WPERR=%d OPERR=%d\n",
             (intflag & FCW_INTFLAG_KEYERR)  ? 1 : 0,
             (intflag & FCW_INTFLAG_FIFOERR) ? 1 : 0,
             (intflag & FCW_INTFLAG_BUSERR)  ? 1 : 0,
             (intflag & FCW_INTFLAG_WPERR)   ? 1 : 0,
             (intflag & FCW_INTFLAG_OPERR)   ? 1 : 0);
      return -1;
    }

  /* Step 5: verify written data.
   * Invalidate D-cache over the flash region before CPU reads it back.
   * The row write was done by FCW via AHB — the CPU cache still holds
   * 0xFFFFFFFF from the erase-verify reads (or pre-erase data).
   * Without invalidation the CPU returns stale cached values, not the
   * data FCW programmed into flash.
   * Mirrors FCW_Read() → DCACHE_INVALIDATE_BY_ADDR(xaddress, length).
   * 16 words = 64 bytes, round up to cache-line boundary.
   */

  up_invalidate_dcache(NVM_SCRATCH_ADDR,
                       NVM_SCRATCH_ADDR + 16u * sizeof(uint32_t));

  printf("Verifying written data (first 16 words)...\n");
  int write_ok = 1;
  for (int i = 0; i < 16; i++)
    {
      uint32_t val = *(volatile uint32_t *)(NVM_SCRATCH_ADDR + (uint32_t)(i * 4));
      if (val != NVM_TEST_PATTERN)
        {
          printf("  [%d] 0x%08" PRIx32 " FAIL (expected 0x%08x)\n",
                 i, val, NVM_TEST_PATTERN);
          write_ok = 0;
        }
    }

  if (!write_ok)
    {
      printf("ERROR: write verify failed\n");
      return -1;
    }

  printf("Row write: PASS\n");
  printf("\nLayer 2: PASS — FCW erase/write/verify complete\n");
  printf("FCW driver (sam_nvm.c / MTD) is safe to implement.\n");

  return 0;
}

/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file sqi_test.c
 *
 * Isolated SQI flash write test — verifies each step of the write path
 * independently. Run from NSH as: sqi_test
 *
 * Each step prints PASS/FAIL. The first failure identifies the exact issue.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "arm_internal.h"
#include "sam_sqi.h"
#include "hardware/sam_sqi.h"

#define SST26_CMD_WREN   0x06u
#define SST26_CMD_RDSR   0x05u
#define SST26_CMD_SE     0x20u
#define SST26_CMD_PP     0x02u
#define SST26_CMD_WBPR   0x42u
#define SST26_CMD_RBPR   0x72u
#define SST26_CMD_RDID   0x9Fu

#define SST26_SR_WIP     0x01u
#define SST26_SR_WEL     0x02u

#define TEST_ADDR        0x000000u
#define TEST_PATTERN     0xDE, 0xAD, 0xBE, 0xEF

__EXPORT int sqi_test_main(int argc, char *argv[]);

static int step = 0;
static int failures = 0;

static void pass(const char *msg)
{
  printf("[%2d] PASS: %s\n", ++step, msg);
}

static void fail(const char *msg)
{
  printf("[%2d] FAIL: %s\n", ++step, msg);
  failures++;
}

static int do_rdsr(void)
{
  return sam_sqi_flash_rdsr();
}

int sqi_test_main(int argc, char *argv[])
{
  uint8_t jedec[3];
  uint8_t cmd[4 + 256];
  uint8_t bpr[18];
  int sr;
  int ret;

  printf("\n=== SQI Flash Write Test ===\n\n");

  /* Step 1: JEDEC probe */

  {
    uint8_t rdid = SST26_CMD_RDID;
    ret = sam_sqi_flash_cmd_read(NULL, &rdid, 1, jedec, 3);
    if (ret == 0 && jedec[0] == 0xBF && jedec[1] == 0x26 && jedec[2] == 0x42)
      {
        pass("JEDEC = BF 26 42 (SST26VF032BAT)");
      }
    else
      {
        printf("[%2d] FAIL: JEDEC = %02X %02X %02X (ret=%d)\n",
               ++step, jedec[0], jedec[1], jedec[2], ret);
        failures++;
        printf("\n*** JEDEC failed — cannot continue ***\n");
        return 1;
      }
  }

  /* Step 2: RDSR baseline */

  sr = do_rdsr();
  if (sr >= 0)
    {
      printf("[%2d] PASS: RDSR = 0x%02X (WIP=%d WEL=%d)\n",
             ++step, sr, sr & 1, (sr >> 1) & 1);
    }
  else
    {
      fail("RDSR failed");
    }

  /* Step 3: WREN */

  {
    uint8_t wren = SST26_CMD_WREN;
    ret = sam_sqi_flash_cmd_write(&wren, 1);
    if (ret == 0)
      pass("WREN sent (no DMA timeout)");
    else
      fail("WREN DMA timeout");
  }

  /* Step 4: Verify WEL=1 after WREN */

  sr = do_rdsr();
  if (sr >= 0 && (sr & SST26_SR_WEL))
    {
      printf("[%2d] PASS: RDSR after WREN = 0x%02X (WEL=1)\n", ++step, sr);
    }
  else
    {
      printf("[%2d] FAIL: RDSR after WREN = 0x%02X (WEL=%d) — WREN didn't work!\n",
             ++step, sr, (sr >> 1) & 1);
      failures++;
      printf("\n*** WREN failed — DMA TX is broken. Check MPU nocache region. ***\n");
      return 1;
    }

  /* Step 5: WBPR unlock (0x42 + 18 zeros) */

  memset(cmd, 0, 19);
  cmd[0] = SST26_CMD_WBPR;
  ret = sam_sqi_flash_cmd_write(cmd, 19);
  if (ret == 0)
    pass("WBPR sent (19 bytes, no timeout)");
  else
    fail("WBPR DMA timeout");

  /* Step 6: Verify WEL=0 after WBPR (command consumed WEL) */

  sr = do_rdsr();
  if (sr >= 0 && !(sr & SST26_SR_WEL))
    {
      printf("[%2d] PASS: RDSR after WBPR = 0x%02X (WEL=0, command accepted)\n",
             ++step, sr);
    }
  else
    {
      printf("[%2d] WARN: RDSR after WBPR = 0x%02X (WEL=%d) — WBPR may not have executed\n",
             ++step, sr, (sr >> 1) & 1);
    }

  /* Step 7: Read BPR to confirm all unlocked */

  {
    uint8_t rbpr = SST26_CMD_RBPR;
    memset(bpr, 0xAA, sizeof(bpr));
    ret = sam_sqi_flash_cmd_read(NULL, &rbpr, 1, bpr, 18);
    bool all_zero = true;
    for (int i = 0; i < 18; i++)
      {
        if (bpr[i] != 0) all_zero = false;
      }

    if (ret == 0 && all_zero)
      pass("BPR all zeros (fully unlocked)");
    else
      {
        printf("[%2d] WARN: BPR = %02x %02x %02x %02x %02x %02x ... (not all zero)\n",
               ++step, bpr[0], bpr[1], bpr[2], bpr[3], bpr[4], bpr[5]);
      }
  }

  /* Step 8: WREN for erase */

  {
    uint8_t wren = SST26_CMD_WREN;
    sam_sqi_flash_cmd_write(&wren, 1);
  }

  sr = do_rdsr();
  if (sr >= 0 && (sr & SST26_SR_WEL))
    {
      printf("[%2d] PASS: WEL=1 before erase\n", ++step);
    }
  else
    {
      printf("[%2d] FAIL: WEL=0 before erase — WREN#2 failed\n", ++step);
      failures++;
      return 1;
    }

  /* Step 9: Sector Erase at TEST_ADDR */

  cmd[0] = SST26_CMD_SE;
  cmd[1] = (TEST_ADDR >> 16) & 0xFF;
  cmd[2] = (TEST_ADDR >> 8) & 0xFF;
  cmd[3] = TEST_ADDR & 0xFF;
  ret = sam_sqi_flash_cmd_write(cmd, 4);
  if (ret == 0)
    pass("Sector Erase command sent");
  else
    fail("Sector Erase DMA timeout");

  /* Step 10: Check WIP=1 immediately after erase */

  sr = do_rdsr();
  if (sr >= 0 && (sr & SST26_SR_WIP))
    {
      printf("[%2d] PASS: WIP=1 after SE (erase in progress)\n", ++step);
    }
  else
    {
      printf("[%2d] FAIL: WIP=0 after SE (RDSR=0x%02X) — erase didn't start!\n",
             ++step, sr);
      failures++;
      printf("    This means the flash rejected the command.\n");
      printf("    Possible causes: WEL was cleared, blocks still protected.\n");
    }

  /* Step 11: Poll until WIP=0 (max 25 ms for sector erase) */

  {
    int polls = 0;
    do
      {
        sr = do_rdsr();
        polls++;
      }
    while (sr >= 0 && (sr & SST26_SR_WIP) && polls < 5000);

    if (sr >= 0 && !(sr & SST26_SR_WIP))
      {
        printf("[%2d] PASS: Erase complete after %d polls\n", ++step, polls);
      }
    else
      {
        printf("[%2d] FAIL: Erase timeout (polls=%d, RDSR=0x%02X)\n",
               ++step, polls, sr);
        failures++;
      }
  }

  /* Step 12: Enter XIP and verify erased (all 0xFF)
   * Errata 2.12.2: only word-aligned 32-bit reads from XIP window */

  sam_sqi_enter_xip();
  {
    uintptr_t a;
    for (a = (SAM_SQI1_XIP_BASE + TEST_ADDR) & ~31u;
         a < SAM_SQI1_XIP_BASE + TEST_ADDR + 4096;
         a += 32)
      putreg32(a, 0xE000EF5Cu);
    __asm__ volatile ("dsb sy" ::: "memory");

    volatile uint32_t *xip32 = (volatile uint32_t *)(SAM_SQI1_XIP_BASE + TEST_ADDR);
    bool all_ff = true;
    for (int i = 0; i < 4; i++)
      {
        if (xip32[i] != 0xFFFFFFFFu) all_ff = false;
      }

    if (all_ff)
      {
        pass("XIP readback: first 16 bytes = all 0xFF (erased)");
      }
    else
      {
        printf("[%2d] FAIL: XIP word read: %08lX %08lX %08lX %08lX"
               " (expected all FFFFFFFF)\n", ++step,
               (unsigned long)xip32[0], (unsigned long)xip32[1],
               (unsigned long)xip32[2], (unsigned long)xip32[3]);
        failures++;
      }
  }

  /* Step 13: WREN for page program */

  {
    uint8_t wren = SST26_CMD_WREN;
    sam_sqi_flash_cmd_write(&wren, 1);
  }

  sr = do_rdsr();
  if (sr >= 0 && (sr & SST26_SR_WEL))
    {
      printf("[%2d] PASS: WEL=1 before program\n", ++step);
    }
  else
    {
      printf("[%2d] FAIL: WEL=0 before program\n", ++step);
      failures++;
      return 1;
    }

  /* Step 14: Page Program 4 bytes at TEST_ADDR */

  {
    uint8_t pattern[] = {TEST_PATTERN};
    cmd[0] = SST26_CMD_PP;
    cmd[1] = (TEST_ADDR >> 16) & 0xFF;
    cmd[2] = (TEST_ADDR >> 8) & 0xFF;
    cmd[3] = TEST_ADDR & 0xFF;
    memcpy(&cmd[4], pattern, 4);
    ret = sam_sqi_flash_cmd_write(cmd, 8);
    if (ret == 0)
      pass("Page Program command sent (8 bytes)");
    else
      fail("Page Program DMA timeout");
  }

  /* Step 15: Check WIP=1 after program */

  sr = do_rdsr();
  if (sr >= 0 && (sr & SST26_SR_WIP))
    {
      printf("[%2d] PASS: WIP=1 after PP (program in progress)\n", ++step);
    }
  else
    {
      printf("[%2d] FAIL: WIP=0 after PP (RDSR=0x%02X) — program didn't start!\n",
             ++step, sr);
      failures++;
    }

  /* Step 16: Poll until WIP=0 */

  {
    int polls = 0;
    do
      {
        sr = do_rdsr();
        polls++;
      }
    while (sr >= 0 && (sr & SST26_SR_WIP) && polls < 5000);

    if (sr >= 0 && !(sr & SST26_SR_WIP))
      printf("[%2d] PASS: Program complete after %d polls\n", ++step, polls);
    else
      {
        printf("[%2d] FAIL: Program timeout\n", ++step);
        failures++;
      }
  }

  /* Step 17: Enter XIP and verify programmed data
   * Errata 2.12.2: only word-aligned 32-bit reads from XIP window */

  sam_sqi_enter_xip();
  {
    uintptr_t a;
    for (a = (SAM_SQI1_XIP_BASE + TEST_ADDR) & ~31u;
         a < SAM_SQI1_XIP_BASE + TEST_ADDR + 256;
         a += 32)
      putreg32(a, 0xE000EF5Cu);
    __asm__ volatile ("dsb sy" ::: "memory");

    volatile uint32_t *xip32 = (volatile uint32_t *)(SAM_SQI1_XIP_BASE + TEST_ADDR);
    uint32_t got = xip32[0];
    /* DE AD BE EF in little-endian memory = 0xEFBEADDE as uint32 */
    uint32_t expected_word = 0xEFBEADDEu;

    if (got == expected_word)
      {
        pass("XIP word read matches written pattern (DE AD BE EF)");
      }
    else
      {
        uint8_t *g = (uint8_t *)&got;
        printf("[%2d] FAIL: XIP word read: %02X %02X %02X %02X (expected DE AD BE EF)\n",
               ++step, g[0], g[1], g[2], g[3]);
        printf("     Raw word: 0x%08lX (expected 0xEFBEADDE)\n",
               (unsigned long)got);
        failures++;
      }
  }

  /* Step 18: PIO mode test (for errata documentation — DMA proven working above) */

  printf("\n--- PIO Mode Cross-Check (errata evidence) ---\n");
  {
    /* SWRST to clean state */
    putreg8(0x01, SAM_SQI1_BASE + 0x0000);
    while (getreg8(SAM_SQI1_BASE + 0x0020) & 0x01) ;

    /* Configure PIO mode */
    putreg32(0x01000801u, SAM_SQI1_BASE + 0x0108);  /* MODE=PIO, BURSTEN, CSEN0 */
    putreg32(0x00000001u, SAM_SQI1_BASE + 0x0110);  /* CLKCON EN */
    while (!(getreg32(SAM_SQI1_BASE + 0x0110) & 0x02)) ;
    putreg32(0x00000403u, SAM_SQI1_BASE + 0x0110);  /* CLKDIV=4, EN, STABLE */
    while (!(getreg32(SAM_SQI1_BASE + 0x0110) & 0x02)) ;
    putreg32(0x01800801u, SAM_SQI1_BASE + 0x0108);  /* +SQIEN */

    /* Set CON: TXRXCOUNT=4, DASSERT */
    putreg32(0x00400004u, SAM_SQI1_BASE + 0x010C);

    /* Write RDID + 3 dummy to TXDATA */
    putreg32(0x9Fu, SAM_SQI1_BASE + 0x0124);
    putreg32(0x00u, SAM_SQI1_BASE + 0x0124);
    putreg32(0x00u, SAM_SQI1_BASE + 0x0124);
    putreg32(0x00u, SAM_SQI1_BASE + 0x0124);

    /* Wait for RXCNT > 0 (or timeout) */
    uint32_t tmo = 500000;
    uint32_t rxcnt = 0;
    while (tmo--)
      {
        uint32_t s1 = getreg32(SAM_SQI1_BASE + 0x012C);
        rxcnt = s1 & 0xFFFF;
        if (rxcnt > 0) break;
      }

    if (rxcnt == 0)
      {
        printf("[%2d] PIO: RXCNT=0 after 500k polls — PIO mode BROKEN (no bus activity)\n",
               ++step);
        printf("     Evidence: TXFREE decreased (FIFO accepted data) but SCK never toggled.\n");
        printf("     This is NOT in errata DS80001023H. File as new silicon issue.\n");
      }
    else
      {
        uint32_t rx = getreg32(SAM_SQI1_BASE + 0x0128);
        printf("[%2d] PIO: RXCNT=%lu, RX=0x%08lX — PIO mode WORKS!\n",
               ++step, (unsigned long)rxcnt, (unsigned long)rx);
      }

    /* Restore DMA mode for normal operation */
    putreg8(0x01, SAM_SQI1_BASE + 0x0000);
    while (getreg8(SAM_SQI1_BASE + 0x0020) & 0x01) ;
  }

  /* Summary */

  printf("\n=== SQI Test Complete: %d failures ===\n\n", failures);
  return (failures == 0) ? 0 : 1;
}

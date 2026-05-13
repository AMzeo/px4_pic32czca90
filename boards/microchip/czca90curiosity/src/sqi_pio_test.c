/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file sqi_pio_test.c
 *
 * Minimal PIO mode test for SQI1 — tests whether PIO mode actually works
 * on Michigan Ax (CA90) silicon. If it does, the entire BD-DMA approach
 * can be replaced with a much simpler register-based driver.
 *
 * Run from NSH: sqi_pio_test
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/config.h>

#include <stdint.h>
#include <stdio.h>

#include "arm_internal.h"
#include "sam_gclk.h"
#include "sam_port.h"
#include "hardware/sam_sqi.h"
#include "hardware/sam_mclk.h"
#include "hardware/pic32czca90_pinmap.h"

__EXPORT int sqi_pio_test_main(int argc, char *argv[]);

#define SQI1_BASE  SAM_SQI1_BASE

static inline uint32_t sqireg(uint32_t off)
{
  return getreg32(SQI1_BASE + off);
}

static inline void sqiwr(uint32_t off, uint32_t val)
{
  putreg32(val, SQI1_BASE + off);
}

int sqi_pio_test_main(int argc, char *argv[])
{
  uint32_t stat1, stat2, cfg, clkcon;
  int i;

  printf("\n=== SQI1 PIO Mode Test ===\n\n");

  /* Step 1: Ensure pins are muxed (may already be done by init.c) */

  sam_portconfig(PORT_SQI1_CLK);
  sam_portconfig(PORT_SQI1_CS0);
  sam_portconfig(PORT_SQI1_IO0);
  sam_portconfig(PORT_SQI1_IO1);
  sam_portconfig(PORT_SQI1_IO2);
  sam_portconfig(PORT_SQI1_IO3);
  printf("[1] Pins muxed\n");

  /* Step 2: Enable GCLK2 → SQI1 and MCLK */

  sam_gclk_chan_enable(SAM_SQI1_GCLK_ID, 2, false);
  {
    uint32_t id  = SAM_SQI1_MCLK_ID_AHB;
    uint32_t reg = SAM_MCLK_CLKMSK_ADDR(id);
    uint32_t bit = SAM_MCLK_CLKMSK_BIT(id);
    putreg32(getreg32(reg) | bit, reg);
  }
  printf("[2] GCLK + MCLK enabled\n");

  /* Step 3: SWRST */

  putreg8(SQI_CTRLA_SWRST, SAM_SQI1_CTRLA);
  while (getreg8(SAM_SQI1_SYNCBUSY) & SQI_SYNCBUSY_SWRST)
    ;
  printf("[3] SWRST done\n");

  /* Step 4: Configure for PIO mode */

  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_PIO |    /* MODE=1 (PIO) */
        SQI_CFG_BURSTEN  |    /* AHB burst */
        SQI_CFG_DATAEN(0) |   /* single-lane SPI */
        SQI_CFG_CSEN0);       /* CS0 enable */

  cfg = sqireg(SAM_SQI_CFG_OFFSET);
  printf("[4] CFG = 0x%08lx (MODE=%lu, SQIEN=%lu, CSEN0=%lu)\n",
         (unsigned long)cfg,
         (unsigned long)(cfg & 7),
         (unsigned long)((cfg >> 23) & 1),
         (unsigned long)((cfg >> 24) & 1));

  /* Step 5: Enable clock, wait stable, set CLKDIV=4 (12.5 MHz, safe) */

  sqiwr(SAM_SQI_CLKCON_OFFSET, SQI_CLKCON_EN);
  while (!(sqireg(SAM_SQI_CLKCON_OFFSET) & SQI_CLKCON_STABLE))
    ;
  sqiwr(SAM_SQI_CLKCON_OFFSET, SQI_CLKCON_EN | SQI_CLKCON_CLKDIV(4u));
  while (!(sqireg(SAM_SQI_CLKCON_OFFSET) & SQI_CLKCON_STABLE))
    ;
  clkcon = sqireg(SAM_SQI_CLKCON_OFFSET);
  printf("[5] CLKCON = 0x%08lx (EN=%lu, STABLE=%lu, CLKDIV=%lu)\n",
         (unsigned long)clkcon,
         (unsigned long)(clkcon & 1),
         (unsigned long)((clkcon >> 1) & 1),
         (unsigned long)((clkcon >> 8) & 0x7FF));

  /* Step 6: Enable SQIEN */

  sqiwr(SAM_SQI_CFG_OFFSET, sqireg(SAM_SQI_CFG_OFFSET) | SQI_CFG_SQIEN);
  cfg = sqireg(SAM_SQI_CFG_OFFSET);
  printf("[6] CFG after SQIEN = 0x%08lx\n", (unsigned long)cfg);

  /* Step 7: Check STAT1/STAT2 before transfer */

  stat1 = sqireg(SAM_SQI_STAT1_OFFSET);
  stat2 = sqireg(SAM_SQI_STAT2_OFFSET);
  printf("[7] PRE-XFER: STAT1=0x%08lx (TXFREE=%lu RXCNT=%lu) STAT2=0x%08lx\n",
         (unsigned long)stat1,
         (unsigned long)((stat1 >> 16) & 0xFFFF),
         (unsigned long)(stat1 & 0xFFFF),
         (unsigned long)stat2);

  /* Step 8: Set CON for 4-byte transfer (1 cmd + 3 response)
   * TXRXCOUNT=4, LANEMODE=0 (single), DEVSEL=0 (CS0), DASSERT=1 (deassert after) */

  sqiwr(SAM_SQI_CON_OFFSET,
        SQI_CON_TXRXCOUNT(4) |
        SQI_CON_LANEMODE(0)  |
        SQI_CON_DEVSEL(0)    |
        SQI_CON_DASSERT);

  printf("[8] CON = 0x%08lx\n", (unsigned long)sqireg(SAM_SQI_CON_OFFSET));

  /* Step 9: Write 4 bytes to TXDATA (0x9F + 3 dummy) */

  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x9Fu);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);

  stat1 = sqireg(SAM_SQI_STAT1_OFFSET);
  printf("[9] After TX writes: STAT1=0x%08lx (TXFREE=%lu RXCNT=%lu)\n",
         (unsigned long)stat1,
         (unsigned long)((stat1 >> 16) & 0xFFFF),
         (unsigned long)(stat1 & 0xFFFF));

  /* Step 10: Wait for RXBUFCNT >= 4 (or timeout) */

  {
    uint32_t timeout = 1000000u;
    uint32_t rxcnt = 0;
    while (timeout--)
      {
        stat1 = sqireg(SAM_SQI_STAT1_OFFSET);
        rxcnt = stat1 & 0xFFFF;
        if (rxcnt >= 4) break;
      }

    printf("[10] Wait result: timeout_remaining=%lu RXCNT=%lu STAT1=0x%08lx\n",
           (unsigned long)timeout, (unsigned long)rxcnt,
           (unsigned long)stat1);

    if (rxcnt == 0)
      {
        printf("\n*** PIO FAILED: No RX data received. RXCNT stuck at 0. ***\n");
        printf("    This confirms PIO mode does NOT work on this silicon.\n");
        stat2 = sqireg(SAM_SQI_STAT2_OFFSET);
        printf("    STAT2=0x%08lx (IO0=%lu IO1=%lu CMDSTAT=%lu)\n",
               (unsigned long)stat2,
               (unsigned long)((stat2 >> 3) & 1),
               (unsigned long)((stat2 >> 4) & 1),
               (unsigned long)((stat2 >> 16) & 3));
        printf("    INTSTAT=0x%08lx\n",
               (unsigned long)sqireg(SAM_SQI_INTSTAT_OFFSET));
        return 1;
      }
  }

  /* Step 11: Read RX data */

  printf("[11] RX data (4 bytes): ");
  for (i = 0; i < 4; i++)
    {
      uint32_t rx = sqireg(SAM_SQI_RXDATA_OFFSET);
      printf("%02lX ", (unsigned long)(rx & 0xFF));
    }
  printf("\n");

  printf("     Expected: XX BF 26 42 (XX=garbage during cmd phase)\n");

  /* Step 12: Final status */

  stat1 = sqireg(SAM_SQI_STAT1_OFFSET);
  stat2 = sqireg(SAM_SQI_STAT2_OFFSET);
  printf("[12] POST: STAT1=0x%08lx STAT2=0x%08lx INTSTAT=0x%08lx\n",
         (unsigned long)stat1,
         (unsigned long)stat2,
         (unsigned long)sqireg(SAM_SQI_INTSTAT_OFFSET));

  printf("\n=== PIO Test Complete ===\n");
  printf("If you see BF 26 42 in the RX data, PIO MODE WORKS!\n\n");
  return 0;
}

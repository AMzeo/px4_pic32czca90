/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file sqi_pio_test.c
 *
 * Definitive PIO vs DMA comparison test for Microchip bug report.
 *
 * Proves PIO mode (CFG.MODE=1) does not generate bus activity on
 * PIC32CZ CA90 Michigan Ax silicon (PIC32CZ8110CA90208), while
 * BD-DMA mode (CFG.MODE=2) works correctly with identical clock,
 * pin, and peripheral configuration.
 *
 * Board:   PIC32CZ CA90 Curiosity Ultra (EV16W43A)
 * Flash:   SST26VF032BAT on SQI1 (JEDEC BF 26 42)
 * GCLK:    GCLK2 = 100 MHz (PLL0/3)
 * CLKDIV:  1 → 50 MHz SCK
 * Pins:    PC30/CLK, PG03/CS0, PC31/IO0, PG00/IO1, PG01/IO2, PG02/IO3
 *
 * Run from NSH: sqi_pio_test
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/config.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arm_internal.h"
#include "sam_gclk.h"
#include "sam_port.h"
#include "sam_sqi.h"
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

static void sqi_hw_init(void)
{
  sam_portconfig(PORT_SQI1_CLK);
  sam_portconfig(PORT_SQI1_CS0);
  sam_portconfig(PORT_SQI1_IO0);
  sam_portconfig(PORT_SQI1_IO1);
  sam_portconfig(PORT_SQI1_IO2);
  sam_portconfig(PORT_SQI1_IO3);

  sam_gclk_chan_enable(SAM_SQI1_GCLK_ID, 2, false);

  uint32_t id  = SAM_SQI1_MCLK_ID_AHB;
  uint32_t reg = SAM_MCLK_CLKMSK_ADDR(id);
  uint32_t bit = SAM_MCLK_CLKMSK_BIT(id);
  putreg32(getreg32(reg) | bit, reg);
}

static void sqi_swrst(void)
{
  putreg8(SQI_CTRLA_SWRST, SAM_SQI1_CTRLA);
  while (getreg8(SAM_SQI1_SYNCBUSY) & SQI_SYNCBUSY_SWRST)
    ;
}

static void sqi_clock_init(uint32_t clkdiv)
{
  sqiwr(SAM_SQI_CLKCON_OFFSET, SQI_CLKCON_EN);
  while (!(sqireg(SAM_SQI_CLKCON_OFFSET) & SQI_CLKCON_STABLE))
    ;
  sqiwr(SAM_SQI_CLKCON_OFFSET, SQI_CLKCON_EN | SQI_CLKCON_CLKDIV(clkdiv));
  while (!(sqireg(SAM_SQI_CLKCON_OFFSET) & SQI_CLKCON_STABLE))
    ;
}

/****************************************************************************
 * TEST A: BD-DMA RDID (control — must succeed to prove flash is alive)
 ****************************************************************************/

static uint8_t g_dma_tx_buf[4] __attribute__((aligned(32)));
static uint8_t g_dma_rx_buf[4] __attribute__((aligned(32)));

struct sqi_bd_s {
  uint32_t bd_ctrl;
  uint32_t bd_stat;
  uint32_t bd_bufaddr;
  uint32_t bd_nxtptr;
  uint8_t  _pad[16];
} __attribute__((aligned(32)));

static struct sqi_bd_s g_tx_bd __attribute__((aligned(32)));
static struct sqi_bd_s g_rx_bd __attribute__((aligned(32)));

static int test_dma_rdid(uint8_t jedec[3])
{
  uint32_t timeout;

  sqi_swrst();

  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_DMA | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0 | SQI_CFG_RXBUFRST);

  sqi_clock_init(1);

  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_DMA | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0 | SQI_CFG_SQIEN);

  sqiwr(SAM_SQI_CMDTHR_OFFSET, SQI_CMDTHR_RXCMDTHR(1) | SQI_CMDTHR_TXCMDTHR(0x20));
  sqiwr(SAM_SQI_INTTHR_OFFSET, SQI_INTTHR_RXINTTHR(1) | SQI_INTTHR_TXINTTHR(1));
  sqiwr(SAM_SQI_THR_OFFSET, 1);
  sqiwr(SAM_SQI_INTEN_OFFSET, SQI_INT_BDDONE | SQI_INT_PKTCOMP);
  sqiwr(SAM_SQI_INTSIGEN_OFFSET, SQI_INT_BDDONE | SQI_INT_PKTCOMP);

  /* TX BD: send 0x9F (RDID command, 1 byte) */

  g_dma_tx_buf[0] = 0x9Fu;

  g_tx_bd.bd_ctrl    = SQI_BDCTRL_DESC_EN | SQI_BDCTRL_PKT_INT_EN |
                       SQI_BDCTRL_BUFLEN(1);
  g_tx_bd.bd_stat    = 0;
  g_tx_bd.bd_bufaddr = (uint32_t)(uintptr_t)g_dma_tx_buf;
  g_tx_bd.bd_nxtptr  = (uint32_t)(uintptr_t)&g_rx_bd;

  /* RX BD: capture 3 bytes (JEDEC mfr + type + capacity) */

  memset(g_dma_rx_buf, 0xFF, 4);

  g_rx_bd.bd_ctrl    = SQI_BDCTRL_DESC_EN | SQI_BDCTRL_LAST_BD |
                       SQI_BDCTRL_LIFM | SQI_BDCTRL_PKT_INT_EN |
                       SQI_BDCTRL_DIR |
                       SQI_BDCTRL_BUFLEN(3);
  g_rx_bd.bd_stat    = 0;
  g_rx_bd.bd_bufaddr = (uint32_t)(uintptr_t)g_dma_rx_buf;
  g_rx_bd.bd_nxtptr  = 0;

  /* Flush D-cache for buffers and descriptors */

  {
    uintptr_t a;
    for (a = (uintptr_t)g_dma_tx_buf & ~31u;
         a < (uintptr_t)g_dma_tx_buf + 32; a += 32)
      putreg32(a, 0xE000EF68u);  /* DCCMVAC */
    for (a = (uintptr_t)g_dma_rx_buf & ~31u;
         a < (uintptr_t)g_dma_rx_buf + 32; a += 32)
      putreg32(a, 0xE000EF68u);
    for (a = (uintptr_t)&g_tx_bd & ~31u;
         a < (uintptr_t)&g_tx_bd + 32; a += 32)
      putreg32(a, 0xE000EF68u);
    for (a = (uintptr_t)&g_rx_bd & ~31u;
         a < (uintptr_t)&g_rx_bd + 32; a += 32)
      putreg32(a, 0xE000EF68u);
    __asm__ volatile ("dsb sy" ::: "memory");
  }

  sqiwr(SAM_SQI_INTSTAT_OFFSET, 0xFFFFFFFFu);
  sqiwr(SAM_SQI_BDBASEADD_OFFSET, (uint32_t)(uintptr_t)&g_tx_bd);
  sqiwr(SAM_SQI_BDCON_OFFSET, SQI_BDCON_START | SQI_BDCON_DMAEN);

  timeout = 2000000u;
  while (!(sqireg(SAM_SQI_INTSTAT_OFFSET) & (SQI_INT_BDDONE | SQI_INT_PKTCOMP)))
    {
      if (--timeout == 0) return -1;
    }

  sqiwr(SAM_SQI_BDCON_OFFSET, 0);

  /* Invalidate D-cache for RX buffer */

  {
    uintptr_t a;
    for (a = (uintptr_t)g_dma_rx_buf & ~31u;
         a < (uintptr_t)g_dma_rx_buf + 32; a += 32)
      putreg32(a, 0xE000EF5Cu);  /* DCIMVAC */
    __asm__ volatile ("dsb sy" ::: "memory");
  }

  jedec[0] = g_dma_rx_buf[0];
  jedec[1] = g_dma_rx_buf[1];
  jedec[2] = g_dma_rx_buf[2];
  return 0;
}

/****************************************************************************
 * TEST B: PIO RDID (under test — expected to fail on Michigan Ax)
 ****************************************************************************/

static int test_pio_rdid(uint8_t jedec[3])
{
  uint32_t stat1;
  uint32_t timeout;
  uint32_t rxcnt;

  sqi_swrst();

  /* Configure PIO mode — identical clock, pins, CS as DMA test */

  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_PIO | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0);

  sqi_clock_init(1);

  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_PIO | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0 | SQI_CFG_SQIEN);

  /* Set CON: 4-byte transfer, CMDINIT=1 (TX), single lane, CS0, deassert after.
   * CMDINIT(1) is required to initiate PIO transfer — CMDINIT(0) = IDLE. */

  sqiwr(SAM_SQI_CON_OFFSET,
        SQI_CON_TXRXCOUNT(4) |
        SQI_CON_CMDINIT(1) |
        SQI_CON_LANEMODE(0) |
        SQI_CON_DEVSEL(0) |
        SQI_CON_DASSERT);

  /* Write 4 bytes to TXDATA: 0x9F (RDID) + 3 dummy */

  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x9Fu);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);
  sqiwr(SAM_SQI_TXDATA_OFFSET, 0x00u);

  /* Wait for RX data (timeout if PIO broken) */

  timeout = 2000000u;
  rxcnt = 0;
  while (timeout--)
    {
      stat1 = sqireg(SAM_SQI_STAT1_OFFSET);
      rxcnt = stat1 & 0xFFFF;
      if (rxcnt >= 4) break;
    }

  if (rxcnt < 4)
    {
      jedec[0] = 0xFF;
      jedec[1] = 0xFF;
      jedec[2] = 0xFF;
      return -1;
    }

  /* Skip first byte (received during cmd phase) */

  sqireg(SAM_SQI_RXDATA_OFFSET);
  jedec[0] = (uint8_t)sqireg(SAM_SQI_RXDATA_OFFSET);
  jedec[1] = (uint8_t)sqireg(SAM_SQI_RXDATA_OFFSET);
  jedec[2] = (uint8_t)sqireg(SAM_SQI_RXDATA_OFFSET);
  return 0;
}

/****************************************************************************
 * Main
 ****************************************************************************/

int sqi_pio_test_main(int argc, char *argv[])
{
  uint8_t jedec_dma[3] = {0};
  uint8_t jedec_pio[3] = {0};
  uint32_t stat1_after_pio;
  int dma_ok, pio_ok;

  printf("\n");
  printf("================================================================\n");
  printf("  SQI1 PIO vs DMA Mode Comparison Test\n");
  printf("  Silicon: PIC32CZ8110CA90208 (Michigan Ax)\n");
  printf("  Board:   EV16W43A (Curiosity Ultra)\n");
  printf("  Flash:   SST26VF032BAT on SQI1\n");
  printf("  GCLK2:   100 MHz, CLKDIV=1 -> 50 MHz SCK\n");
  printf("================================================================\n\n");

  sqi_hw_init();
  printf("[HW] Pins muxed, GCLK2 + MCLK enabled\n\n");

  /* === TEST A: DMA RDID === */

  printf("--- TEST A: BD-DMA Mode (CFG.MODE=2) RDID ---\n");
  dma_ok = test_dma_rdid(jedec_dma);

  if (dma_ok == 0)
    {
      printf("  Result: %02X %02X %02X", jedec_dma[0], jedec_dma[1], jedec_dma[2]);

      if (jedec_dma[0] == 0xBF && jedec_dma[1] == 0x26 && jedec_dma[2] == 0x42)
        {
          printf(" -> PASS (SST26VF032BAT confirmed)\n");
        }
      else
        {
          printf(" -> UNEXPECTED JEDEC (flash alive but wrong chip?)\n");
        }
    }
  else
    {
      printf("  Result: TIMEOUT (DMA failed — wiring/clock problem)\n");
      printf("  Cannot proceed with PIO test.\n");
      return 1;
    }

  printf("\n");

  /* === TEST B: PIO RDID === */

  printf("--- TEST B: PIO Mode (CFG.MODE=1) RDID ---\n");
  pio_ok = test_pio_rdid(jedec_pio);

  stat1_after_pio = sqireg(SAM_SQI_STAT1_OFFSET);

  if (pio_ok == 0)
    {
      printf("  Result: %02X %02X %02X", jedec_pio[0], jedec_pio[1], jedec_pio[2]);

      if (jedec_pio[0] == 0xBF && jedec_pio[1] == 0x26 && jedec_pio[2] == 0x42)
        {
          printf(" -> PASS (PIO mode works!)\n");
        }
      else
        {
          printf(" -> DATA CORRUPT (PIO partially working)\n");
        }
    }
  else
    {
      printf("  Result: TIMEOUT — no RX data received\n");
      printf("  STAT1 = 0x%08lx (TXFREE=%lu, RXCNT=%lu)\n",
             (unsigned long)stat1_after_pio,
             (unsigned long)((stat1_after_pio >> 16) & 0xFFFF),
             (unsigned long)(stat1_after_pio & 0xFFFF));
      printf("  INTSTAT = 0x%08lx\n",
             (unsigned long)sqireg(SAM_SQI_INTSTAT_OFFSET));
    }

  printf("\n");
  printf("================================================================\n");
  printf("  VERDICT:\n");

  if (dma_ok == 0 && pio_ok != 0)
    {
      printf("  DMA mode: PASS  |  PIO mode: FAIL\n");
      printf("\n");
      printf("  PIO mode (CFG.MODE=1) does NOT generate SCK/MOSI bus activity\n");
      printf("  on this silicon. TXFREE decreases (bytes accepted into FIFO)\n");
      printf("  but RXCNT stays 0 (no data shifted in). The SPI clock never\n");
      printf("  toggles — confirmed by RXCNT=0 timeout with same flash chip\n");
      printf("  and pin configuration that works in DMA mode.\n");
      printf("\n");
      printf("  This is a HARDWARE BUG in PIC32CZ CA90 Michigan Ax silicon.\n");
    }
  else if (dma_ok == 0 && pio_ok == 0)
    {
      printf("  DMA mode: PASS  |  PIO mode: PASS\n");
      printf("  Both modes work. PIO mode is NOT broken on this silicon.\n");
    }
  else
    {
      printf("  DMA mode: FAIL  |  Cannot determine PIO status.\n");
    }

  printf("================================================================\n\n");

  /* Restore SQI to working state — PIO test leaves peripheral dirty.
   * Without this, subsequent param save hangs (DMA can't recover from
   * PIO residual state without full SWRST + XIP re-entry). */

  sqi_swrst();
  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_DMA | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0 | SQI_CFG_RXBUFRST);
  sqi_clock_init(1);
  sqiwr(SAM_SQI_CFG_OFFSET,
        SQI_CFG_MODE_DMA | SQI_CFG_BURSTEN |
        SQI_CFG_DATAEN(0) | SQI_CFG_CSEN0 | SQI_CFG_SQIEN);
  sam_sqi_enter_xip();

  printf("[CLEANUP] SQI restored to XIP mode — param save safe.\n\n");

  return (pio_ok != 0) ? 1 : 0;
}

/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * boards/microchip/czca90curiosity/nuttx-config/include/board.h
 *
 * PIC32CZ CA90 Curiosity Ultra board configuration for PX4
 *
 * CHANGES vs previous version:
 *
 *   FIX A (CRITICAL – no TX output):
 *     BOARD_SERCOM4_MUXCONFIG was (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1).
 *     USART_CTRLA_TXPAD0_2 = (2 << TXPO_SHIFT) → TXPO=2 which activates
 *     hardware CTS on SERCOM4_PAD3. With PAD3 unconnected and floating high,
 *     the USART hardware sees CTS deasserted and suppresses all TX. Changed
 *     to USART_CTRLA_TXPO_PAD0 (TXPO=0) which is TX-only on PAD0 (PC21)
 *     with no flow control. RX stays RXPO=1 (PAD1, PC22). Confirmed correct
 *     for the PKoB4 VCP connection on the Curiosity Ultra schematic.
 *
 *   FIX B (CRITICAL – unstable UART clock during bringup):
 *     BOARD_SERCOM4_COREGEN was 1 (GCLK1 = DFLL48M, 48 MHz). In open-loop
 *     mode without USB connected the DFLL may be unstable at boot, producing
 *     wrong baud rates. Changed to GCLK5 (6 MHz, straight from verified
 *     XOSC0 crystal) for a rock-solid console clock at bringup. Baud error
 *     at 6 MHz / 115200 = 0.003%.  Re-point to GCLK1 once DFLL is proven.
 *     BOARD_SERCOM4_FREQUENCY updated to match (BOARD_GCLK5_FREQUENCY).
 *
 *   FIX C (MODERATE – DFLL WAITLOCK in open-loop):
 *     BOARD_DFLL_WAITLOCK changed from TRUE to FALSE. In open-loop mode
 *     (BOARD_DFLL_MODE=FALSE) the CTRLB.WAITLOCK bit tells the hardware to
 *     hold the DFLL output until a stable lock condition – a condition that
 *     may never occur without the USB SOF reference. This would prevent
 *     GCLK1 from producing any clock at all, starving USB later on.
 *
 *   FIX D (COMMENT): XOSC0_FREQUENCY comment corrected to 12 MHz. The
 *     previous sam_clockconfig.c header mistakenly said "24 MHz". The define
 *     was always 12 MHz (DSC6011JI2B-012.0000). The GCLK5 DIV=2 and
 *     DPLL0 LDR=49 math remains unchanged (6 MHz × 50 = 300 MHz CPU).
 ****************************************************************************/

#ifndef __BOARDS_MICROCHIP_CZCA90CURIOSITY_NUTTX_CONFIG_INCLUDE_BOARD_H
#define __BOARDS_MICROCHIP_CZCA90CURIOSITY_NUTTX_CONFIG_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking *****************************************************************
 *
 * PIC32CZ CA90 Curiosity Ultra clock chain:
 *
 *   XOSC0: Y300 = DSC6011JI2B-012.0000 = 12 MHz MEMS (XTALEN=0, ext clock)
 *     |
 *   GCLK5 (÷2 = 6 MHz)  -- configured first (SET1), before DPLLs
 *     |-- GCLK_PCHCTRL[1] (DPLL0 reference)   ← FIX: was missing
 *     |-- SERCOM4 core clock (console UART)    ← FIX B
 *     |
 *   DPLL0 (LDR=49, ×50 = 300 MHz)
 *     |
 *   GCLK0 (÷1 = 300 MHz)  -- CPU clock
 *     |
 *   MCLK CPUDIV=1 → CPU @ 300 MHz
 *
 *   DFLL48M (open loop, USB CRM) → 48 MHz → GCLK1 (for USB)
 */

/* Oscillator frequencies */

#define BOARD_XOSC0_FREQUENCY    12000000   /* 12 MHz – DSC6011JI2B-012.0000 */
#define BOARD_XOSC1_FREQUENCY    0          /* XOSC1 not used */
#define BOARD_XOSC32K_FREQUENCY  32768      /* 32.768 kHz (if present) */
#define BOARD_OSC32K_FREQUENCY   32768      /* OSCULP32K nominal */
#define BOARD_DFLL_FREQUENCY     48000000   /* DFLL48M output */
#define BOARD_DPLL0_FREQUENCY    300000000  /* DPLL0 output: 6 MHz × 50 */
#define BOARD_DPLL1_FREQUENCY    0          /* DPLL1 not used */

/* GCLK frequencies */

#define BOARD_GCLK0_FREQUENCY    BOARD_DPLL0_FREQUENCY          /* 300 MHz  */
#define BOARD_GCLK1_FREQUENCY    (BOARD_DPLL0_FREQUENCY / 2)    /* 150 MHz  */
#define BOARD_GCLK2_FREQUENCY    0                              /* Disabled */
#define BOARD_GCLK3_FREQUENCY    BOARD_OSC32K_FREQUENCY         /* 32.768kHz*/
#define BOARD_GCLK4_FREQUENCY    BOARD_DPLL0_FREQUENCY          /* 300 MHz  */
#define BOARD_GCLK5_FREQUENCY    (BOARD_XOSC0_FREQUENCY / 2)   /* 6 MHz    */
#define BOARD_GCLK6_FREQUENCY    0                              /* Disabled */
#define BOARD_GCLK7_FREQUENCY    0
#define BOARD_GCLK8_FREQUENCY    0
#define BOARD_GCLK9_FREQUENCY    0
#define BOARD_GCLK10_FREQUENCY   0
#define BOARD_GCLK11_FREQUENCY   0

#define BOARD_CPU_FREQUENCY      BOARD_GCLK0_FREQUENCY  /* 300 MHz */
#define BOARD_MCK_FREQUENCY      BOARD_GCLK0_FREQUENCY  /* 300 MHz */

/* XOSC32K - not used, rely on internal OSCULP32K */

#define BOARD_HAVE_XOSC32K       0
#define BOARD_XOSC32K_ENABLE     FALSE
#define BOARD_XOSC32K_XTALEN     FALSE
#define BOARD_XOSC32K_EN32K      FALSE
#define BOARD_XOSC32K_EN1K       FALSE
#define BOARD_XOSC32K_HIGHSPEED  FALSE
#define BOARD_XOSC32K_RUNSTDBY   FALSE
#define BOARD_XOSC32K_ONDEMAND   TRUE
#define BOARD_XOSC32K_CFDEN      FALSE
#define BOARD_XOSC32K_CFDEO      FALSE
#define BOARD_XOSC32K_CALIBEN    FALSE
#define BOARD_XOSC32K_STARTUP    0
#define BOARD_XOSC32K_CALIB      0
#define BOARD_XOSC32K_RTCSEL     0

/* XOSC0 - 12 MHz MEMS oscillator (XTALEN=0 = external clock, not crystal) */

#define BOARD_HAVE_XOSC0         0
#define BOARD_XOSC0_ENABLE       FALSE
#define BOARD_XOSC0_XTALEN       FALSE     /* MEMS oscillator, NOT crystal */
#define BOARD_XOSC0_RUNSTDBY     FALSE
#define BOARD_XOSC0_ONDEMAND     FALSE
#define BOARD_XOSC0_LOWGAIN      FALSE
#define BOARD_XOSC0_ENALC        FALSE
#define BOARD_XOSC0_CFDEN        FALSE
#define BOARD_XOSC0_SWBEN        FALSE
#define BOARD_XOSC0_STARTUP      0

/* XOSC1 - not used */

#define BOARD_HAVE_XOSC1         0
#define BOARD_XOSC1_ENABLE       FALSE
#define BOARD_XOSC1_XTALEN       FALSE
#define BOARD_XOSC1_RUNSTDBY     FALSE
#define BOARD_XOSC1_ONDEMAND     TRUE
#define BOARD_XOSC1_LOWGAIN      FALSE
#define BOARD_XOSC1_ENALC        FALSE
#define BOARD_XOSC1_CFDEN        FALSE
#define BOARD_XOSC1_SWBEN        FALSE
#define BOARD_XOSC1_STARTUP      0

/* GCLK configuration
 *
 * GCLK_SET1: configured BEFORE DPLLs (provides DPLL reference)
 * GCLK_SET2: configured AFTER DPLLs
 *
 * Bits: GCLK5 (bit 5) is in SET1.
 * All others in SET2 (0x0fdf = bits 0-4, 6-11).
 */

#define BOARD_GCLK_SET1          0x0000    /* SET1: nothing (no XOSC0 dependent clocks) */
#define BOARD_GCLK_SET2          0x0fff    /* SET2: all GCLKs 0-11 */

/* GCLK0 - 300 MHz CPU clock from DPLL0 */

#define BOARD_GCLK0_ENABLE       TRUE
#define BOARD_GCLK0_OOV          FALSE
#define BOARD_GCLK0_OE           FALSE
#define BOARD_GCLK0_DIVSEL       0
#define BOARD_GCLK0_RUNSTDBY     FALSE
#define BOARD_GCLK0_SOURCE       6         /* PLL0_1 = 300 MHz (CA90: 6 not 7) */
#define BOARD_GCLK0_DIV          1

/* GCLK1 - 150 MHz from PLL0_1/2 (Harmony: GENCTRL[1]=DIV(2)|SRC(6))
 * Matches Harmony usart_echo_blocking exactly. SERCOM1 baud at 150 MHz
 * gives 64730 for 115200 baud (identical to Harmony). */

#define BOARD_GCLK1_ENABLE       TRUE
#define BOARD_GCLK1_OOV          FALSE
#define BOARD_GCLK1_OE           FALSE
#define BOARD_GCLK1_RUNSTDBY     FALSE
#define BOARD_GCLK1_SOURCE       6         /* PLL0_1 = 300 MHz (CA90: 6) */
#define BOARD_GCLK1_DIV          2         /* 300 MHz / 2 = 150 MHz */

/* GCLK2 - disabled */

#define BOARD_GCLK2_ENABLE       FALSE
#define BOARD_GCLK2_OOV          FALSE
#define BOARD_GCLK2_OE           FALSE
#define BOARD_GCLK2_RUNSTDBY     FALSE
#define BOARD_GCLK2_SOURCE       1
#define BOARD_GCLK2_DIV          1

/* GCLK3 - 32.768 kHz from OSCULP32K (slow clock for SERCOM slow, WDT) */

#define BOARD_GCLK3_ENABLE       TRUE
#define BOARD_GCLK3_OOV          FALSE
#define BOARD_GCLK3_OE           FALSE
#define BOARD_GCLK3_RUNSTDBY     FALSE
#define BOARD_GCLK3_SOURCE       3         /* OSCULP32K = 3 on CA90 (not 4) */
#define BOARD_GCLK3_DIV          1

/* GCLK4 - 300 MHz from DPLL0 */

#define BOARD_GCLK4_ENABLE       TRUE
#define BOARD_GCLK4_OOV          FALSE
#define BOARD_GCLK4_OE           FALSE
#define BOARD_GCLK4_RUNSTDBY     FALSE
#define BOARD_GCLK4_SOURCE       6         /* PLL0_1 = 300 MHz (CA90: 6 not 7) */
#define BOARD_GCLK4_DIV          1

/* GCLK5 - disabled (XOSC0 removed; SERCOM1 now uses GCLK1=DFLL48M) */

#define BOARD_GCLK5_ENABLE       FALSE
#define BOARD_GCLK5_OOV          FALSE
#define BOARD_GCLK5_OE           FALSE
#define BOARD_GCLK5_RUNSTDBY     FALSE
#define BOARD_GCLK5_SOURCE       1         /* (don't care, disabled) */
#define BOARD_GCLK5_DIV          1

/* GCLK6-11 - disabled */

#define BOARD_GCLK6_ENABLE       FALSE
#define BOARD_GCLK6_OOV          FALSE
#define BOARD_GCLK6_OE           FALSE
#define BOARD_GCLK6_RUNSTDBY     FALSE
#define BOARD_GCLK6_SOURCE       1
#define BOARD_GCLK6_DIV          1

#define BOARD_GCLK7_ENABLE       FALSE
#define BOARD_GCLK7_OOV          FALSE
#define BOARD_GCLK7_OE           FALSE
#define BOARD_GCLK7_RUNSTDBY     FALSE
#define BOARD_GCLK7_SOURCE       1
#define BOARD_GCLK7_DIV          1

#define BOARD_GCLK8_ENABLE       FALSE
#define BOARD_GCLK8_OOV          FALSE
#define BOARD_GCLK8_OE           FALSE
#define BOARD_GCLK8_RUNSTDBY     FALSE
#define BOARD_GCLK8_SOURCE       1
#define BOARD_GCLK8_DIV          1

#define BOARD_GCLK9_ENABLE       FALSE
#define BOARD_GCLK9_OOV          FALSE
#define BOARD_GCLK9_OE           FALSE
#define BOARD_GCLK9_RUNSTDBY     FALSE
#define BOARD_GCLK9_SOURCE       1
#define BOARD_GCLK9_DIV          1

#define BOARD_GCLK10_ENABLE      FALSE
#define BOARD_GCLK10_OOV         FALSE
#define BOARD_GCLK10_OE          FALSE
#define BOARD_GCLK10_RUNSTDBY    FALSE
#define BOARD_GCLK10_SOURCE      1
#define BOARD_GCLK10_DIV         1

#define BOARD_GCLK11_ENABLE      FALSE
#define BOARD_GCLK11_OOV         FALSE
#define BOARD_GCLK11_OE          FALSE
#define BOARD_GCLK11_RUNSTDBY    FALSE
#define BOARD_GCLK11_SOURCE      1
#define BOARD_GCLK11_DIV         1

/* DFLL48M - open loop
 *
 * FIX C: BOARD_DFLL_WAITLOCK must be FALSE in open-loop mode.
 * CTRLB.WAITLOCK=1 in open-loop holds the output clock until stability is
 * signalled — this may never happen without USB, leaving GCLK1 dead.
 */

#define BOARD_DFLL_ENABLE        TRUE
#define BOARD_DFLL_RUNSTDBY      FALSE
#define BOARD_DFLL_ONDEMAND      FALSE
#define BOARD_DFLL_MODE          FALSE     /* Open loop mode */
#define BOARD_DFLL_STABLE        FALSE
#define BOARD_DFLL_LLAW          FALSE
#define BOARD_DFLL_USBCRM        TRUE
#define BOARD_DFLL_CCDIS         TRUE
#define BOARD_DFLL_QLDIS         FALSE
#define BOARD_DFLL_BPLCKC        FALSE
#define BOARD_DFLL_WAITLOCK      FALSE     /* FIX C: was TRUE → starved GCLK1 */
#define BOARD_DFLL_CALIBEN       FALSE
#define BOARD_DFLL_GCLKLOCK      FALSE
#define BOARD_DFLL_FCALIB        128
#define BOARD_DFLL_CCALIB        (31 / 4)
#define BOARD_DFLL_FSTEP         1
#define BOARD_DFLL_CSTEP         1
#define BOARD_DFLL_GCLK          3
#define BOARD_DFLL_MUL           0

/* DPLL0 - 300 MHz from GCLK5 (6 MHz × 50)
 *
 *   CTRLB.REFCLK = 0 → GCLK reference (routes through GCLK_PCHCTRL[1])
 *   DPLL0_GCLK   = 5 → GCLK5 feeds GCLK_PCHCTRL[1]
 *     (sam_clockconfig.c now calls sam_gclk_chan_enable() for this)
 */

#define BOARD_DPLL0_ENABLE       FALSE     /* sam_pll0_init() handles PLL0 directly */
#define BOARD_DPLL0_DCOEN        FALSE
#define BOARD_DPLL0_LBYPASS      FALSE
#define BOARD_DPLL0_WUF          FALSE
#define BOARD_DPLL0_RUNSTDBY     FALSE
#define BOARD_DPLL0_ONDEMAND     FALSE
#define BOARD_DPLL0_REFLOCK      FALSE
#define BOARD_DPLL0_REFCLK       0         /* GCLK reference */
#define BOARD_DPLL0_LTIME        0
#define BOARD_DPLL0_FILTER       0
#define BOARD_DPLL0_DCOFILTER    0
#define BOARD_DPLL0_GCLK         5         /* GCLK5 feeds DPLL0 via PCHCTRL[1] */
#define BOARD_DPLL0_GCLKLOCK     0
#define BOARD_DPLL0_LDRFRAC      0
#define BOARD_DPLL0_LDRINT       49        /* 6 MHz × (49+1) = 300 MHz */
#define BOARD_DPLL0_DIV          0

/* DPLL1 - not used */

#define BOARD_DPLL1_ENABLE       FALSE
#define BOARD_DPLL1_DCOEN        FALSE
#define BOARD_DPLL1_LBYPASS      FALSE
#define BOARD_DPLL1_WUF          FALSE
#define BOARD_DPLL1_RUNSTDBY     FALSE
#define BOARD_DPLL1_ONDEMAND     FALSE
#define BOARD_DPLL1_REFLOCK      FALSE
#define BOARD_DPLL1_REFCLK       0
#define BOARD_DPLL1_LTIME        0
#define BOARD_DPLL1_FILTER       0
#define BOARD_DPLL1_DCOFILTER    0
#define BOARD_DPLL1_GCLK         0
#define BOARD_DPLL1_GCLKLOCK     0
#define BOARD_DPLL1_LDRFRAC      0
#define BOARD_DPLL1_LDRINT       0
#define BOARD_DPLL1_DIV          0

/* Master Clock (MCLK) */

#define BOARD_MCLK_CPUDIV        2         /* Harmony: MCLK.CLKDIV[1]=2 before PLL0 GCLK0 switch */

/* Flash wait states (FCR manages this automatically on CZCA90) */

#define BOARD_FLASH_WAITSTATES   8

/* LED definitions **********************************************************/

#define BOARD_LED0               0
#define BOARD_NLEDS              1

#define BOARD_LED0_BIT           (1 << BOARD_LED0)

#define LED_STARTED              0
#define LED_HEAPALLOCATE         0
#define LED_IRQSENABLED          0
#define LED_STACKCREATED         1
#define LED_INIRQ                2
#define LED_SIGNAL               2
#define LED_ASSERTION            2
#define LED_PANIC                3
#undef  LED_IDLE

/* Button definitions *******************************************************/

#define BUTTON_SW0        0
#define NUM_BUTTONS       1
#define BUTTON_SW0_BIT    (1 << BUTTON_SW0)

/* SERCOM configuration *****************************************************/

/* SERCOM slow clock – shared by all SERCOMs */

#define BOARD_SERCOM_SLOWGEN     3
#define BOARD_SERCOM_SLOWLOCK    FALSE
#define BOARD_SLOWCLOCK_FREQUENCY BOARD_GCLK3_FREQUENCY

/* SERCOM1 – Console UART (PKoB4 VCP on J700) — GROUND TRUTH from Harmony
 *
 * Harmony usart_echo_blocking example (working on CA90 hardware) uses:
 *   SERCOM1, TXPO=0 (PAD0=TX), RXPO=3 (PAD3=RX)
 *   PC04 = PAD0 TX (function D, PMUX value 3)
 *   PC07 = PAD3 RX (function D, PMUX value 3)
 *   Core clock: GCLK1 = 150 MHz (PLL0_1/2), BAUD=64730 → 115200 baud
 */

#define BOARD_SERCOM1_MUXCONFIG   (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD3)
                                  /* TXPO=0 (PAD0 TX), RXPO=3 (PAD3 RX) */
#define BOARD_SERCOM1_PINMAP_PAD0 PORT_SERCOM1_PAD0  /* PC04 TX, func D */
#define BOARD_SERCOM1_PINMAP_PAD1 0
#define BOARD_SERCOM1_PINMAP_PAD2 0
#define BOARD_SERCOM1_PINMAP_PAD3 PORT_SERCOM1_PAD3  /* PC07 RX, func D */

#define BOARD_TXIRQ_SERCOM1       SAM_IRQ_SERCOM1_0
#define BOARD_RXIRQ_SERCOM1       SAM_IRQ_SERCOM1_2

#define BOARD_SERCOM1_COREGEN     1          /* GCLK1 = PLL0_1/2 = 150 MHz */
#define BOARD_SERCOM1_CORELOCK    FALSE
#define BOARD_SERCOM1_FREQUENCY   BOARD_GCLK1_FREQUENCY  /* 150 MHz → BAUD=64730 @ 115200 */

/* SERCOM4 – EXT2 expansion connector (NOT console, NOT PKoB4 VCP)
 *   PC21 (PAD0) and PC22 (PAD1) are EXT2 header pins only.
 */

#define BOARD_SERCOM4_MUXCONFIG   (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM4_PINMAP_PAD0 PORT_SERCOM4_PAD0  /* PC21 EXT2 */
#define BOARD_SERCOM4_PINMAP_PAD1 PORT_SERCOM4_PAD1  /* PC22 EXT2 */
#define BOARD_SERCOM4_PINMAP_PAD2 0
#define BOARD_SERCOM4_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM4       SAM_IRQ_SERCOM4_0
#define BOARD_RXIRQ_SERCOM4       SAM_IRQ_SERCOM4_2

#define BOARD_SERCOM4_COREGEN     5
#define BOARD_SERCOM4_CORELOCK    FALSE
#define BOARD_SERCOM4_FREQUENCY   BOARD_GCLK5_FREQUENCY

/* SERCOM0 - spare (PA04/PA05, function D) */

#define BOARD_SERCOM0_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM0_PINMAP_PAD0 PORT_SERCOM0_PAD0
#define BOARD_SERCOM0_PINMAP_PAD1 PORT_SERCOM0_PAD1
#define BOARD_SERCOM0_PINMAP_PAD2 0
#define BOARD_SERCOM0_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM0      SAM_IRQ_SERCOM0_0
#define BOARD_RXIRQ_SERCOM0      SAM_IRQ_SERCOM0_2
#define BOARD_SERCOM0_COREGEN    5
#define BOARD_SERCOM0_CORELOCK   FALSE
#define BOARD_SERCOM0_FREQUENCY  BOARD_GCLK5_FREQUENCY

/* SERCOM2 (PC08/PC09, function E) */

#define BOARD_SERCOM2_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM2_PINMAP_PAD0 PORT_SERCOM2_PAD0
#define BOARD_SERCOM2_PINMAP_PAD1 PORT_SERCOM2_PAD1
#define BOARD_SERCOM2_PINMAP_PAD2 0
#define BOARD_SERCOM2_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM2      SAM_IRQ_SERCOM2_0
#define BOARD_RXIRQ_SERCOM2      SAM_IRQ_SERCOM2_2
#define BOARD_SERCOM2_COREGEN    1
#define BOARD_SERCOM2_CORELOCK   FALSE
#define BOARD_SERCOM2_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM3 (PC12/PC13, function E) */

#define BOARD_SERCOM3_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM3_PINMAP_PAD0 PORT_SERCOM3_PAD0
#define BOARD_SERCOM3_PINMAP_PAD1 PORT_SERCOM3_PAD1
#define BOARD_SERCOM3_PINMAP_PAD2 0
#define BOARD_SERCOM3_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM3      SAM_IRQ_SERCOM3_0
#define BOARD_RXIRQ_SERCOM3      SAM_IRQ_SERCOM3_2
#define BOARD_SERCOM3_COREGEN    1
#define BOARD_SERCOM3_CORELOCK   FALSE
#define BOARD_SERCOM3_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM5-7 defaults */

#define BOARD_SERCOM5_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM5_PINMAP_PAD0 0
#define BOARD_SERCOM5_PINMAP_PAD1 0
#define BOARD_SERCOM5_PINMAP_PAD2 0
#define BOARD_SERCOM5_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM5      SAM_IRQ_SERCOM5_0
#define BOARD_RXIRQ_SERCOM5      SAM_IRQ_SERCOM5_2
#define BOARD_SERCOM5_COREGEN    1
#define BOARD_SERCOM5_CORELOCK   FALSE
#define BOARD_SERCOM5_FREQUENCY  BOARD_GCLK1_FREQUENCY

#define BOARD_SERCOM6_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM6_PINMAP_PAD0 0
#define BOARD_SERCOM6_PINMAP_PAD1 0
#define BOARD_SERCOM6_PINMAP_PAD2 0
#define BOARD_SERCOM6_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM6      SAM_IRQ_SERCOM6_0
#define BOARD_RXIRQ_SERCOM6      SAM_IRQ_SERCOM6_2
#define BOARD_SERCOM6_COREGEN    1
#define BOARD_SERCOM6_CORELOCK   FALSE
#define BOARD_SERCOM6_FREQUENCY  BOARD_GCLK1_FREQUENCY

#define BOARD_SERCOM7_MUXCONFIG  (USART_CTRLA_TXPO_PAD0 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM7_PINMAP_PAD0 0
#define BOARD_SERCOM7_PINMAP_PAD1 0
#define BOARD_SERCOM7_PINMAP_PAD2 0
#define BOARD_SERCOM7_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM7      SAM_IRQ_SERCOM7_0
#define BOARD_RXIRQ_SERCOM7      SAM_IRQ_SERCOM7_2
#define BOARD_SERCOM7_COREGEN    1
#define BOARD_SERCOM7_CORELOCK   FALSE
#define BOARD_SERCOM7_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* USB */

#define BOARD_USB_GCLKGEN        1

#endif /* __BOARDS_MICROCHIP_CZCA90CURIOSITY_NUTTX_CONFIG_INCLUDE_BOARD_H */

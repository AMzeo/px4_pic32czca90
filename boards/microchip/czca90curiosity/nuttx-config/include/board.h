/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * boards/microchip/czca90curiosity/nuttx-config/include/board.h
 *
 * PIC32CZ CA90 Curiosity Ultra board configuration for PX4
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
 *   DFLL48M (48 MHz, open-loop, running from reset — not configured in SW)
 *     |
 *   PLL0: REFSEL=DFLL, REFDIV=12, FBDIV=225, POSTDIV0=3
 *     = 48 MHz / 12 = 4 MHz × 225 = 900 MHz VCO / 3 = 300 MHz
 *     |
 *   GCLK0 (SRC=6=PLL0_1, DIV=1) → 300 MHz   [BOARD_GCLK0_FREQUENCY]
 *   GCLK1 (SRC=6=PLL0_1, DIV=2) → 150 MHz   [BOARD_GCLK1_FREQUENCY]
 *     └─ SERCOM1 core clock → BAUD=64730 → 115200 baud
 *
 *   GCLK3 (SRC=3=OSCULP32K, DIV=1) → 32.768 kHz → SERCOM slow, WDT
 *
 *   XOSC0 (12 MHz MEMS) is on the board but not used (BOARD_HAVE_XOSC0=0).
 *   BOARD_DPLL0_ENABLE=FALSE; PLL0 is initialized by sam_pll0_init().
 */

/* Oscillator frequencies */

#define BOARD_XOSC0_FREQUENCY    12000000   /* 12 MHz – DSC6011JI2B-012.0000 */
#define BOARD_XOSC32K_FREQUENCY  32768      /* 32.768 kHz (if present) */
#define BOARD_OSC32K_FREQUENCY   32768      /* OSCULP32K nominal */
#define BOARD_DFLL_FREQUENCY     48000000   /* DFLL48M output */
#define BOARD_DPLL0_FREQUENCY    300000000  /* PLL0 output: DFLL/12 × 225/3 = 300 MHz */
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

/* CPU frequency = GCLK0 = 300 MHz.
 * MCLK.CLKDIV[1] (offset 0x0010, DFP-verified stride ×4) is PAC write-protected.
 * sam_clockconfig.c writes CPUDIV=2 but PAC silently drops it; CPUDIV stays at
 * reset default 1 (no division).  CPU = GCLK0 = PLL0 = 300 MHz.
 * BOARD_MCLK_CPUDIV=2 is kept so sam_clockconfig executes the write+CKRDY read
 * (providing a clock-domain synchronization barrier before the GCLK0 switch);
 * the write itself has no hardware effect.
 */

#define BOARD_CPU_FREQUENCY      BOARD_DPLL0_FREQUENCY         /* 300 MHz */
#define BOARD_MCK_FREQUENCY      BOARD_DPLL0_FREQUENCY         /* 300 MHz */

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

/* GCLK1 - 150 MHz from PLL0_1/2 (SERCOM1 baud: BAUD=64730 → 115200 baud) */

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

/* GCLK5 - disabled. Was the XOSC0/2 reference for DPLL0 in an earlier design.
 *         Current design uses sam_pll0_init() with PLL0 referencing DFLL directly
 *         (REFSEL=2), so no GCLK is needed as PLL reference.  GCLK_PCHCTRL[1]
 *         (PLL0 reference channel) is therefore never programmed either. */

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

/* DFLL48M - open loop, running from reset, not configured in software.
 * WAITLOCK must be FALSE in open-loop mode: CTRLB.WAITLOCK=1 would hold the
 * output clock until stability is signalled, which cannot happen without USB.
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
#define BOARD_DFLL_WAITLOCK      FALSE
#define BOARD_DFLL_CALIBEN       FALSE
#define BOARD_DFLL_GCLKLOCK      FALSE
#define BOARD_DFLL_FCALIB        128
#define BOARD_DFLL_CCALIB        (31 / 4)
#define BOARD_DFLL_FSTEP         1
#define BOARD_DFLL_CSTEP         1
#define BOARD_DFLL_GCLK          3
#define BOARD_DFLL_MUL           0

/* DPLL0 fields — BOARD_DPLL0_ENABLE=FALSE so these values are unused.
 *   PLL0 is initialised by sam_pll0_init() using DFLL48M directly (REFSEL=2).
 *   BOARD_DPLL0_GCLK=5 / BOARD_DPLL0_LDRINT=49 are legacy from an earlier
 *   design that routed XOSC0→GCLK5→PLL0 and are ignored at runtime.
 */

#define BOARD_DPLL0_ENABLE       FALSE     /* PLL0 init done by sam_pll0_init() */
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

/* Master Clock (MCLK)
 *
 * MCLK.CLKDIV[0] (offset 0x000C, DFP: MCLK_CLKDIV0_REG_OFST) = CPU Clock Divider.
 * Cross-test confirmed: CLKDIV[0]=1 → CPU = GCLK0 = 300 MHz (no division).
 * CLKDIV[1] (offset 0x0010) is a separate domain divider — NOT the CPU divider.
 * Harmony GCLK0_Initialize sets CLKDIV[1]=2; sam_clockconfig follows suit.
 * BOARD_MCLK_CPUDIV=1 → sam_clockconfig writes 1 to CLKDIV[0] (idempotent,
 * already at reset default) and the CKRDY read provides the clock-domain barrier.
 */

#define BOARD_MCLK_CPUDIV        1         /* CLKDIV[0]=1: CPU = GCLK0 = 300 MHz */

/* Flash wait states (FCR manages this automatically on CZCA90) */

#define BOARD_FLASH_WAITSTATES   8

/* LED definitions **********************************************************
 *
 * LED0: PB21, active LOW (yellow) — DS70005522C Table 2-11
 * LED1: PB22, active LOW (yellow) — DS70005522C Table 2-11
 *
 * NuttX auto-LED state machine:
 *   LED_STARTED/HEAPALLOCATE/IRQSENABLED = 0  → no change (LEDs off)
 *   LED_STACKCREATED = 1                       → LED0 on  (system running)
 *   LED_INIRQ/SIGNAL/ASSERTION = 2             → LED1 on  (activity)
 *   LED_PANIC = 3                              → LED0+LED1 blink (fault)
 */

#define BOARD_LED0               0         /* PB21 */
#define BOARD_LED1               1         /* PB22 */
#define BOARD_NLEDS              2

#define BOARD_LED0_BIT           (1 << BOARD_LED0)
#define BOARD_LED1_BIT           (1 << BOARD_LED1)

#define LED_STARTED              0
#define LED_HEAPALLOCATE         0
#define LED_IRQSENABLED          0
#define LED_STACKCREATED         1
#define LED_INIRQ                2
#define LED_SIGNAL               2
#define LED_ASSERTION            2
#define LED_PANIC                3
#undef  LED_IDLE

/* Button definitions *******************************************************
 *
 * SW0: PB24, active LOW (input with pullup) — DS70005522C Table 2-11
 * SW1: PC23, active LOW (input with pullup) — DS70005522C Table 2-11
 */

#define BUTTON_SW0        0         /* PB24 */
#define BUTTON_SW1        1         /* PC23 */
#define NUM_BUTTONS       2
#define BUTTON_SW0_BIT    (1 << BUTTON_SW0)
#define BUTTON_SW1_BIT    (1 << BUTTON_SW1)

/* SERCOM configuration *****************************************************/

/* SERCOM slow clock – shared by all SERCOMs */

#define BOARD_SERCOM_SLOWGEN     3
#define BOARD_SERCOM_SLOWLOCK    FALSE
#define BOARD_SLOWCLOCK_FREQUENCY BOARD_GCLK3_FREQUENCY

/* SERCOM1 – Console UART (PKoB4 VCP on J700)
 *   PC04 = PAD0 TX (function D), PC07 = PAD3 RX (function D)
 *   TXPO=0 (no flow control), RXPO=3
 *   Core clock: GCLK1 = 150 MHz → BAUD=64730 → 115200 baud
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

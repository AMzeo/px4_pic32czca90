/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * boards/arm/pic32czca90/pic32czca90-curiosity/include/board.h
 *
 * PIC32CZ CA90 Curiosity Ultra board configuration (EV16W43A)
 ****************************************************************************/

#ifndef __BOARDS_ARM_PIC32CZCA90_PIC32CZCA90_CURIOSITY_INCLUDE_BOARD_H
#define __BOARDS_ARM_PIC32CZCA90_PIC32CZCA90_CURIOSITY_INCLUDE_BOARD_H

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
 *   XOSC0 (24 MHz MEMS oscillator, XTALEN=0)
 *     |
 *   GCLK5 (÷4 = 6 MHz)  -- configured first, before DPLLs
 *     |
 *   DPLL0 (LDR=49, ×50 = 300 MHz)
 *     |
 *   GCLK0 (÷1 = 300 MHz)  -- CPU clock
 *     |
 *   MCLK CPUDIV=1 → CPU @ 300 MHz
 *
 *   DFLL48M (open loop, USB CRM) → 48 MHz → GCLK1 (for USB, SERCOM)
 */

/* Oscillator frequencies */

#define BOARD_XOSC0_FREQUENCY    12000000   /* 12 MHz MEMS oscillator Y300: DSC6011JI2B-012.0000 */
#define BOARD_XOSC1_FREQUENCY    0          /* XOSC1 not used */
#define BOARD_XOSC32K_FREQUENCY  32768      /* 32.768 KHz (if present) */
#define BOARD_OSC32K_FREQUENCY   32768      /* OSCULP32K nominal */
#define BOARD_DFLL_FREQUENCY     48000000   /* DFLL48M output */
#define BOARD_DPLL0_FREQUENCY    300000000  /* DPLL0 output: 6 MHz × 50 */
#define BOARD_DPLL1_FREQUENCY    0          /* DPLL1 not used */

/* GCLK frequencies */

#define BOARD_GCLK0_FREQUENCY    BOARD_DPLL0_FREQUENCY    /* 300 MHz */
#define BOARD_GCLK1_FREQUENCY    BOARD_DFLL_FREQUENCY     /* 48 MHz */
#define BOARD_GCLK2_FREQUENCY    0                        /* Disabled */
#define BOARD_GCLK3_FREQUENCY    BOARD_OSC32K_FREQUENCY   /* 32.768 KHz */
#define BOARD_GCLK4_FREQUENCY    BOARD_DPLL0_FREQUENCY    /* 300 MHz */
#define BOARD_GCLK5_FREQUENCY    (BOARD_XOSC0_FREQUENCY / 2) /* 6 MHz */
#define BOARD_GCLK6_FREQUENCY    0                        /* Disabled */
#define BOARD_GCLK7_FREQUENCY    0                        /* Disabled */
#define BOARD_GCLK8_FREQUENCY    0                        /* Disabled */
#define BOARD_GCLK9_FREQUENCY    0                        /* Disabled */
#define BOARD_GCLK10_FREQUENCY   0                        /* Disabled */
#define BOARD_GCLK11_FREQUENCY   0                        /* Disabled */

#define BOARD_CPU_FREQUENCY      BOARD_GCLK0_FREQUENCY    /* 300 MHz */
#define BOARD_MCK_FREQUENCY      BOARD_GCLK0_FREQUENCY    /* 300 MHz */

/* XOSC32K - use internal OSCULP32K (no external 32K crystal) */

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

/* XOSC0 - 24 MHz MEMS oscillator (XTALEN=0 = external clock, NOT crystal)
 *
 * The CA90 Curiosity Ultra uses a MEMS oscillator connected to the XOSC0
 * input. Unlike a crystal, this requires XTALEN=0 (external clock mode).
 */

#define BOARD_HAVE_XOSC0         1
#define BOARD_XOSC0_ENABLE       TRUE
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
 * GCLK_SET1 - configured before DPLLs (needed as DPLL reference)
 * GCLK_SET2 - configured after DPLLs
 *
 * GCLK5 must be in SET1 because DPLL0 uses it as reference.
 * GCLK0 must be in SET2 because it uses DPLL0 output.
 */

#define BOARD_GCLK_SET1          0x0020    /* Pre-configure: GCLK5 */
#define BOARD_GCLK_SET2          0x0fdf    /* Post-configure: all except GCLK5 */

/* GCLK0 - CPU clock from DPLL0 (300 MHz) */

#define BOARD_GCLK0_ENABLE       TRUE
#define BOARD_GCLK0_OOV          FALSE
#define BOARD_GCLK0_OE           FALSE
#define BOARD_GCLK0_DIVSEL       0         /* GCLK frequency = source/DIV */
#define BOARD_GCLK0_RUNSTDBY     FALSE
#define BOARD_GCLK0_SOURCE       7         /* DPLL0 output */
#define BOARD_GCLK0_DIV          1

/* GCLK1 - 48 MHz from DFLL (for USB, SERCOM clocking) */

#define BOARD_GCLK1_ENABLE       TRUE
#define BOARD_GCLK1_OOV          FALSE
#define BOARD_GCLK1_OE           FALSE
#define BOARD_GCLK1_RUNSTDBY     FALSE
#define BOARD_GCLK1_SOURCE       6         /* DFLL output */
#define BOARD_GCLK1_DIV          1

/* GCLK2 - disabled */

#define BOARD_GCLK2_ENABLE       FALSE
#define BOARD_GCLK2_OOV          FALSE
#define BOARD_GCLK2_OE           FALSE
#define BOARD_GCLK2_RUNSTDBY     FALSE
#define BOARD_GCLK2_SOURCE       1
#define BOARD_GCLK2_DIV          1

/* GCLK3 - 32.768 KHz from OSCULP32K (slow clock for SERCOM) */

#define BOARD_GCLK3_ENABLE       TRUE
#define BOARD_GCLK3_OOV          FALSE
#define BOARD_GCLK3_OE           FALSE
#define BOARD_GCLK3_RUNSTDBY     FALSE
#define BOARD_GCLK3_SOURCE       4         /* OSCULP32K */
#define BOARD_GCLK3_DIV          1

/* GCLK4 - 300 MHz from DPLL0 (for peripherals that need high-speed) */

#define BOARD_GCLK4_ENABLE       TRUE
#define BOARD_GCLK4_OOV          FALSE
#define BOARD_GCLK4_OE           FALSE
#define BOARD_GCLK4_RUNSTDBY     FALSE
#define BOARD_GCLK4_SOURCE       7         /* DPLL0 output */
#define BOARD_GCLK4_DIV          1

/* GCLK5 - 6 MHz from XOSC0 ÷ 4 (DPLL0 reference) */

#define BOARD_GCLK5_ENABLE       TRUE
#define BOARD_GCLK5_OOV          FALSE
#define BOARD_GCLK5_OE           FALSE
#define BOARD_GCLK5_RUNSTDBY     FALSE
#define BOARD_GCLK5_SOURCE       0         /* XOSC0 */
#define BOARD_GCLK5_DIV          2         /* 12 MHz / 2 = 6 MHz (DPLL0 ref) */

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

/* DFLL48M - open loop, USB clock recovery mode */

#define BOARD_DFLL_ENABLE        TRUE
#define BOARD_DFLL_RUNSTDBY      FALSE
#define BOARD_DFLL_ONDEMAND      FALSE
#define BOARD_DFLL_MODE          FALSE     /* Open loop mode */
#define BOARD_DFLL_STABLE        FALSE
#define BOARD_DFLL_LLAW          FALSE
#define BOARD_DFLL_USBCRM        TRUE      /* USB clock recovery mode */
#define BOARD_DFLL_CCDIS         TRUE      /* Chill cycle disable */
#define BOARD_DFLL_QLDIS         FALSE
#define BOARD_DFLL_BPLCKC        FALSE
#define BOARD_DFLL_WAITLOCK      TRUE
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
 *   Fckr  = GCLK5 = 6 MHz
 *   Fdpll = 6 MHz × (49 + 1 + 0/32) = 300 MHz
 */

#define BOARD_DPLL0_ENABLE       TRUE
#define BOARD_DPLL0_DCOEN        FALSE
#define BOARD_DPLL0_LBYPASS      FALSE
#define BOARD_DPLL0_WUF          FALSE
#define BOARD_DPLL0_RUNSTDBY     FALSE
#define BOARD_DPLL0_ONDEMAND     FALSE
#define BOARD_DPLL0_REFLOCK      FALSE
#define BOARD_DPLL0_REFCLK       0         /* GCLK reference */
#define BOARD_DPLL0_LTIME        0         /* No time-out */
#define BOARD_DPLL0_FILTER       0         /* Default filter */
#define BOARD_DPLL0_DCOFILTER    0
#define BOARD_DPLL0_GCLK         5         /* GCLK5 as reference */
#define BOARD_DPLL0_GCLKLOCK     0
#define BOARD_DPLL0_LDRFRAC      0         /* No fractional part */
#define BOARD_DPLL0_LDRINT       49        /* LDR = 49 → multiply by 50 */
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
 * GCLK0 → GCLK_MAIN → MCLK → CPU
 * CPU frequency = 300 MHz / 1 = 300 MHz
 */

#define BOARD_MCLK_CPUDIV        1

/* Flash wait states
 *
 * PIC32CZ CA90 at 300 MHz with 3.3V supply needs 7+ wait states.
 * Use 8 for safety margin.
 */

#define BOARD_FLASH_WAITSTATES   8

/* LED definitions **********************************************************/

/* LED0 on PC21 */

#define BOARD_LED0               0
#define BOARD_NLEDS              1

#define BOARD_LED0_BIT           (1 << BOARD_LED0)

#define LED_STARTED              0  /* OFF */
#define LED_HEAPALLOCATE         0  /* OFF */
#define LED_IRQSENABLED          0  /* OFF */
#define LED_STACKCREATED         1  /* ON */
#define LED_INIRQ                2  /* N/C */
#define LED_SIGNAL               2  /* N/C */
#define LED_ASSERTION            2  /* N/C */
#define LED_PANIC                3  /* FLASH */
#undef  LED_IDLE

/* SERCOM configuration *****************************************************/

/* SERCOM slow clock (common to all SERCOMs) */

#define BOARD_SERCOM_SLOWGEN     3                       /* GCLK3 = 32.768 KHz */
#define BOARD_SERCOM_SLOWLOCK    FALSE
#define BOARD_SLOWCLOCK_FREQUENCY BOARD_GCLK3_FREQUENCY

/* SERCOM4 - Console UART (debug via PKOB4)
 *
 *   PC21 = SERCOM4 PAD0 (TX), function E
 *   PC22 = SERCOM4 PAD1 (RX), function E
 *
 *   TXPO = PAD0 (TXPAD0_2: TxD=PAD0, no flow control)
 *   RXPO = PAD1
 *
 *   Core clock: GCLK1 = 48 MHz (good baud rate accuracy at 115200)
 */

#define BOARD_SERCOM4_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM4_PINMAP_PAD0 PORT_SERCOM4_PAD0      /* PB08, TX */
#define BOARD_SERCOM4_PINMAP_PAD1 PORT_SERCOM4_PAD1      /* PB09, RX */
#define BOARD_SERCOM4_PINMAP_PAD2 0
#define BOARD_SERCOM4_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM4      SAM_IRQ_SERCOM4_0      /* INTFLAG[0] DRE */
#define BOARD_RXIRQ_SERCOM4      SAM_IRQ_SERCOM4_2      /* INTFLAG[2] RXC */

#define BOARD_SERCOM4_COREGEN    1                       /* GCLK1 = 48 MHz */
#define BOARD_SERCOM4_CORELOCK   FALSE
#define BOARD_SERCOM4_FREQUENCY  BOARD_GCLK1_FREQUENCY   /* 48 MHz */

/* SERCOM0 - spare UART (PA04/PA05, function E)
 *
 *   PA04 = SERCOM0 PAD0 (TX)
 *   PA05 = SERCOM0 PAD1 (RX)
 */

#define BOARD_SERCOM0_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM0_PINMAP_PAD0 PORT_SERCOM0_PAD0
#define BOARD_SERCOM0_PINMAP_PAD1 PORT_SERCOM0_PAD1
#define BOARD_SERCOM0_PINMAP_PAD2 0
#define BOARD_SERCOM0_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM0      SAM_IRQ_SERCOM0_0
#define BOARD_RXIRQ_SERCOM0      SAM_IRQ_SERCOM0_2

#define BOARD_SERCOM0_COREGEN    1
#define BOARD_SERCOM0_CORELOCK   FALSE
#define BOARD_SERCOM0_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM1 (PA16/PA17, function C) */

#define BOARD_SERCOM1_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM1_PINMAP_PAD0 PORT_SERCOM1_PAD0
#define BOARD_SERCOM1_PINMAP_PAD1 PORT_SERCOM1_PAD1
#define BOARD_SERCOM1_PINMAP_PAD2 0
#define BOARD_SERCOM1_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM1      SAM_IRQ_SERCOM1_0
#define BOARD_RXIRQ_SERCOM1      SAM_IRQ_SERCOM1_2

#define BOARD_SERCOM1_COREGEN    1
#define BOARD_SERCOM1_CORELOCK   FALSE
#define BOARD_SERCOM1_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM2 (PA12/PA13, function C) */

#define BOARD_SERCOM2_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM2_PINMAP_PAD0 PORT_SERCOM2_PAD0
#define BOARD_SERCOM2_PINMAP_PAD1 PORT_SERCOM2_PAD1
#define BOARD_SERCOM2_PINMAP_PAD2 0
#define BOARD_SERCOM2_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM2      SAM_IRQ_SERCOM2_0
#define BOARD_RXIRQ_SERCOM2      SAM_IRQ_SERCOM2_2

#define BOARD_SERCOM2_COREGEN    1
#define BOARD_SERCOM2_CORELOCK   FALSE
#define BOARD_SERCOM2_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM3 (PA22/PA23, function C) */

#define BOARD_SERCOM3_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM3_PINMAP_PAD0 PORT_SERCOM3_PAD0
#define BOARD_SERCOM3_PINMAP_PAD1 PORT_SERCOM3_PAD1
#define BOARD_SERCOM3_PINMAP_PAD2 0
#define BOARD_SERCOM3_PINMAP_PAD3 0

#define BOARD_TXIRQ_SERCOM3      SAM_IRQ_SERCOM3_0
#define BOARD_RXIRQ_SERCOM3      SAM_IRQ_SERCOM3_2

#define BOARD_SERCOM3_COREGEN    1
#define BOARD_SERCOM3_CORELOCK   FALSE
#define BOARD_SERCOM3_FREQUENCY  BOARD_GCLK1_FREQUENCY

/* SERCOM5-7 defaults (not configured for specific pins yet) */

#define BOARD_SERCOM5_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM5_PINMAP_PAD0 0
#define BOARD_SERCOM5_PINMAP_PAD1 0
#define BOARD_SERCOM5_PINMAP_PAD2 0
#define BOARD_SERCOM5_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM5      SAM_IRQ_SERCOM5_0
#define BOARD_RXIRQ_SERCOM5      SAM_IRQ_SERCOM5_2
#define BOARD_SERCOM5_COREGEN    1
#define BOARD_SERCOM5_CORELOCK   FALSE
#define BOARD_SERCOM5_FREQUENCY  BOARD_GCLK1_FREQUENCY

#define BOARD_SERCOM6_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
#define BOARD_SERCOM6_PINMAP_PAD0 0
#define BOARD_SERCOM6_PINMAP_PAD1 0
#define BOARD_SERCOM6_PINMAP_PAD2 0
#define BOARD_SERCOM6_PINMAP_PAD3 0
#define BOARD_TXIRQ_SERCOM6      SAM_IRQ_SERCOM6_0
#define BOARD_RXIRQ_SERCOM6      SAM_IRQ_SERCOM6_2
#define BOARD_SERCOM6_COREGEN    1
#define BOARD_SERCOM6_CORELOCK   FALSE
#define BOARD_SERCOM6_FREQUENCY  BOARD_GCLK1_FREQUENCY

#define BOARD_SERCOM7_MUXCONFIG  (USART_CTRLA_TXPAD0_2 | USART_CTRLA_RXPAD1)
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

#define BOARD_USB_GCLKGEN        1                       /* GCLK1, 48 MHz */

#endif /* __BOARDS_ARM_PIC32CZCA90_PIC32CZCA90_CURIOSITY_INCLUDE_BOARD_H */

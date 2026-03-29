/* SPDX-License-Identifier: Apache-2.0 */

/****************************************************************************
 * arch/arm/src/pic32czca90/hardware/sam_mclk.h
 *
 * PIC32CZ CA90 Main Clock (MCLK) – DS60001749K Section 21
 * Base: 0x44052000
 *
 * CORRECTED: Previous version had SAMD5x APB bus structure.
 * CZCA90 has 6 APB buses (A-F), not 4 like SAMD5x.
 * The bit positions for peripheral enables are completely different.
 *
 * Key CZCA90 APB assignments:
 *   APB A (0x44000000): DSU, FCW, FCR, PM, SUPC, RSTC, OSCCTRL,
 *                       OSC32KCTRL, GCLK, MCLK, FREQM, WDT, RTC
 *   APB B (0x44800000): EIC, PAC, DRMTCM, MCRAMC, TRAM, PORT, DMA,
 *                       EVSY, TRNG
 *   APB C (0x45000000): SERCOM7-9, TCC0-2, I2S1, CAN0/4, GMAC
 *   APB D (0x45800000): SERCOM2/3/5/6, TCC3-4, CAN3, SDHC0, EBI, MLB
 *   APB E (0x46000000): SERCOM0/1/4, TCC5-6, I2S0, CAN1/2, SDHC1
 *   APB F (0x46800000): TCC7-9, ADC, AC, PTC, CAN5
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_MCLK_H
#define __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_MCLK_H

#include "hardware/sam_memorymap.h"

/* =========================================================================
 * Register Offsets – DS60001749K Section 21.6
 * =========================================================================
 */

#define SAM_MCLK_CTRLA_OFFSET       0x0000
#define SAM_MCLK_INTENCLR_OFFSET    0x0001
#define SAM_MCLK_INTENSET_OFFSET    0x0002
#define SAM_MCLK_INTFLAG_OFFSET     0x0003
#define SAM_MCLK_HSDIV_OFFSET       0x0004  /* 8-bit                       */
#define SAM_MCLK_CPUDIV_OFFSET      0x0005  /* 8-bit                       */
#define SAM_MCLK_AHBMASK_OFFSET     0x0010
#define SAM_MCLK_APBAMASK_OFFSET    0x0014
#define SAM_MCLK_APBBMASK_OFFSET    0x0018
#define SAM_MCLK_APBCMASK_OFFSET    0x001C
#define SAM_MCLK_APBDMASK_OFFSET    0x0020
#define SAM_MCLK_APBEMASK_OFFSET    0x0024
#define SAM_MCLK_APBFMASK_OFFSET    0x0028

/* =========================================================================
 * Register Addresses
 * =========================================================================
 */

#define SAM_MCLK_HSDIV              (SAM_MCLK_BASE + SAM_MCLK_HSDIV_OFFSET)
#define SAM_MCLK_CPUDIV             (SAM_MCLK_BASE + SAM_MCLK_CPUDIV_OFFSET)
#define SAM_MCLK_AHBMASK            (SAM_MCLK_BASE + SAM_MCLK_AHBMASK_OFFSET)
#define SAM_MCLK_APBAMASK           (SAM_MCLK_BASE + SAM_MCLK_APBAMASK_OFFSET)
#define SAM_MCLK_APBBMASK           (SAM_MCLK_BASE + SAM_MCLK_APBBMASK_OFFSET)
#define SAM_MCLK_APBCMASK           (SAM_MCLK_BASE + SAM_MCLK_APBCMASK_OFFSET)
#define SAM_MCLK_APBDMASK           (SAM_MCLK_BASE + SAM_MCLK_APBDMASK_OFFSET)
#define SAM_MCLK_APBEMASK           (SAM_MCLK_BASE + SAM_MCLK_APBEMASK_OFFSET)
#define SAM_MCLK_APBFMASK           (SAM_MCLK_BASE + SAM_MCLK_APBFMASK_OFFSET)

/* =========================================================================
 * CPUDIV values
 * =========================================================================
 */

#define MCLK_CPUDIV_DIV1            0x01
#define MCLK_CPUDIV_DIV2            0x02
#define MCLK_CPUDIV_DIV4            0x04
#define MCLK_CPUDIV_DIV8            0x08
#define MCLK_CPUDIV_DIV16           0x10
#define MCLK_CPUDIV_DIV32           0x20
#define MCLK_CPUDIV_DIV64           0x40
#define MCLK_CPUDIV_DIV128          0x80

/* =========================================================================
 * APB A Mask Bits (DS60001749K Table 8-5 peripherals)
 * =========================================================================
 */

#define MCLK_APBAMASK_DSU           (1 << 1)
#define MCLK_APBAMASK_FCW           (1 << 2)
#define MCLK_APBAMASK_FCR           (1 << 3)
#define MCLK_APBAMASK_PM            (1 << 4)
#define MCLK_APBAMASK_SUPC          (1 << 5)
#define MCLK_APBAMASK_RSTC          (1 << 6)
#define MCLK_APBAMASK_OSCCTRL       (1 << 7)
#define MCLK_APBAMASK_OSC32KCTRL    (1 << 8)
#define MCLK_APBAMASK_GCLK          (1 << 9)
#define MCLK_APBAMASK_MCLK          (1 << 10)
#define MCLK_APBAMASK_FREQM         (1 << 11)
#define MCLK_APBAMASK_WDT           (1 << 12)
#define MCLK_APBAMASK_RTC           (1 << 13)
#define MCLK_APBAMASK_BROMC         (1 << 14)

/* =========================================================================
 * APB B Mask Bits (DS60001749K Table 8-6 peripherals)
 * =========================================================================
 */

#define MCLK_APBBMASK_EIC           (1 << 0)
#define MCLK_APBBMASK_PAC           (1 << 1)
#define MCLK_APBBMASK_DRMTCM        (1 << 2)
#define MCLK_APBBMASK_MCRAMC        (1 << 3)
#define MCLK_APBBMASK_TRAM          (1 << 4)
#define MCLK_APBBMASK_PORT          (1 << 5)
#define MCLK_APBBMASK_DMA           (1 << 6)
#define MCLK_APBBMASK_EVSY          (1 << 7)
#define MCLK_APBBMASK_TRNG          (1 << 8)

/* =========================================================================
 * APB C Mask Bits (DS60001749K Table 8-7 peripherals)
 * SERCOM7-9, TCC0-2, I2S1, CAN0/4, GMAC
 * =========================================================================
 */

#define MCLK_APBCMASK_SERCOM7       (1 << 0)
#define MCLK_APBCMASK_SERCOM8       (1 << 1)
#define MCLK_APBCMASK_SERCOM9       (1 << 2)
#define MCLK_APBCMASK_TCC0          (1 << 4)
#define MCLK_APBCMASK_TCC1          (1 << 5)
#define MCLK_APBCMASK_TCC2          (1 << 6)
#define MCLK_APBCMASK_I2S1          (1 << 7)
#define MCLK_APBCMASK_CAN0          (1 << 9)
#define MCLK_APBCMASK_CAN4          (1 << 10)
#define MCLK_APBCMASK_GMAC          (1 << 11)

/* =========================================================================
 * APB D Mask Bits (DS60001749K Table 8-8 peripherals)
 * SERCOM2/3/5/6, TCC3-4, CAN3, SDHC0, EBI, MLB
 * =========================================================================
 */

#define MCLK_APBDMASK_SERCOM2       (1 << 0)
#define MCLK_APBDMASK_SERCOM3       (1 << 1)
#define MCLK_APBDMASK_SERCOM5       (1 << 2)
#define MCLK_APBDMASK_SERCOM6       (1 << 3)
#define MCLK_APBDMASK_TCC3          (1 << 4)
#define MCLK_APBDMASK_TCC4          (1 << 5)
#define MCLK_APBDMASK_CAN3          (1 << 8)
#define MCLK_APBDMASK_SDHC0         (1 << 10)
#define MCLK_APBDMASK_EBI           (1 << 11)
#define MCLK_APBDMASK_MLB           (1 << 12)

/* =========================================================================
 * APB E Mask Bits (DS60001749K Table 8-9 peripherals)
 * SERCOM0/1/4, TCC5-6, I2S0, CAN1/2, SDHC1
 *
 * CORRECTED: Previous file had MCLK_APBEMASK_SERCOM4 = (1 << 0)
 * Correct bit: SERCOM4 is bit 2, SERCOM0=bit0, SERCOM1=bit1
 * =========================================================================
 */

#define MCLK_APBEMASK_SERCOM0       (1 << 0)
#define MCLK_APBEMASK_SERCOM1       (1 << 1)
#define MCLK_APBEMASK_SERCOM4       (1 << 2)   /* Console UART – bit 2!   */
#define MCLK_APBEMASK_TCC5          (1 << 3)
#define MCLK_APBEMASK_TCC6          (1 << 4)
#define MCLK_APBEMASK_I2S0          (1 << 5)
#define MCLK_APBEMASK_CAN1          (1 << 6)
#define MCLK_APBEMASK_CAN2          (1 << 7)
#define MCLK_APBEMASK_SDHC1         (1 << 9)

/* =========================================================================
 * APB F Mask Bits (DS60001749K Table 8-10 peripherals)
 * TCC7-9, ADC, AC, PTC, CAN5
 * =========================================================================
 */

#define MCLK_APBFMASK_TCC7          (1 << 0)
#define MCLK_APBFMASK_TCC8          (1 << 1)
#define MCLK_APBFMASK_TCC9          (1 << 2)
#define MCLK_APBFMASK_ADC           (1 << 3)
#define MCLK_APBFMASK_AC            (1 << 4)
#define MCLK_APBFMASK_PTC           (1 << 5)
#define MCLK_APBFMASK_CAN5          (1 << 6)

#endif /* __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_MCLK_H */

/* SPDX-License-Identifier: Apache-2.0 */

/****************************************************************************
 * arch/arm/src/pic32czca90/hardware/pic32czca90_memorymap.h
 *
 * PIC32CZ CA90 Memory Map - DS60001749K Section 8
 *
 * CORRECTED: Previous version had SAMD5x/E5x addresses throughout.
 * All addresses here are verified from DS60001749K Tables 8-1 through 8-11.
 *
 * Key differences from SAMD5x that caused the port to fail:
 *   - CZCA90 has 6 APB buses (A-F) vs 3 on SAMD5x
 *   - Flash starts at 0x0C000000 not 0x00000000
 *   - OSCCTRL is at 0x44040000 not 0x40001000
 *   - GCLK is at 0x44050000 not 0x40001C00
 *   - PORT is at 0x44840000 not 0x41008000
 *   - SERCOM4 is at 0x46004000 not 0x44000000
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_MEMORYMAP_H
#define __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_MEMORYMAP_H

#include <nuttx/config.h>

/* =========================================================================
 * Flash / SRAM / TCM (DS60001749K Table 8-1, 8-2)
 * =========================================================================
 */

#define SAM_ITCM_BASE           0x00000000  /* 128 KB ITCM SRAM            */
#define SAM_ROM_BASE            0x04000000  /* 32 KB Boot ROM              */
#define SAM_BFM_BASE            0x08000000  /* 128 KB Boot Flash (BFM)     */
#define SAM_CFM_BASE            0x0A000000  /* 64 KB Config Fuses          */
#define SAM_FLASH_BASE          0x0C000000  /* 8 MB Program Flash (PFM1)   */
#define SAM_DTCM_BASE           0x20000000  /* 128 KB DTCM SRAM            */
#define SAM_SRAM_BASE           0x20020000  /* 896 KB Data RAM (DRM)       */

/* =========================================================================
 * APB A (0x44000000 – 0x447FFFFF) – DS60001749K Table 8-5
 * =========================================================================
 */

#define SAM_DSU_BASE            0x44000000
#define SAM_FCW_BASE            0x44002000  /* Flash Write Controller      */
#define SAM_FCR_BASE            0x44004000  /* Flash Read Controller       */
#define SAM_PM_BASE             0x44010000  /* Power Manager               */
#define SAM_SUPC_BASE           0x44020000  /* Supply Controller           */
#define SAM_RSTC_BASE           0x44030000  /* Reset Controller            */
#define SAM_OSCCTRL_BASE        0x44040000  /* Oscillator Controller       */
#define SAM_OSC32KCTRL_BASE     0x44042000  /* 32K Oscillator Controller   */
#define SAM_GCLK_BASE           0x44050000  /* Generic Clock Controller    */
#define SAM_MCLK_BASE           0x44052000  /* Main Clock                  */
#define SAM_FREQM_BASE          0x44060000  /* Frequency Meter             */
#define SAM_WDT_BASE            0x44070000  /* Watchdog Timer              */
#define SAM_RTC_BASE            0x44072000  /* Real-Time Counter           */
#define SAM_BROMC_BASE          0x44080000  /* Boot ROM Controller         */

/* =========================================================================
 * APB B (0x44800000 – 0x44FFFFFF) – DS60001749K Table 8-6
 * =========================================================================
 */

#define SAM_EIC_BASE            0x44800000  /* External Interrupt Ctrl     */
#define SAM_PAC_BASE            0x44810000  /* Peripheral Access Ctrl      */
#define SAM_DRMTCM_BASE         0x44820000  /* TCM RAM ECC                 */
#define SAM_MCRAMC_BASE         0x44822000  /* Multi-Channel RAM Ctrl      */
#define SAM_TRAM_BASE           0x44824000  /* TrustRAM                    */
#define SAM_PORT_BASE           0x44840000  /* PORT (GPIO) A-G             */
#define SAM_DMAC_BASE           0x44850000  /* DMA Controller              */
#define SAM_EVSYS_BASE          0x44860000  /* Event System                */
#define SAM_TRNG_BASE           0x44870000  /* True Random Number Gen      */

/* =========================================================================
 * APB C (0x45000000 – 0x457FFFFF) – DS60001749K Table 8-7
 * =========================================================================
 */

#define SAM_SERCOM7_BASE        0x45000000
#define SAM_SERCOM8_BASE        0x45002000
#define SAM_SERCOM9_BASE        0x45004000
#define SAM_TCC0_BASE           0x45010000
#define SAM_TCC1_BASE           0x45012000
#define SAM_TCC2_BASE           0x45014000
#define SAM_I2S1_BASE           0x45030000
#define SAM_CAN0_BASE           0x45060000
#define SAM_CAN4_BASE           0x45062000
#define SAM_GMAC_BASE           0x45070000

/* =========================================================================
 * APB D (0x45800000 – 0x45FFFFFF) – DS60001749K Table 8-8
 * =========================================================================
 */

#define SAM_SERCOM2_BASE        0x45800000
#define SAM_SERCOM3_BASE        0x45802000
#define SAM_SERCOM5_BASE        0x45804000
#define SAM_SERCOM6_BASE        0x45806000
#define SAM_TCC3_BASE           0x45810000
#define SAM_TCC4_BASE           0x45812000
#define SAM_CAN3_BASE           0x45860000  /* CAN3 (Curiosity J701)       */
#define SAM_SDHC0_BASE          0x458A0000
#define SAM_EBI_BASE            0x458B0000
#define SAM_MLB_BASE            0x458C0000

/* =========================================================================
 * APB E (0x46000000 – 0x467FFFFF) – DS60001749K Table 8-9
 * SERCOM0, SERCOM1, SERCOM4 are on APB E
 * =========================================================================
 */

#define SAM_SERCOM0_BASE        0x46000000
#define SAM_SERCOM1_BASE        0x46002000
#define SAM_SERCOM4_BASE        0x46004000  /* Console UART (PKoB4 VCP)    */
#define SAM_TCC5_BASE           0x46010000
#define SAM_TCC6_BASE           0x46012000
#define SAM_I2S0_BASE           0x46030000
#define SAM_CAN1_BASE           0x46060000
#define SAM_CAN2_BASE           0x46062000
#define SAM_SDHC1_BASE          0x460A2000

/* =========================================================================
 * APB F (0x46800000 – 0x46FFFFFF) – DS60001749K Table 8-10
 * =========================================================================
 */

#define SAM_TCC7_BASE           0x46810000
#define SAM_TCC8_BASE           0x46812000
#define SAM_TCC9_BASE           0x46814000
#define SAM_ADC0_BASE           0x46820000
#define SAM_AC_BASE             0x46822000
#define SAM_PTC_BASE            0x46828000
#define SAM_CAN5_BASE           0x46860000

/* =========================================================================
 * AHB Peripherals (0x4F000000) – DS60001749K Table 8-11
 * =========================================================================
 */

#define SAM_SQI0_BASE           0x4F008000
#define SAM_SQI1_BASE           0x4F009000
#define SAM_HSUSB0_BASE         0x4F010000
#define SAM_HSUSB1_BASE         0x4F012000

/* =========================================================================
 * External Bus / SQI Memory Windows – DS60001749K Table 8-4
 * =========================================================================
 */

#define SAM_EBI_CS0_BASE        0x60000000
#define SAM_EBI_CS1_BASE        0x61000000
#define SAM_EBI_CS2_BASE        0x62000000
#define SAM_EBI_CS3_BASE        0x63000000
#define SAM_SQI0_MEM_BASE       0x80000000  /* 256 MB XIP window SQI0     */
#define SAM_SQI1_MEM_BASE       0x90000000  /* 256 MB XIP window SQI1     */

/* =========================================================================
 * PORT group base addresses (stride = 0x80 per group)
 * DS60001749K Table 8-6: PORT base = 0x44840000
 * Groups A-G, 7 total on CZCA90
 * =========================================================================
 */

#define SAM_PORTA_BASE          (SAM_PORT_BASE + 0x0000)
#define SAM_PORTB_BASE          (SAM_PORT_BASE + 0x0080)
#define SAM_PORTC_BASE          (SAM_PORT_BASE + 0x0100)  /* Console UART  */
#define SAM_PORTD_BASE          (SAM_PORT_BASE + 0x0180)
#define SAM_PORTE_BASE          (SAM_PORT_BASE + 0x0200)
#define SAM_PORTF_BASE          (SAM_PORT_BASE + 0x0280)
#define SAM_PORTG_BASE          (SAM_PORT_BASE + 0x0300)

/* =========================================================================
 * Compatibility: CZCA90 uses FCR/FCW instead of NVMCTRL.
 * Do NOT define SAM_NVMCTRL_BASE — there is no NVMCTRL on CZCA90.
 * Code using SAM_NVMCTRL_CTRLA must be updated to use FCR.
 * =========================================================================
 */

#undef SAM_NVMCTRL_BASE   /* Intentionally removed — use SAM_FCR_BASE */

#endif /* __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_MEMORYMAP_H */

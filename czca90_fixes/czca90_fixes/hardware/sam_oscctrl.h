/* SPDX-License-Identifier: Apache-2.0 */

/****************************************************************************
 * arch/arm/src/pic32czca90/hardware/sam_oscctrl.h
 *
 * PIC32CZ CA90 Oscillator Controller (OSCCTRL) – DS60001749K Section 18
 * Base: 0x44040000
 *
 * CORRECTED: Previous version had SAMD5x register offset order.
 * On CZCA90, XOSCCTRL0/1 come FIRST (0x0000, 0x0004), then
 * EVCTRL/INTENCLR/INTENSET/INTFLAG/STATUS at 0x0008+.
 * On SAMD5x the order was reversed. This caused XOSC enable writes
 * to hit the EVCTRL register instead — XOSC0 never started.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_OSCCTRL_H
#define __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_OSCCTRL_H

#include "hardware/sam_memorymap.h"

/* =========================================================================
 * Register Offsets – DS60001749K Section 18.7
 * IMPORTANT: Order differs from SAMD5x
 * =========================================================================
 */

/* XOSC Control – FIRST registers on CZCA90 */
#define SAM_OSCCTRL_XOSCCTRL0_OFFSET    0x0000  /* External Osc 0 Control  */
#define SAM_OSCCTRL_XOSCCTRL1_OFFSET    0x0004  /* External Osc 1 Control  */

/* Event / Interrupt / Status – come AFTER XOSC on CZCA90 */
#define SAM_OSCCTRL_EVCTRL_OFFSET       0x0008
#define SAM_OSCCTRL_INTENCLR_OFFSET     0x000C
#define SAM_OSCCTRL_INTENSET_OFFSET     0x0010
#define SAM_OSCCTRL_INTFLAG_OFFSET      0x0014
#define SAM_OSCCTRL_STATUS_OFFSET       0x0018

/* DFLL */
#define SAM_OSCCTRL_DFLLCTRLA_OFFSET    0x001C
#define SAM_OSCCTRL_DFLLCTRLB_OFFSET    0x0020
#define SAM_OSCCTRL_DFLLVAL_OFFSET      0x0024
#define SAM_OSCCTRL_DFLLMUL_OFFSET      0x0028
#define SAM_OSCCTRL_DFLLSYNC_OFFSET     0x002C

/* DPLL0 */
#define SAM_OSCCTRL_DPLL0CTRLA_OFFSET   0x0030
#define SAM_OSCCTRL_DPLL0RATIO_OFFSET   0x0034
#define SAM_OSCCTRL_DPLL0CTRLB_OFFSET   0x0038
#define SAM_OSCCTRL_DPLL0SYNCBUSY_OFFSET 0x003C
#define SAM_OSCCTRL_DPLL0STATUS_OFFSET  0x0040

/* DPLL1 */
#define SAM_OSCCTRL_DPLL1CTRLA_OFFSET   0x0044
#define SAM_OSCCTRL_DPLL1RATIO_OFFSET   0x0048
#define SAM_OSCCTRL_DPLL1CTRLB_OFFSET   0x004C
#define SAM_OSCCTRL_DPLL1SYNCBUSY_OFFSET 0x0050
#define SAM_OSCCTRL_DPLL1STATUS_OFFSET  0x0054

/* =========================================================================
 * Register Addresses
 * =========================================================================
 */

#define SAM_OSCCTRL_XOSCCTRL0       (SAM_OSCCTRL_BASE + SAM_OSCCTRL_XOSCCTRL0_OFFSET)
#define SAM_OSCCTRL_XOSCCTRL1       (SAM_OSCCTRL_BASE + SAM_OSCCTRL_XOSCCTRL1_OFFSET)
#define SAM_OSCCTRL_EVCTRL          (SAM_OSCCTRL_BASE + SAM_OSCCTRL_EVCTRL_OFFSET)
#define SAM_OSCCTRL_INTENCLR        (SAM_OSCCTRL_BASE + SAM_OSCCTRL_INTENCLR_OFFSET)
#define SAM_OSCCTRL_INTENSET        (SAM_OSCCTRL_BASE + SAM_OSCCTRL_INTENSET_OFFSET)
#define SAM_OSCCTRL_INTFLAG         (SAM_OSCCTRL_BASE + SAM_OSCCTRL_INTFLAG_OFFSET)
#define SAM_OSCCTRL_STATUS          (SAM_OSCCTRL_BASE + SAM_OSCCTRL_STATUS_OFFSET)
#define SAM_OSCCTRL_DFLLCTRLA       (SAM_OSCCTRL_BASE + SAM_OSCCTRL_DFLLCTRLA_OFFSET)
#define SAM_OSCCTRL_DFLLCTRLB       (SAM_OSCCTRL_BASE + SAM_OSCCTRL_DFLLCTRLB_OFFSET)
#define SAM_OSCCTRL_DFLLVAL         (SAM_OSCCTRL_BASE + SAM_OSCCTRL_DFLLVAL_OFFSET)
#define SAM_OSCCTRL_DFLLMUL         (SAM_OSCCTRL_BASE + SAM_OSCCTRL_DFLLMUL_OFFSET)
#define SAM_OSCCTRL_DFLLSYNC        (SAM_OSCCTRL_BASE + SAM_OSCCTRL_DFLLSYNC_OFFSET)

/* Indexed accessors for DPLL0/1 (stride 0x14 between instances) */
#define SAM_OSCCTRL_XOSCCTRL(n)     (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_XOSCCTRL0_OFFSET + (n)*4)
#define SAM_OSCCTRL_DPLLCTRLA(n)    (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_DPLL0CTRLA_OFFSET + (n)*0x14)
#define SAM_OSCCTRL_DPLLRATIO(n)    (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_DPLL0RATIO_OFFSET + (n)*0x14)
#define SAM_OSCCTRL_DPLLCTRLB(n)    (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_DPLL0CTRLB_OFFSET + (n)*0x14)
#define SAM_OSCCTRL_DPLLSYNCBUSY(n) (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_DPLL0SYNCBUSY_OFFSET + (n)*0x14)
#define SAM_OSCCTRL_DPLLSTATUS(n)   (SAM_OSCCTRL_BASE + \
                                     SAM_OSCCTRL_DPLL0STATUS_OFFSET + (n)*0x14)

/* =========================================================================
 * XOSCCTRL Bit Definitions (32-bit)
 * =========================================================================
 */

#define OSCCTRL_XOSCCTRL_ENABLE         (1 << 1)
#define OSCCTRL_XOSCCTRL_XTALEN         (1 << 2)   /* 0=ext clock, 1=crystal */
#define OSCCTRL_XOSCCTRL_CFDEN          (1 << 3)
#define OSCCTRL_XOSCCTRL_SWBEN          (1 << 4)
#define OSCCTRL_XOSCCTRL_RUNSTDBY       (1 << 6)
#define OSCCTRL_XOSCCTRL_ONDEMAND       (1 << 7)
#define OSCCTRL_XOSCCTRL_LOWBUFGAIN     (1 << 8)
#define OSCCTRL_XOSCCTRL_IPTAT_SHIFT    9
#define OSCCTRL_XOSCCTRL_IPTAT_MASK     (0x3 << OSCCTRL_XOSCCTRL_IPTAT_SHIFT)
#define OSCCTRL_XOSCCTRL_IMULT_SHIFT    11
#define OSCCTRL_XOSCCTRL_IMULT_MASK     (0xf << OSCCTRL_XOSCCTRL_IMULT_SHIFT)
#define OSCCTRL_XOSCCTRL_ENALC          (1 << 15)
#define OSCCTRL_XOSCCTRL_CFDEN_BIT      (1 << 16)
#define OSCCTRL_XOSCCTRL_SWBEN_BIT      (1 << 17)
#define OSCCTRL_XOSCCTRL_STARTUP_SHIFT  20
#define OSCCTRL_XOSCCTRL_STARTUP_MASK   (0xf << OSCCTRL_XOSCCTRL_STARTUP_SHIFT)

/* =========================================================================
 * STATUS Bit Definitions
 * =========================================================================
 */

#define OSCCTRL_STATUS_XOSCRDY0         (1 << 0)
#define OSCCTRL_STATUS_XOSCRDY1         (1 << 1)
#define OSCCTRL_STATUS_XOSCFAIL0        (1 << 2)
#define OSCCTRL_STATUS_XOSCFAIL1        (1 << 3)
#define OSCCTRL_STATUS_DFLLRDY          (1 << 8)
#define OSCCTRL_STATUS_DFLLOOB          (1 << 9)
#define OSCCTRL_STATUS_DFLLLCKF         (1 << 10)
#define OSCCTRL_STATUS_DFLLLCKC         (1 << 11)
#define OSCCTRL_STATUS_DPLLLCKR0        (1 << 16)
#define OSCCTRL_STATUS_DPLLLCKF0        (1 << 17)
#define OSCCTRL_STATUS_DPLLTO0          (1 << 18)
#define OSCCTRL_STATUS_DPLLLDRTO0       (1 << 19)
#define OSCCTRL_STATUS_DPLLLCKR1        (1 << 24)
#define OSCCTRL_STATUS_DPLLLCKF1        (1 << 25)

/* =========================================================================
 * DFLL Bit Definitions
 * =========================================================================
 */

#define OSCCTRL_DFLLCTRLA_ENABLE        (1 << 1)
#define OSCCTRL_DFLLCTRLA_RUNSTDBY      (1 << 6)
#define OSCCTRL_DFLLCTRLA_ONDEMAND      (1 << 7)

#define OSCCTRL_DFLLCTRLB_MODE          (1 << 0)
#define OSCCTRL_DFLLCTRLB_STABLE        (1 << 1)
#define OSCCTRL_DFLLCTRLB_LLAW          (1 << 2)
#define OSCCTRL_DFLLCTRLB_USBCRM        (1 << 3)
#define OSCCTRL_DFLLCTRLB_CCDIS         (1 << 4)
#define OSCCTRL_DFLLCTRLB_QLDIS         (1 << 5)
#define OSCCTRL_DFLLCTRLB_BPLCKC        (1 << 6)
#define OSCCTRL_DFLLCTRLB_WAITLOCK      (1 << 7)

#define OSCCTRL_DFLLMUL_MUL_SHIFT       0
#define OSCCTRL_DFLLMUL_MUL_MASK        (0xffff << OSCCTRL_DFLLMUL_MUL_SHIFT)
#define OSCCTRL_DFLLMUL_FSTEP_SHIFT     16
#define OSCCTRL_DFLLMUL_FSTEP_MASK      (0xff << OSCCTRL_DFLLMUL_FSTEP_SHIFT)
#define OSCCTRL_DFLLMUL_CSTEP_SHIFT     26
#define OSCCTRL_DFLLMUL_CSTEP_MASK      (0x3f << OSCCTRL_DFLLMUL_CSTEP_SHIFT)

#define OSCCTRL_DFLLSYNC_ENABLE         (1 << 1)
#define OSCCTRL_DFLLSYNC_DFLLCTRLB      (1 << 2)
#define OSCCTRL_DFLLSYNC_DFLLVAL        (1 << 3)
#define OSCCTRL_DFLLSYNC_DFLLMUL        (1 << 4)

/* =========================================================================
 * DPLL Bit Definitions
 * =========================================================================
 */

#define OSCCTRL_DPLLCTRLA_ENABLE        (1 << 1)
#define OSCCTRL_DPLLCTRLA_RUNSTDBY      (1 << 6)
#define OSCCTRL_DPLLCTRLA_ONDEMAND      (1 << 7)

#define OSCCTRL_DPLLRATIO_LDR_SHIFT     0
#define OSCCTRL_DPLLRATIO_LDR_MASK      (0x1fff << OSCCTRL_DPLLRATIO_LDR_SHIFT)
#define OSCCTRL_DPLLRATIO_LDRFRAC_SHIFT 16
#define OSCCTRL_DPLLRATIO_LDRFRAC_MASK  (0x1f << OSCCTRL_DPLLRATIO_LDRFRAC_SHIFT)

#define OSCCTRL_DPLLCTRLB_FILTER_SHIFT  0
#define OSCCTRL_DPLLCTRLB_FILTER_MASK   (0xf << OSCCTRL_DPLLCTRLB_FILTER_SHIFT)
#define OSCCTRL_DPLLCTRLB_WUF           (1 << 4)
#define OSCCTRL_DPLLCTRLB_REFCLK_SHIFT  5
#define OSCCTRL_DPLLCTRLB_REFCLK_MASK   (0x7 << OSCCTRL_DPLLCTRLB_REFCLK_SHIFT)
#  define OSCCTRL_DPLLCTRLB_REFCLK_GCLK   (0 << OSCCTRL_DPLLCTRLB_REFCLK_SHIFT)
#  define OSCCTRL_DPLLCTRLB_REFCLK_XOSC32 (1 << OSCCTRL_DPLLCTRLB_REFCLK_SHIFT)
#  define OSCCTRL_DPLLCTRLB_REFCLK_XOSC0  (2 << OSCCTRL_DPLLCTRLB_REFCLK_SHIFT)
#  define OSCCTRL_DPLLCTRLB_REFCLK_XOSC1  (3 << OSCCTRL_DPLLCTRLB_REFCLK_SHIFT)
#define OSCCTRL_DPLLCTRLB_LTIME_SHIFT   8
#define OSCCTRL_DPLLCTRLB_LTIME_MASK    (0x7 << OSCCTRL_DPLLCTRLB_LTIME_SHIFT)
#define OSCCTRL_DPLLCTRLB_LBYPASS       (1 << 11)
#define OSCCTRL_DPLLCTRLB_DCOFILTER_SHIFT 12
#define OSCCTRL_DPLLCTRLB_DCOFILTER_MASK  (0x7 << OSCCTRL_DPLLCTRLB_DCOFILTER_SHIFT)
#define OSCCTRL_DPLLCTRLB_DCOEN         (1 << 15)
#define OSCCTRL_DPLLCTRLB_DIV_SHIFT     16
#define OSCCTRL_DPLLCTRLB_DIV_MASK      (0x7ff << OSCCTRL_DPLLCTRLB_DIV_SHIFT)

#define OSCCTRL_DPLLSYNCBUSY_ENABLE     (1 << 1)
#define OSCCTRL_DPLLSYNCBUSY_DPLLRATIO  (1 << 2)

#define OSCCTRL_DPLLSTATUS_LOCK         (1 << 0)
#define OSCCTRL_DPLLSTATUS_CLKRDY       (1 << 1)

#endif /* __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_SAM_OSCCTRL_H */

/* SPDX-License-Identifier: Apache-2.0 */

/****************************************************************************
 * arch/arm/src/pic32czca90/hardware/pic32czca90_pinmap.h
 *
 * PIC32CZ CA90 pin multiplexing definitions
 * Curiosity Ultra board (EV16W43A) – DS70005522C
 *
 * CORRECTED: Previous version had SERCOM4 on PB08/PB09 (SAMD5x default).
 *
 * Actual Curiosity Ultra console UART pins (DS70005522C schematic):
 *   PKoB4_VCP → APP_VCP_TX/RX → PC21 (TX) / PC22 (RX)
 *   Confirmed from schematic sheet SHT_4_Target_MCU_R4, signal names:
 *   P17_PC21_EXT2_SERCOM4_PAD0 (TX)
 *   P18_PC22_EXT2_SERCOM4_PAD1 (RX)
 *   PMUX function = E (value 4) for SERCOM4 on PC21/PC22
 *
 * Oscillator: Y300 = DSC6011JI2B-012.0000 = 12 MHz (NOT 24 MHz)
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_PINMAP_H
#define __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_PINMAP_H

/* =========================================================================
 * Pin encoding for port_pinset_t (uint32_t)
 *
 *   Bits 31-28: Port (0=A, 1=B, 2=C, 3=D, 4=E, 5=F, 6=G)
 *   Bits 27-24: Peripheral function (A=0 .. N=13)
 *   Bits 20-16: Pin number (0-31)
 *   Bits 7-0:   Configuration flags
 * =========================================================================
 */

#define PORT_PORTA          (0u << 28)
#define PORT_PORTB          (1u << 28)
#define PORT_PORTC          (2u << 28)
#define PORT_PORTD          (3u << 28)
#define PORT_PORTE          (4u << 28)
#define PORT_PORTF          (5u << 28)
#define PORT_PORTG          (6u << 28)

#define PORT_FUNC_SHIFT     24
#define PORT_FUNC(n)        ((uint32_t)(n) << PORT_FUNC_SHIFT)

#define PORT_PIN_SHIFT      16
#define PORT_PIN(n)         ((uint32_t)(n) << PORT_PIN_SHIFT)

/* Configuration flags */
#define PORT_FLAG_PMUXEN    (1 << 0)
#define PORT_FLAG_INEN      (1 << 1)
#define PORT_FLAG_PULLEN    (1 << 2)
#define PORT_FLAG_OUTPUT    (1 << 3)
#define PORT_FLAG_DRVSTR    (1 << 4)

/* =========================================================================
 * SERCOM4 – Console UART (PKoB4 VCP)
 *
 * CORRECTED from PB08/PB09 to PC21/PC22.
 * DS70005522C schematic net names:
 *   P17_PC21_EXT2_SERCOM4_PAD0 → PKoB4 APP_VCP_RX (board TX)
 *   P18_PC22_EXT2_SERCOM4_PAD1 → PKoB4 APP_VCP_TX (board RX)
 *
 * PMUX function E (value 4) = SERCOM4 on Port C pins 21/22
 * This is verified from DS70005522C Table 2-11 and schematic page 21.
 * =========================================================================
 */

#define PORT_SERCOM4_PAD0   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(21) | \
                             PORT_FLAG_PMUXEN)                /* PC21 TX */
#define PORT_SERCOM4_PAD1   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(22) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN) /* PC22 RX */

/* =========================================================================
 * SERCOM0 – PA04(TX)/PA05(RX), function D
 * =========================================================================
 */

#define PORT_SERCOM0_PAD0   (PORT_PORTA | PORT_FUNC(3) | PORT_PIN(4) | \
                             PORT_FLAG_PMUXEN)
#define PORT_SERCOM0_PAD1   (PORT_PORTA | PORT_FUNC(3) | PORT_PIN(5) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

/* =========================================================================
 * SERCOM1 – PA16(TX)/PA17(RX), function C
 * =========================================================================
 */

#define PORT_SERCOM1_PAD0   (PORT_PORTA | PORT_FUNC(2) | PORT_PIN(16) | \
                             PORT_FLAG_PMUXEN)
#define PORT_SERCOM1_PAD1   (PORT_PORTA | PORT_FUNC(2) | PORT_PIN(17) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

/* =========================================================================
 * SERCOM2 – PC08(TX)/PC09(RX), function E (EXT1 SPI pins)
 * DS70005522C Table 2-4 EXT1: SPI MOSI=PC08, SCK=PC09
 * =========================================================================
 */

#define PORT_SERCOM2_PAD0   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(8) | \
                             PORT_FLAG_PMUXEN)
#define PORT_SERCOM2_PAD1   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(9) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

/* =========================================================================
 * SERCOM3 – PC12(TX)/PC13(RX), function E (EXT2 / MikroBUS SPI)
 * DS70005522C Table 2-3 MikroBUS: MOSI=PC12, SCK=PC13
 * =========================================================================
 */

#define PORT_SERCOM3_PAD0   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(12) | \
                             PORT_FLAG_PMUXEN)
#define PORT_SERCOM3_PAD1   (PORT_PORTC | PORT_FUNC(4) | PORT_PIN(13) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

/* =========================================================================
 * On-board LEDs – DS70005522C Table 2-11
 * LED0: PB21 (active LOW, yellow)
 * LED1: PB22 (active LOW, yellow)
 * =========================================================================
 */

#define PORT_LED0           (PORT_PORTB | PORT_PIN(21) | PORT_FLAG_OUTPUT)
#define PORT_LED1           (PORT_PORTB | PORT_PIN(22) | PORT_FLAG_OUTPUT)

/* =========================================================================
 * On-board Buttons – DS70005522C Table 2-11
 * SW0: PB24 (active LOW, input with pullup)
 * SW1: PC23 (active LOW, input with pullup)
 * =========================================================================
 */

#define PORT_SW0            (PORT_PORTB | PORT_PIN(24) | \
                             PORT_FLAG_INEN | PORT_FLAG_PULLEN)
#define PORT_SW1            (PORT_PORTC | PORT_PIN(23) | \
                             PORT_FLAG_INEN | PORT_FLAG_PULLEN)

/* =========================================================================
 * CAN3 – DS70005522C schematic, ATA6561 transceiver J701
 * PD13 = CAN3_TX (PMUX G = function 6)
 * PC29 = CAN3_RX (PMUX G = function 6)
 * =========================================================================
 */

#define PORT_CAN3_TX        (PORT_PORTD | PORT_FUNC(6) | PORT_PIN(13) | \
                             PORT_FLAG_PMUXEN)
#define PORT_CAN3_RX        (PORT_PORTC | PORT_FUNC(6) | PORT_PIN(29) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

/* =========================================================================
 * CAN4 – DS70005522C schematic, ATA6561 transceiver J702
 * PA31 = CAN4_TX (PMUX G = function 6)
 * PA30 = CAN4_RX (PMUX G = function 6)
 * =========================================================================
 */

#define PORT_CAN4_TX        (PORT_PORTA | PORT_FUNC(6) | PORT_PIN(31) | \
                             PORT_FLAG_PMUXEN)
#define PORT_CAN4_RX        (PORT_PORTA | PORT_FUNC(6) | PORT_PIN(30) | \
                             PORT_FLAG_PMUXEN | PORT_FLAG_INEN)

#endif /* __ARCH_ARM_SRC_PIC32CZCA90_HARDWARE_PIC32CZCA90_PINMAP_H */

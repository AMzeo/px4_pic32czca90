/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * SAMV71-XULT with Click sensor boards internal definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#include <sam_gpio.h>

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* GPIOs ***********************************************************************************/

/* LEDs - SAMV71-XULT has two LEDs:
 *   PA23 - Yellow LED0
 *   PC9  - Yellow LED1
 */

#define GPIO_nLED_BLUE       /* PA23 */  (GPIO_OUTPUT|GPIO_CFG_PULLUP|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN23)

/* Only one LED available on SAMV71-XULT - PA23 (Blue LED)
 * Driver will use drv_board_led.h defaults:
 * LED_BLUE=0, LED_AMBER=1, LED_RED=1, LED_GREEN=3
 * Board only implements LED_BLUE (index 0)
 */

#define BOARD_HAS_CONTROL_STATUS_LEDS      1
#define BOARD_ARMED_STATE_LED  LED_BLUE

/* ICM20689 on EXT1 header (not mikroBUS socket)
 * EXT1 Pin 15 = CS  = PD25
 * EXT1 Pin 9  = IRQ = PD28 (directly connected to DRDY)
 */
#define GPIO_SPI0_CS_ICM20689    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN25)
#define GPIO_SPI0_DRDY_ICM20689  (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_INT_FALLING|GPIO_PORT_PIOD|GPIO_PIN28)
#define GPIO_SPI0_DRDY_ICM20689_IRQ  SAM_IRQ_PD28

/* BMP388 Pressure sensor on EXT2 header via mikroBUS adapter
 * EXT2 Pin 15 = CS  = PD27
 * BMP388 does not use DRDY, uses polling mode
 */
#define GPIO_SPI0_CS_BMP388      (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN27)

/* mikroBUS Socket RST pins - Active LOW, start HIGH to release reset
 * Socket 1: PA19 (RST), PA0 (INT)
 * Socket 2: PB0 (RST), PA6 (INT)
 * EXT1 adapter: PA5 (RST) - for Xplained Pro extension reset line (EXT1 pin 10)
 * EXT2 adapter: PA24 (RST) - for Xplained Pro extension reset line (EXT2 pin 10)
 * Compass 4 Click (AK09915) requires RST pin HIGH to operate
 */
#define GPIO_MB1_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN19)
#define GPIO_MB2_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOB|GPIO_PIN0)
#define GPIO_EXT1_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN5)
#define GPIO_EXT2_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN24)

/* Primary storage defaults to SD card. Enable BOARD_HAS_FRAM_CLICK (and re-add
 * FLASH_BASED_PARAMS) only when a FRAM Click board or other flash backend is
 * present.
 */
// #define FLASH_BASED_PARAMS

/* ADC Channels ***********************************************************************************/

/* ADC is not yet configured for SAMV71-XULT
 * Define placeholder channels to allow battery_status module to compile
 * These will need to be properly configured once ADC hardware mapping is done
 */
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define BOARD_NUMBER_BRICKS          1
#define BOARD_ADC_BRICK_VALID        1  /* Brick 1 valid (placeholder - all bricks considered valid) */

/* I2C Buses ***********************************************************************************/

/* SAMV71-XULT I2C Configuration:
 * I2C0 (TWIHS0): All sensors on mikroBUS sockets and Arduino headers
 *   PA3 - TWD0 (SDA)
 *   PA4 - TWCK0 (SCL)
 */

#define PX4_NUMBER_I2C_BUSES 1
#define BOARD_NUMBER_I2C_BUSES 1

/* PWM Timer Configuration ***********************************************************************************/

/* SAMV71-XULT PWM Configuration using TC (Timer/Counter):
 * TC0 CH0 (TC0) - Reserved for HRT
 * TC0 CH1 (TC1) - PWM1: PA15 (TIOA1)
 * TC0 CH2 (TC2) - RESERVED: PA26 conflicts with HSMCI DA2 (SD card data line 2)
 * TC1 CH0 (TC3) - PWM2: PC23 (TIOA3)
 * TC1 CH1 (TC4) - PWM3: PC26 (TIOA4)
 * TC1 CH2 (TC5) - Reserved for RC Input capture: PC29 (TIOA5)
 *
 * NOTE: TC0 CH2 (PA26) cannot be used for PWM because PA26 is the HSMCI0 DA2
 *       data line required for 4-bit SD card mode. Using PA26 for PWM would
 *       corrupt SD card writes (bits 2 and 6 are carried on D2).
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  3

/* PWM Output GPIO Definitions - TC TIOA outputs for PWM */
#define GPIO_PWM1_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN15)  /* TC1 TIOA - PA15 */
/* GPIO_PWM2_OUT (PA26) REMOVED - conflicts with HSMCI0 DA2 (SD card data line 2) */
#define GPIO_PWM2_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN23)  /* TC3 TIOA - PC23 */
#define GPIO_PWM3_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN26)  /* TC4 TIOA - PC26 */

/* RC Input capture - TC5 (TC1 CH2) - Reserved for future use */
#define GPIO_RC_INPUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN29)  /* TC5 TIOA - PC29 */

/* High-resolution timer */
#define HRT_TIMER               0  /* use TC0 channel 0 for the HRT */
#define HRT_TIMER_CHANNEL       0  /* use capture/compare channel 0 */

/* HSMCI SD Card ***************************************************************************/

#ifdef CONFIG_SAMV7_HSMCI0
#  define HSMCI0_SLOTNO      0
#  define HSMCI0_MINOR       0
  /* Card Detect: PD18, active low, interrupt on both edges */
#  define GPIO_HSMCI0_CD     (GPIO_INPUT | GPIO_CFG_DEFAULT | GPIO_CFG_DEGLITCH | \
                              GPIO_INT_BOTHEDGES | GPIO_PORT_PIOD | GPIO_PIN18)
#  define IRQ_HSMCI0_CD      SAM_IRQ_PD18
#endif

/* USB ***********************************************************************************/

/* SAMV71 has USB high-speed device */

/* This board provides a DMA pool */
#define BOARD_DMA_ALLOC_POOL_SIZE 5120

/* This board provides the board_on_reset interface */
#define BOARD_HAS_ON_RESET 1

/* Hardfault log path for crash dumps - stored in flash */
#define HARDFAULT_ULOG_PATH "/fs/microsd"
#define HARDFAULT_MAX_ULOG_FILE_LEN 80  /* Maximum length for ULog filename */

#define PX4_GPIO_INIT_LIST { \
		GPIO_nLED_BLUE,           \
		GPIO_SPI0_CS_ICM20689,    \
		GPIO_SPI0_DRDY_ICM20689,  \
		GPIO_SPI0_CS_BMP388,      \
		GPIO_MB1_RST,             \
		GPIO_MB2_RST,             \
		GPIO_EXT1_RST,            \
		GPIO_EXT2_RST,            \
		GPIO_PWM1_OUT,            \
		GPIO_PWM2_OUT,            \
		GPIO_PWM3_OUT,            \
	}

// Console buffer - ENABLED: lazy initialization implemented in console_buffer.cpp
// (ensure_initialized() with double-checked locking avoids static init issues)
#define BOARD_ENABLE_CONSOLE_BUFFER

/* Number of IO timers used for PWM (one timer per PWM channel) */
#define BOARD_NUM_IO_TIMERS 3

__BEGIN_DECLS

/****************************************************************************************************
 * Public Types
 ****************************************************************************************************/

/****************************************************************************************************
 * Public data
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

extern void sam_usbinitialize(void);

extern void board_peripheral_reset(int ms);

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS

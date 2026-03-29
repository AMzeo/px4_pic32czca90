/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * PIC32CZ CA90 Curiosity Ultra internal definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#include "sam_port.h"
#include "hardware/sam_pinmap.h"

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* LED - PC21 (active high on Curiosity Ultra) */

#define GPIO_nLED_BLUE    PORT_LED0   /* PC21 */

#define BOARD_HAS_CONTROL_STATUS_LEDS      1
#define BOARD_ARMED_STATE_LED  LED_BLUE

/* I2C - not yet wired, but PX4 board_common.h requires this */
#define PX4_NUMBER_I2C_BUSES    1
#define BOARD_NUMBER_I2C_BUSES  1

/* No ADC / battery monitoring yet — use digital brick to avoid ADC dependency */
#define BOARD_NUMBER_BRICKS          1
#define BOARD_NUMBER_DIGITAL_BRICKS  1

/* No sensors, PWM, QSPI, SD card configured yet.
 * These will be added as peripheral drivers are ported.
 */

/* No PWM channels */
#define DIRECT_PWM_OUTPUT_CHANNELS  0

/* High-resolution timer - uses DWT cycle counter (see platform hrt.c) */
#define HRT_TIMER               0
#define HRT_TIMER_CHANNEL       0

/* No DMA pool for now */
#define BOARD_DMA_ALLOC_POOL_SIZE 0

/* Board provides on_reset */
#define BOARD_HAS_ON_RESET 1

#define PX4_GPIO_INIT_LIST { \
		GPIO_nLED_BLUE, \
	}

/* Console buffer */
#define BOARD_ENABLE_CONSOLE_BUFFER

/* No IO timers for PWM yet */
#define BOARD_NUM_IO_TIMERS 0

__BEGIN_DECLS

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

extern void board_peripheral_reset(int ms);

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS

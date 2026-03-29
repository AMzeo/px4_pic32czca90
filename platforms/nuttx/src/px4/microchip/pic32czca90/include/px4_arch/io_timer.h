/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/* Stub io_timer.h for PIC32CZ CA90 - PWM not yet implemented */

#pragma once

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>

#include <stdint.h>

/* Timer IO Channel - stub definition */
typedef struct {
	uint32_t gpio_out;
	uint32_t gpio_in;
} timer_io_channels_t;

/* Timer IO - stub definition */
typedef struct {
	uint32_t base;
} io_timers_t;

__BEGIN_DECLS

#define MAX_TIMER_IO_CHANNELS 0

extern const io_timers_t io_timers[];
extern const timer_io_channels_t timer_io_channels[];

__END_DECLS

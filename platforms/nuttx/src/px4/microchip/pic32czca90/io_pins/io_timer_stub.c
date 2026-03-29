/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file io_timer_stub.c
 *
 * Stub IO timer / PWM implementation for PIC32CZ CA90.
 * PWM output is not yet implemented on this platform.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_arch/io_timer.h>
#include <nuttx/arch.h>
#include <errno.h>

const io_timers_t io_timers[1] = {};
const timer_io_channels_t timer_io_channels[1] = {};

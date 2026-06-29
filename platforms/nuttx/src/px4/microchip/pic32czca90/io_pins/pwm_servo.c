/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file pwm_servo.c
 *
 * PWM servo driver for PIC32CZ CA90 TCC-based PWM output.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>

#include <arch/board/board.h>
#include <drivers/drv_pwm_output.h>

#include <px4_arch/io_timer.h>

int up_pwm_servo_set(unsigned channel, uint16_t value)
{
	return io_timer_set_ccr(channel, value);
}

uint16_t up_pwm_servo_get(unsigned channel)
{
	return io_channel_get_ccr(channel);
}

int up_pwm_servo_init(uint32_t channel_mask)
{
	uint32_t current = io_timer_get_mode_channels(IOTimerChanMode_PWMOut) |
			   io_timer_get_mode_channels(IOTimerChanMode_OneShot);

	for (unsigned channel = 0; current != 0 && channel < MAX_TIMER_IO_CHANNELS; channel++) {
		if (current & (1 << channel)) {
			io_timer_set_enable(false, IOTimerChanMode_PWMOut, 1 << channel);
			io_timer_unallocate_channel(channel);
			current &= ~(1 << channel);
		}
	}

	int ret_val = OK;
	int channels_init_mask = 0;

	for (unsigned channel = 0; channel_mask != 0 && channel < MAX_TIMER_IO_CHANNELS; channel++) {
		if (channel_mask & (1 << channel)) {
			ret_val = io_timer_channel_init(channel, IOTimerChanMode_PWMOut, NULL, NULL);
			channel_mask &= ~(1 << channel);

			if (OK == ret_val) {
				channels_init_mask |= 1 << channel;

			} else if (ret_val == -EBUSY) {
				ret_val = 0;
			}
		}
	}

	return ret_val == OK ? channels_init_mask : ret_val;
}

void up_pwm_servo_deinit(uint32_t channel_mask)
{
	up_pwm_servo_arm(false, channel_mask);
}

int up_pwm_servo_set_rate_group_update(unsigned group, unsigned rate)
{
	if ((group >= MAX_IO_TIMERS) || (io_timers[group].base == 0)) {
		return ERROR;
	}

	if (rate != PWM_RATE_ONESHOT) {
		if ((rate < PWM_RATE_LOWER_LIMIT) || (rate > PWM_RATE_UPPER_LIMIT)) {
			return -ERANGE;
		}
	}

	return io_timer_set_pwm_rate(group, rate);
}

void up_pwm_update(unsigned channels_mask)
{
	io_timer_trigger(channels_mask);
}

uint32_t up_pwm_servo_get_rate_group(unsigned group)
{
	return (io_timer_get_mode_channels(IOTimerChanMode_PWMOut) |
		io_timer_get_mode_channels(IOTimerChanMode_OneShot)) &
	       io_timer_get_group(group);
}

void up_pwm_servo_arm(bool armed, uint32_t channel_mask)
{
	io_timer_set_enable(armed, IOTimerChanMode_OneShot, channel_mask);
	io_timer_set_enable(armed, IOTimerChanMode_PWMOut, channel_mask);
}

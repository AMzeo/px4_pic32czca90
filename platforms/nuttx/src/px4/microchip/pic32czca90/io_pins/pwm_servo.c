/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file pwm_servo.c
 *
 * Stub PWM servo driver for PIC32CZ CA90.
 * PWM output is not yet implemented on this platform.
 */

#include <px4_platform_common/px4_config.h>
#include <stdint.h>
#include <errno.h>

int up_pwm_servo_init(uint32_t channel_mask)
{
	(void)channel_mask;
	return -ENOSYS;
}

void up_pwm_servo_deinit(uint32_t channel_mask)
{
	(void)channel_mask;
}

int up_pwm_servo_set(unsigned channel, uint16_t value)
{
	(void)channel;
	(void)value;
	return -ENOSYS;
}

uint16_t up_pwm_servo_get(unsigned channel)
{
	(void)channel;
	return 0;
}

int up_pwm_servo_set_rate(unsigned rate)
{
	(void)rate;
	return -ENOSYS;
}

uint32_t up_pwm_servo_get_rate_group(unsigned group)
{
	(void)group;
	return 0;
}

void up_pwm_servo_arm(bool armed, uint32_t channel_mask)
{
	(void)armed;
	(void)channel_mask;
}

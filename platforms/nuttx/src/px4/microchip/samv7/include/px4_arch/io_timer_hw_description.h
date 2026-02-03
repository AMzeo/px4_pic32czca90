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

#pragma once

/**
 * @file io_timer_hw_description.h
 *
 * SAMV7 Timer/Counter hardware description for PWM output
 */

#include <px4_arch/io_timer.h>
#include <px4_arch/hw_description.h>
#include <px4_platform_common/constexpr_util.h>

#define initIOTimerChannelCapture initIOTimerChannel  /* alias for param metadata generation */

/**
 * Initialize an IO timer block
 */
static inline constexpr io_timers_t initIOTimer(Timer::Timer timer)
{
	io_timers_t ret{};

	/* Get TC block base address based on timer number */
	switch (timer) {
	case Timer::Timer1:
	case Timer::Timer2:
		ret.base = SAM_TC012_BASE;  /* TC block 0 */
		break;

	case Timer::Timer3:
	case Timer::Timer4:
	case Timer::Timer5:
		ret.base = SAM_TC345_BASE;  /* TC block 1 */
		break;
	}

	ret.clock_register = 0;  /* Handled by io_timer_tc.c */
	ret.clock_bit = 0;
	ret.vectorno = 0;

	return ret;
}

/**
 * Initialize a timer channel with GPIO pin mapping
 */
static inline constexpr timer_io_channels_t initIOTimerChannel(const io_timers_t io_timers_conf[MAX_IO_TIMERS],
		Timer::TimerChannel timer, GPIO::GPIOPin pin)
{
	timer_io_channels_t ret{};

	/* Set GPIO configuration for TC output (Peripheral B for TC)
	 * NuttX GPIO encoding for SAMV7:
	 * - Bits 21-23: Mode (GPIO_PERIPHB = 4 << 21)
	 * - Bits 16-20: Config (GPIO_CFG_DEFAULT = 0)
	 * - Bits 5-7:   Port (PIOA=0, PIOB=1, PIOC=2, PIOD=3, PIOE=4)
	 * - Bits 0-4:   Pin number (0-31)
	 */
	uint32_t gpio_mode = (4 << 21);  /* GPIO_PERIPHB */
	uint32_t gpio_cfg = (0 << 16);   /* GPIO_CFG_DEFAULT */
	uint32_t gpio_port = ((uint32_t)pin.port << 5);
	uint32_t gpio_pin = (uint32_t)pin.pin;

	ret.gpio_out = gpio_mode | gpio_cfg | gpio_port | gpio_pin;
	ret.gpio_in = 0;

	/* Find timer index and channel
	 * timer_index must match position in io_timers[] array
	 * timer_channel is the channel offset within the TC block for address calculation
	 */
	ret.timer_index = 0xff;

	switch (timer.timer) {
	case Timer::Timer1:
		ret.timer_index = 0;    /* io_timers[0] */
		ret.timer_channel = 1;  /* TC0 CH1 */
		break;

	case Timer::Timer2:
		ret.timer_index = 1;    /* io_timers[1] */
		ret.timer_channel = 2;  /* TC0 CH2 */
		break;

	case Timer::Timer3:
		ret.timer_index = 2;    /* io_timers[2] */
		ret.timer_channel = 0;  /* TC1 CH0 */
		break;

	case Timer::Timer4:
		ret.timer_index = 3;    /* io_timers[3] */
		ret.timer_channel = 1;  /* TC1 CH1 */
		break;

	case Timer::Timer5:
		ret.timer_index = 4;    /* io_timers[4] if used */
		ret.timer_channel = 2;  /* TC1 CH2 */
		break;
	}

	constexpr_assert(ret.timer_index != 0xff, "Timer not found");

	return ret;
}

/**
 * Timer channel mapping initialization
 */
struct io_timers_channel_mapping_t {
	uint32_t element[MAX_IO_TIMERS];
};

static inline constexpr io_timers_channel_mapping_t initIOTimerChannelMapping(const io_timers_t io_timers_conf[MAX_IO_TIMERS],
		const timer_io_channels_t timer_io_channels_conf[MAX_TIMER_IO_CHANNELS])
{
	io_timers_channel_mapping_t ret{};

	for (int i = 0; i < MAX_IO_TIMERS; ++i) {
		ret.element[i] = 0;
	}

	for (int i = 0; i < MAX_TIMER_IO_CHANNELS; ++i) {
		if (timer_io_channels_conf[i].timer_index < MAX_IO_TIMERS) {
			ret.element[timer_io_channels_conf[i].timer_index] |= (1 << i);
		}
	}

	return ret;
}

/*******************************************************************************
 * PWMC (PWM Controller) Helper Functions
 *
 * SAMV7 PWMC provides hardware PWM with 4 channels per module.
 * - PWM0: PID 31, base 0x40020000
 * - PWM1: PID 60, base 0x4005C000
 ******************************************************************************/

/**
 * Initialize an IO timer for PWMC (PWM Controller)
 * @param module PWM0 or PWM1
 */
static inline constexpr io_timers_t initIOPWMTimer(PWM::PWMModule module)
{
	io_timers_t ret{};
	ret.base = pwmBaseRegister(module);
	ret.clock_register = 0;  /* Clock enable handled by io_timer_pwmc.c */
	ret.clock_bit = 0;
	ret.vectorno = 0;
	return ret;
}

/**
 * Initialize a PWMC channel with GPIO pin mapping
 * @param io_timers_conf Array of io_timers_t (for future validation)
 * @param pwm_channel PWM module and channel (e.g., {PWM0, Channel0})
 * @param pin GPIO port and pin (e.g., {PortB, Pin0})
 * @param periph Peripheral function A or B (pin-dependent, see samv71_pinmap.h)
 */
static inline constexpr timer_io_channels_t initIOPWMChannel(const io_timers_t io_timers_conf[MAX_IO_TIMERS],
		PWM::PWMChannel pwm_channel, GPIO::GPIOPin pin, PWMCPeripheral periph)
{
	timer_io_channels_t ret{};

	/* GPIO configuration: Peripheral A (3) or B (4) depending on pin
	 * NuttX GPIO encoding for SAMV7:
	 * - Bits 21-23: Mode (GPIO_PERIPHA = 3 << 21, GPIO_PERIPHB = 4 << 21)
	 * - Bits 16-20: Config (GPIO_CFG_DEFAULT = 0)
	 * - Bits 5-7:   Port (PIOA=0, PIOB=1, PIOC=2, PIOD=3, PIOE=4)
	 * - Bits 0-4:   Pin number (0-31)
	 */
	uint32_t gpio_mode = (periph == PWMCPeripheral::A) ? (3 << 21) : (4 << 21);
	uint32_t gpio_cfg = (0 << 16);  /* GPIO_CFG_DEFAULT */
	uint32_t gpio_port = ((uint32_t)pin.port << 5);
	uint32_t gpio_pin = (uint32_t)pin.pin;

	ret.gpio_out = gpio_mode | gpio_cfg | gpio_port | gpio_pin;
	ret.gpio_in = 0;

	/* Derive timer_index from PWM module
	 * This maps to position in io_timers[] array:
	 * PWM0 = index 0, PWM1 = index 1
	 */
	ret.timer_index = (uint8_t)pwm_channel.module;

	/* Channel within PWMC (0-3) for register offset calculation */
	ret.timer_channel = (uint8_t)pwm_channel.channel;

	return ret;
}

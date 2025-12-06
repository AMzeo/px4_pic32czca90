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

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <sam_tc.h>
#include <stdint.h>

/* Maximum number of IO timers (one per TC channel used for PWM) */
#ifndef MAX_IO_TIMERS
#define MAX_IO_TIMERS  4  /* Timer1-4 for 4 PWM channels */
#endif

/* Maximum number of timer channels across all timers */
#ifndef MAX_TIMER_IO_CHANNELS
#define MAX_TIMER_IO_CHANNELS  4  /* 2 channels from TC0 + 2 channels from TC1 */
#endif

/* Forward declarations for types */
typedef uint8_t io_timer_channel_mode_t;
typedef uint16_t io_timer_channel_allocation_t;
typedef int (*channel_handler_t)(void *context);

__BEGIN_DECLS

/* SAMV7 IO Timer Hardware Description
 * SAMV7 uses TC (Timer/Counter) peripherals for PWM generation
 * TC0, TC1, TC2 - each has 3 channels
 */

/* Timer/PWM configuration types */
#define IOTimerChanMode_NotUsed         0
#define IOTimerChanMode_PWMOut          1
#define IOTimerChanMode_PWMIn           2
#define IOTimerChanMode_Capture         3
#define IOTimerChanMode_OneShot         4
#define IOTimerChanMode_Trigger         5
#define IOTimerChanMode_PPS             6

/* Timer channel allocation structure */
typedef struct io_timers_t {
	uint32_t  base;             /* Timer base address */
	uint32_t  clock_register;   /* Clock enable register */
	uint32_t  clock_bit;        /* Clock enable bit */
	uint32_t  vectorno;         /* Interrupt vector number */
} io_timers_t;

typedef struct timer_io_channels_t {
	uint32_t    gpio_out;       /* GPIO configuration for output */
	uint32_t    gpio_in;        /* GPIO configuration for input */
	uint8_t     timer_index;    /* Index in io_timers_t array */
	uint8_t     timer_channel;  /* Timer channel number (0-2) */
} timer_io_channels_t;

/* Functions */
__EXPORT extern int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
		channel_handler_t channel_handler, void *context);

__EXPORT extern int io_timer_init_timer(unsigned timer);

__EXPORT extern int io_timer_set_rate(unsigned timer, unsigned rate);

__EXPORT extern int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
		io_timer_channel_allocation_t masks);

__EXPORT extern int io_timer_set_ccr(unsigned channel, uint16_t value);

__EXPORT extern uint16_t io_channel_get_ccr(unsigned channel);

__EXPORT extern uint32_t io_timer_get_group(unsigned timer);

__EXPORT extern int io_timer_validate_channel_index(unsigned channel);

__EXPORT extern int io_timer_is_channel_free(unsigned channel);

__EXPORT extern int io_timer_free_channel(unsigned channel);

__EXPORT extern int io_timer_get_channel_mode(unsigned channel);

__EXPORT extern int io_timer_get_mode_channels(io_timer_channel_mode_t mode);

__EXPORT extern const io_timers_t io_timers[MAX_IO_TIMERS];

__EXPORT extern const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS];

__EXPORT extern uint32_t io_timer_channel_get_as_pwm_input(unsigned channel);

__EXPORT extern int io_timer_unallocate_channel(unsigned channel);

__EXPORT extern int io_timer_set_pwm_rate(unsigned timer, unsigned rate);

__EXPORT extern void io_timer_trigger(unsigned channels_mask);

__END_DECLS

/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file io_timer.h
 *
 * PIC32CZ CA90 IO Timer API — TCC-based PWM output.
 *
 * Uses TCC peripherals in Normal PWM mode (WAVE=NPWM). Each TCC instance
 * provides up to 8 independent waveform outputs with glitch-free duty
 * updates via CCBUF registers.
 */

#pragma once

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <drivers/drv_hrt.h>
#include <stdint.h>

__BEGIN_DECLS

#ifdef BOARD_NUM_IO_TIMERS
#define MAX_IO_TIMERS		BOARD_NUM_IO_TIMERS
#else
#define MAX_IO_TIMERS		2
#endif

#if defined(DIRECT_PWM_OUTPUT_CHANNELS) && DIRECT_PWM_OUTPUT_CHANNELS > 0
#define MAX_TIMER_IO_CHANNELS	DIRECT_PWM_OUTPUT_CHANNELS
#else
#define MAX_TIMER_IO_CHANNELS	4
#endif

#define MAX_LED_TIMERS		0
#define MAX_TIMER_LED_CHANNELS	0

#define IO_TIMER_ALL_MODES_CHANNELS 0

typedef enum io_timer_channel_mode_t {
	IOTimerChanMode_NotUsed         = 0,
	IOTimerChanMode_PWMOut          = 1,
	IOTimerChanMode_PWMIn           = 2,
	IOTimerChanMode_Capture         = 3,
	IOTimerChanMode_OneShot         = 4,
	IOTimerChanMode_Trigger         = 5,
	IOTimerChanMode_Dshot           = 6,
	IOTimerChanMode_LED             = 7,
	IOTimerChanMode_PPS             = 8,
	IOTimerChanMode_RPM             = 9,
	IOTimerChanMode_Other           = 10,
	IOTimerChanMode_DshotInverted   = 11,
	IOTimerChanMode_CaptureDMA      = 12,
	IOTimerChanModeSize
} io_timer_channel_mode_t;

typedef uint16_t io_timer_channel_allocation_t;

/* TCC module descriptor */
typedef struct io_timers_t {
	uint32_t base;           /* TCC base address */
	uint32_t gclk_id;       /* GCLK peripheral channel ID */
	uint32_t mclk_id;       /* MCLK APB clock ID */
	uint32_t clock_freq;    /* GCLK input frequency (Hz) */
	uint32_t first_irq;     /* First MC IRQ number (SAM_IRQ_TCCnMC0) */
	uint8_t  cc_num;        /* Number of CC channels available */
} io_timers_t;

/* Channel-to-timer mapping element */
typedef struct io_timers_channel_mapping_element_t {
	uint32_t first_channel_index;
	uint32_t channel_count;
	uint32_t lowest_timer_channel;
	uint32_t channel_count_including_gaps;
} io_timers_channel_mapping_element_t;

typedef struct io_timers_channel_mapping_t {
	io_timers_channel_mapping_element_t element[MAX_IO_TIMERS];
} io_timers_channel_mapping_t;

/* Channel descriptor */
typedef struct timer_io_channels_t {
	uint32_t gpio_out;       /* Pin config for WO output */
	uint32_t gpio_in;        /* Pin config for input capture (future) */
	uint8_t  timer_index;    /* Index into io_timers[] */
	uint8_t  timer_channel;  /* CC channel number (0-7) */
	uint16_t masks;          /* Channel bit (1 << timer_channel) */
} timer_io_channels_t;

typedef void (*channel_handler_t)(void *context, const io_timers_t *timer,
				  uint32_t chan_index, const timer_io_channels_t *chan,
				  hrt_abstime isrs_time, uint16_t isrs_rcnt);

/* Board-supplied data */
__EXPORT extern const io_timers_t io_timers[MAX_IO_TIMERS];
__EXPORT extern const io_timers_channel_mapping_t io_timers_channel_mapping;
__EXPORT extern const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS];

/* Timer allocation */
__EXPORT int io_timer_allocate_timer(unsigned timer, io_timer_channel_mode_t mode);
__EXPORT int io_timer_unallocate_timer(unsigned timer);

/* Channel allocation */
__EXPORT int io_timer_allocate_channel(unsigned channel, io_timer_channel_mode_t mode);
__EXPORT int io_timer_unallocate_channel(unsigned channel);

/* Initialization */
__EXPORT int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode);
__EXPORT int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
				   channel_handler_t channel_handler, void *context);

/* Rate and duty cycle */
__EXPORT int io_timer_set_pwm_rate(unsigned timer, unsigned rate);
__EXPORT int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
				 io_timer_channel_allocation_t masks);
__EXPORT int io_timer_set_ccr(unsigned channel, uint16_t value);
__EXPORT uint16_t io_channel_get_ccr(unsigned channel);

/* Channel queries */
__EXPORT uint32_t io_timer_get_group(unsigned timer);
__EXPORT int io_timer_validate_channel_index(unsigned channel);
__EXPORT int io_timer_is_channel_free(unsigned channel);
__EXPORT int io_timer_free_channel(unsigned channel);
__EXPORT int io_timer_get_channel_mode(unsigned channel);
__EXPORT int io_timer_get_mode_channels(io_timer_channel_mode_t mode);

/* GPIO helpers */
__EXPORT uint32_t io_timer_channel_get_gpio_output(unsigned channel);
__EXPORT uint32_t io_timer_channel_get_as_pwm_input(unsigned channel);

/* OneShot trigger */
__EXPORT void io_timer_trigger(unsigned channels_mask);

/* DShot (stub — not implemented) */
__EXPORT void io_timer_update_dma_req(uint8_t timer, bool enable);
__EXPORT int io_timer_set_dshot_mode(uint8_t timer, unsigned dshot_pwm_freq);

__END_DECLS

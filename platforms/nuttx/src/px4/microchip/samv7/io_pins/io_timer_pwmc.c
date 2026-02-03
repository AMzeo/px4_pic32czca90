/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the distribution.
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
 * @file io_timer_pwmc.c
 *
 * SAMV7 IO Timer implementation using PWMC (PWM Controller) for motor PWM.
 *
 * PWMC provides 4 independent channels per module (PWM0, PWM1).
 * Each channel has:
 * - CPRD: Period register (determines PWM frequency)
 * - CDTY: Duty cycle register
 * - CDTYUPD: Duty cycle update register (glitch-free updates)
 *
 * Clock configuration:
 * - MCK = 150MHz
 * - CPRE = 3 (MCK/8 = 18.75MHz)
 * - For 400Hz: CPRD = 18750000/400 = 46875
 *
 * Pin mapping (Option A):
 * - Motor 1 (CH3): PA7  - GPIO_PWMC0_H3 (Peripheral B)
 * - Motor 2 (CH1): PA2  - GPIO_PWMC0_H1 (Peripheral A)
 * - Motor 3 (CH2): PC19 - GPIO_PWMC0_H2 (Peripheral B)
 * - Motor 4 (CH0): PB0  - GPIO_PWMC0_H0 (Peripheral A)
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <drivers/drv_pwm_output.h>

#include <px4_arch/io_timer.h>
#include <board_config.h>

#include "arm_internal.h"
#include "hardware/sam_pwm.h"
#include "hardware/sam_pmc.h"
#include "sam_gpio.h"
#include "sam_periphclks.h"

/* IRQ header for peripheral IDs */
#include <arch/chip/irq.h>

/* PWMC register offsets - channel registers */
#define PWM_CMR_OFFSET      0x200   /* Channel Mode Register base */
#define PWM_CDTY_OFFSET     0x204   /* Channel Duty Cycle Register base */
#define PWM_CDTYUPD_OFFSET  0x208   /* Channel Duty Cycle Update Register base */
#define PWM_CPRD_OFFSET     0x20C   /* Channel Period Register base */
#define PWM_CPRDUPD_OFFSET  0x210   /* Channel Period Update Register base */
#define PWM_CCNT_OFFSET     0x214   /* Channel Counter Register base */

/* Channel spacing in register map */
#define PWM_CHAN_SPACING    0x20    /* 32 bytes per channel */

/* Global PWMC registers */
#define PWM_CLK_OFFSET      0x000   /* Clock Register */
#define PWM_ENA_OFFSET      0x004   /* Enable Register */
#define PWM_DIS_OFFSET      0x008   /* Disable Register */
#define PWM_SR_OFFSET       0x00C   /* Status Register */

/* Clock configuration:
 * MCK = 150MHz, using CPRE=3 (MCK/8) gives 18.75MHz PWM clock
 * For 400Hz PWM: CPRD = 18750000 / 400 = 46875
 */
#define PWM_DEFAULT_RATE        400
#define PWM_CLOCK_DIVIDER       8       /* CPRE = 3 means MCK/8 */
#define MCK_FREQUENCY           150000000UL
#define PWM_CLOCK_FREQ          (MCK_FREQUENCY / PWM_CLOCK_DIVIDER)

/* Channel Mode Register (CMR) settings:
 * CPRE = 3: MCK/8 prescaler
 * CALG = 0: Left-aligned (edge-aligned)
 * CPOL = 1: Output starts high, goes low at duty match (normal polarity for ESCs)
 */
#define PWM_CMR_CPRE_MCK8       (3 << 0)    /* CPRE = 3 for MCK/8 */
#define PWM_CMR_CPOL            (1 << 9)    /* Channel polarity: high at start */

/* Channel state tracking */
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static bool g_timers_initialized[MAX_IO_TIMERS];
static uint32_t g_timer_period[MAX_IO_TIMERS];  /* CPRD value for each timer */

/* Enable peripheral clock for PWMC module via PMC
 * PWM0: PID 31 -> PCER0 bit 31
 * PWM1: PID 60 -> PCER1 bit 28 (60 - 32)
 */
static void enable_pwm_clock(unsigned timer)
{
	if (timer == 0) {
		/* PWM0: PID 31, use PCER0 (PIDs 0-31) */
		putreg32((1 << SAM_PID_PWM0), SAM_PMC_PCER0);

	} else if (timer == 1) {
		/* PWM1: PID 60, use PCER1 (PIDs 32-63) */
		putreg32((1 << (SAM_PID_PWM1 - 32)), SAM_PMC_PCER1);
	}
}

/* Helper to get PWMC base address for a timer (PWM module) */
static inline uint32_t get_pwm_base(unsigned timer)
{
	if (timer >= MAX_IO_TIMERS) {
		return 0;
	}

	return io_timers[timer].base;
}

/* Helper to get channel register address
 * channel = index into timer_io_channels[]
 * Returns the base address for channel-specific registers
 */
static inline uint32_t get_channel_reg_base(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return 0;
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint8_t pwm_ch = timer_io_channels[channel].timer_channel;  /* 0-3 */

	uint32_t base = get_pwm_base(timer_idx);

	if (base == 0) {
		return 0;
	}

	/* Channel registers start at offset 0x200, spacing of 0x20 per channel */
	return base + PWM_CMR_OFFSET + (pwm_ch * PWM_CHAN_SPACING);
}

/* Register access helpers */
static inline void pwm_putreg(uint32_t addr, uint32_t value)
{
	putreg32(value, addr);
}

static inline uint32_t pwm_getreg(uint32_t addr)
{
	return getreg32(addr);
}

/* Channel-specific register helpers */
static inline void pwm_ch_putreg(uint32_t ch_base, uint32_t offset, uint32_t value)
{
	/* ch_base already points to CMR, adjust offset relative to CMR */
	putreg32(value, ch_base + (offset - PWM_CMR_OFFSET));
}

static inline uint32_t pwm_ch_getreg(uint32_t ch_base, uint32_t offset)
{
	return getreg32(ch_base + (offset - PWM_CMR_OFFSET));
}

/**
 * Initialize a timer block (PWMC module)
 */
int io_timer_init_timer(unsigned timer)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	if (g_timers_initialized[timer]) {
		return OK;
	}

	/* Enable peripheral clock for this PWMC module */
	enable_pwm_clock(timer);

	/* Set default period for 400Hz PWM */
	g_timer_period[timer] = PWM_CLOCK_FREQ / PWM_DEFAULT_RATE;

	g_timers_initialized[timer] = true;

	return OK;
}

/**
 * Initialize a PWM channel
 */
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
			  channel_handler_t channel_handler, void *context)
{
	(void)channel_handler;
	(void)context;

	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_NotUsed) {
		return -EINVAL;  /* Only PWM output supported */
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint8_t pwm_ch = timer_io_channels[channel].timer_channel;

	/* Initialize the timer block if needed */
	int ret = io_timer_init_timer(timer_idx);

	if (ret != OK) {
		return ret;
	}

	uint32_t base = get_pwm_base(timer_idx);
	uint32_t ch_base = get_channel_reg_base(channel);

	if (base == 0 || ch_base == 0) {
		return -EINVAL;
	}

	if (mode == IOTimerChanMode_PWMOut) {
		uint32_t gpio = timer_io_channels[channel].gpio_out;

		/* Configure GPIO for PWMC output (peripheral A or B) */
		sam_configgpio(gpio);

		/* Disable channel first (write to DIS register) */
		pwm_putreg(base + PWM_DIS_OFFSET, (1 << pwm_ch));

		/* Configure Channel Mode Register:
		 * - CPRE = 3 (MCK/8 = 18.75MHz)
		 * - CALG = 0 (left-aligned)
		 * - CPOL = 1 (high at period start, low at duty match)
		 */
		pwm_ch_putreg(ch_base, PWM_CMR_OFFSET, PWM_CMR_CPRE_MCK8 | PWM_CMR_CPOL);

		/* Set period (CPRD register) */
		pwm_ch_putreg(ch_base, PWM_CPRD_OFFSET, g_timer_period[timer_idx]);

		/* Set initial duty cycle to 0 (CDTY register)
		 * Note: With CPOL=1, CDTY=0 means output stays high entire period
		 * For motor safety, we want 0 duty initially
		 */
		pwm_ch_putreg(ch_base, PWM_CDTY_OFFSET, 0);

		/* Enable channel (write to ENA register) */
		pwm_putreg(base + PWM_ENA_OFFSET, (1 << pwm_ch));
	}

	g_channel_modes[channel] = mode;

	return OK;
}

/**
 * Set PWM rate (frequency) for a timer
 */
int io_timer_set_rate(unsigned timer, unsigned rate)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	if (rate < 50 || rate > 8000) {
		return -EINVAL;
	}

	/* Calculate new period */
	uint32_t period = PWM_CLOCK_FREQ / rate;
	g_timer_period[timer] = period;

	/* Update CPRD for all channels using this timer
	 * Use CPRDUPD for glitch-free update (takes effect at next period boundary)
	 */
	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if (timer_io_channels[ch].timer_index == timer &&
		    g_channel_modes[ch] == IOTimerChanMode_PWMOut) {
			uint32_t ch_base = get_channel_reg_base(ch);
			pwm_ch_putreg(ch_base, PWM_CPRDUPD_OFFSET, period);
		}
	}

	return OK;
}

/**
 * Enable/disable PWM channels
 */
int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
			io_timer_channel_allocation_t masks)
{
	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if ((masks & (1 << ch)) && g_channel_modes[ch] == mode) {
			uint8_t timer_idx = timer_io_channels[ch].timer_index;
			uint8_t pwm_ch = timer_io_channels[ch].timer_channel;
			uint32_t base = get_pwm_base(timer_idx);

			if (state) {
				/* Enable channel */
				pwm_putreg(base + PWM_ENA_OFFSET, (1 << pwm_ch));

			} else {
				/* Disable channel */
				pwm_putreg(base + PWM_DIS_OFFSET, (1 << pwm_ch));
			}
		}
	}

	return OK;
}

/**
 * Set PWM duty cycle (CCR = Compare Capture Register, here CDTY)
 * Value is in microseconds (1000-2000 typical for servos/ESCs)
 */
int io_timer_set_ccr(unsigned channel, uint16_t value)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	if (g_channel_modes[channel] != IOTimerChanMode_PWMOut) {
		return -EINVAL;
	}

	uint32_t ch_base = get_channel_reg_base(channel);

	/* Convert microseconds to timer ticks */
	uint32_t ticks = (uint32_t)value * PWM_CLOCK_FREQ / 1000000UL;

	/* Clamp to period */
	uint8_t timer_idx = timer_io_channels[channel].timer_index;

	if (ticks > g_timer_period[timer_idx]) {
		ticks = g_timer_period[timer_idx];
	}

	/* Use CDTYUPD for glitch-free update (Harmony CSP pattern)
	 * The new duty value takes effect at the next period boundary
	 */
	pwm_ch_putreg(ch_base, PWM_CDTYUPD_OFFSET, ticks);

	return OK;
}

/**
 * Get current CCR (duty cycle) value
 */
uint16_t io_channel_get_ccr(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return 0;
	}

	uint32_t ch_base = get_channel_reg_base(channel);
	uint32_t ticks = pwm_ch_getreg(ch_base, PWM_CDTY_OFFSET);

	/* Convert back to microseconds */
	return (uint16_t)(ticks * 1000000UL / PWM_CLOCK_FREQ);
}

/**
 * Get channel group bitmask for a timer
 */
uint32_t io_timer_get_group(unsigned timer)
{
	if (timer >= MAX_IO_TIMERS) {
		return 0;
	}

	uint32_t mask = 0;

	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if (timer_io_channels[ch].timer_index == timer) {
			mask |= (1 << ch);
		}
	}

	return mask;
}

/**
 * Validate channel index
 */
int io_timer_validate_channel_index(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	return OK;
}

/**
 * Check if channel is free
 */
int io_timer_is_channel_free(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	return g_channel_modes[channel] == IOTimerChanMode_NotUsed ? 0 : -EBUSY;
}

/**
 * Free a channel
 */
int io_timer_free_channel(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint8_t pwm_ch = timer_io_channels[channel].timer_channel;
	uint32_t base = get_pwm_base(timer_idx);

	/* Disable the channel */
	pwm_putreg(base + PWM_DIS_OFFSET, (1 << pwm_ch));

	g_channel_modes[channel] = IOTimerChanMode_NotUsed;

	return OK;
}

/**
 * Get channel mode
 */
int io_timer_get_channel_mode(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return IOTimerChanMode_NotUsed;
	}

	return g_channel_modes[channel];
}

/**
 * Get bitmask of channels in a specific mode
 */
int io_timer_get_mode_channels(io_timer_channel_mode_t mode)
{
	int mask = 0;

	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if (g_channel_modes[ch] == mode) {
			mask |= (1 << ch);
		}
	}

	return mask;
}

/**
 * Get GPIO configuration for PWM input (not used for PWMC output)
 */
uint32_t io_timer_channel_get_as_pwm_input(unsigned channel)
{
	(void)channel;
	return 0;  /* PWM input not implemented for PWMC */
}

/**
 * Unallocate a channel (alias for io_timer_free_channel)
 */
int io_timer_unallocate_channel(unsigned channel)
{
	return io_timer_free_channel(channel);
}

/**
 * Set PWM rate for a timer (alias for io_timer_set_rate)
 */
int io_timer_set_pwm_rate(unsigned timer, unsigned rate)
{
	return io_timer_set_rate(timer, rate);
}

/**
 * Trigger PWM update on specified channels
 * For PWMC with CDTYUPD/CPRDUPD, updates are automatic at period boundary.
 * This function is a no-op for synchronous mode.
 */
void io_timer_trigger(unsigned channels_mask)
{
	/* PWMC channels using CDTYUPD update automatically at the next period
	 * boundary. No explicit trigger needed unlike some STM32 implementations.
	 */
	(void)channels_mask;
}

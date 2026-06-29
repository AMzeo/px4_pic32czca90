/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file io_timer_tcc.c
 *
 * PIC32CZ CA90 IO Timer implementation using TCC (Timer/Counter for Control).
 *
 * TCC provides up to 8 independent waveform outputs per instance. Each channel
 * has buffered CC registers (CCBUF) for glitch-free duty updates at period
 * boundary. The 32-bit PER register eliminates prescaler switching.
 *
 * Clock: GCLK7 = 100 MHz (PLL0/3), no prescaler (DIV1).
 * PWM mode: NPWM (Normal PWM) — output HIGH while COUNT < CC[n].
 * Duty update: write CCBUF[n] — applies at next TOP (PER match).
 * Rate change: write PERBUF — applies at next TOP.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
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
#include "hardware/sam_tcc.h"
#include "hardware/pic32czca90_memorymap.h"
#include "hardware/sam_gclk.h"
#include "hardware/sam_mclk.h"
#include "hardware/sam_supc.h"
#include "sam_port.h"

#define PWM_DEFAULT_RATE  400

static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static bool g_timers_initialized[MAX_IO_TIMERS];
static io_timer_channel_mode_t g_timer_modes[MAX_IO_TIMERS];
static uint32_t g_timer_period[MAX_IO_TIMERS];

static channel_handler_t g_channel_handler_callbacks[MAX_TIMER_IO_CHANNELS];
static void             *g_channel_handler_contexts[MAX_TIMER_IO_CHANNELS];

static inline void tcc_putreg(uint32_t base, uint32_t offset, uint32_t value)
{
	putreg32(value, base + offset);
}

static inline uint32_t tcc_getreg(uint32_t base, uint32_t offset)
{
	return getreg32(base + offset);
}

static inline int tcc_wait_syncbusy(uint32_t base, uint32_t mask)
{
	volatile uint32_t timeout = 300000u;

	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & mask) {
		if (--timeout == 0) {
			return -ETIMEDOUT;
		}
	}

	return 0;
}

int io_timer_allocate_timer(unsigned timer, io_timer_channel_mode_t mode)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	irqstate_t flags = enter_critical_section();

	if (g_timers_initialized[timer] && g_timer_modes[timer] != mode) {
		leave_critical_section(flags);
		return -EBUSY;
	}

	g_timer_modes[timer] = mode;
	leave_critical_section(flags);

	return OK;
}

int io_timer_unallocate_timer(unsigned timer)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	g_timer_modes[timer] = IOTimerChanMode_NotUsed;
	g_timers_initialized[timer] = false;

	return OK;
}

int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	if (g_timers_initialized[timer]) {
		return OK;
	}

	int ret = io_timer_allocate_timer(timer, mode);

	if (ret != OK) {
		return ret;
	}

	uint32_t clock_freq = io_timers[timer].clock_freq;
	uint32_t mclk_id    = io_timers[timer].mclk_id;
	uint32_t regval;
	uint32_t base       = io_timers[timer].base;
	uint32_t gclk_id    = io_timers[timer].gclk_id;

	/* 1. Enable MCLK APB clock */
	regval  = getreg32(SAM_MCLK_CLKMSK(mclk_id / 32u));
	regval |= (1u << (mclk_id % 32u));
	putreg32(regval, SAM_MCLK_CLKMSK(mclk_id / 32u));

	/* 2. Route GCLK1 (150 MHz) to TCC peripheral channel */
	putreg32(GCLK_PCHCTRL_GEN(1) | GCLK_PCHCTRL_CHEN,
		 SAM_GCLK_PCHCTRL(gclk_id));

	while ((getreg32(SAM_GCLK_PCHCTRL(gclk_id)) & GCLK_PCHCTRL_CHEN) == 0) {
	}

	/* 3. Software reset */
	tcc_putreg(base, SAM_TCC_CTRLA_OFFSET, TCC_CTRLA_SWRST);

	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_SWRST) {
	}

	/* 4. Set waveform: Normal PWM */
	tcc_putreg(base, SAM_TCC_WAVE_OFFSET, TCC_WAVE_WAVEGEN_NPWM);

	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_WAVE) {
	}

	/* 5. Set period for default 400 Hz rate */
	uint32_t period = clock_freq / PWM_DEFAULT_RATE;
	tcc_putreg(base, SAM_TCC_PER_OFFSET, period);

	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_PER) {
	}

	/* 6. Set all CC channels to 0 (outputs off) */
	for (uint8_t ch = 0; ch < io_timers[timer].cc_num && ch < 4; ch++) {
		tcc_putreg(base, SAM_TCC_CC_OFFSET(ch), 0);

		while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & (TCC_SYNCBUSY_CC0 << ch)) {
		}
	}

	/* 7. Clear all interrupt flags */
	tcc_putreg(base, SAM_TCC_INTFLAG_OFFSET, TCC_INTFLAG_ALL);

	/* 8. Enable TCC with prescaler DIV1 */
	tcc_putreg(base, SAM_TCC_CTRLA_OFFSET,
		   TCC_CTRLA_PRESCALER_DIV1 | TCC_CTRLA_ENABLE);

	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_ENABLE) {
	}

	g_timer_period[timer] = period;
	g_timers_initialized[timer] = true;

	return OK;
}

int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
			  channel_handler_t channel_handler, void *context)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	if (mode <= IOTimerChanMode_NotUsed || mode >= IOTimerChanModeSize) {
		return -EINVAL;
	}

	int ret = io_timer_allocate_channel(channel, mode);

	if (ret != OK) {
		return ret;
	}

	g_channel_handler_callbacks[channel] = channel_handler;
	g_channel_handler_contexts[channel] = context;

	uint8_t timer_idx = timer_io_channels[channel].timer_index;

	ret = io_timer_init_timer(timer_idx, mode);

	if (ret != OK) {
		g_channel_modes[channel] = IOTimerChanMode_NotUsed;
		return ret;
	}

	switch (mode) {
	case IOTimerChanMode_PWMOut:
	case IOTimerChanMode_OneShot: {
		sam_portconfig(timer_io_channels[channel].gpio_out);

		uint32_t base = io_timers[timer_idx].base;
		uint8_t cc_ch = timer_io_channels[channel].timer_channel;
		tcc_putreg(base, SAM_TCC_CCBUF_OFFSET(cc_ch), 0);
		break;
	}

	default:
		break;
	}

	return OK;
}

int io_timer_set_pwm_rate(unsigned timer, unsigned rate)
{
	if (timer >= MAX_IO_TIMERS) {
		return -EINVAL;
	}

	if (!g_timers_initialized[timer]) {
		return -EINVAL;
	}

	if (rate < 50 || rate > 8000) {
		return -ERANGE;
	}

	uint32_t clock_freq = io_timers[timer].clock_freq;
	uint32_t period = clock_freq / rate;

	g_timer_period[timer] = period;

	uint32_t base = io_timers[timer].base;
	tcc_putreg(base, SAM_TCC_PERBUF_OFFSET, period);

	return OK;
}

int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
			io_timer_channel_allocation_t masks)
{
	if (masks == IO_TIMER_ALL_MODES_CHANNELS) {
		masks = io_timer_get_mode_channels(mode);

	} else {
		masks &= io_timer_get_mode_channels(mode);
	}

	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if (masks & (1 << ch)) {
			uint8_t timer_idx = timer_io_channels[ch].timer_index;
			uint32_t base = io_timers[timer_idx].base;
			uint8_t cc_ch = timer_io_channels[ch].timer_channel;

			if (state) {
				sam_portconfig(timer_io_channels[ch].gpio_out);

			} else {
				tcc_putreg(base, SAM_TCC_CCBUF_OFFSET(cc_ch), 0);
			}
		}
	}

	return OK;
}

int io_timer_set_ccr(unsigned channel, uint16_t value)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	if (g_channel_modes[channel] != IOTimerChanMode_PWMOut &&
	    g_channel_modes[channel] != IOTimerChanMode_OneShot) {
		return -EINVAL;
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint32_t base = io_timers[timer_idx].base;
	uint8_t cc_ch = timer_io_channels[channel].timer_channel;
	uint32_t clock_freq = io_timers[timer_idx].clock_freq;

	uint32_t ticks = (uint64_t)value * clock_freq / 1000000ULL;

	if (ticks > g_timer_period[timer_idx]) {
		ticks = g_timer_period[timer_idx];
	}

	/* Use CC direct write instead of CCBUF — CCBUF is Write-Synchronized
	 * on CA90 and causes bus stall under DMA load (DS §44.7.22).
	 * CC direct write with SYNCBUSY poll matches HRT pattern (proven). */
	while (tcc_getreg(base, SAM_TCC_SYNCBUSY_OFFSET) &
	       (TCC_SYNCBUSY_CC0 << cc_ch)) {
	}

	tcc_putreg(base, SAM_TCC_CC_OFFSET(cc_ch), ticks);

	return OK;
}

uint16_t io_channel_get_ccr(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return 0;
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint32_t base = io_timers[timer_idx].base;
	uint8_t cc_ch = timer_io_channels[channel].timer_channel;
	uint32_t clock_freq = io_timers[timer_idx].clock_freq;

	uint32_t ticks = tcc_getreg(base, SAM_TCC_CC_OFFSET(cc_ch));

	return (uint16_t)((uint64_t)ticks * 1000000ULL / clock_freq);
}

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

int io_timer_validate_channel_index(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	return OK;
}

int io_timer_is_channel_free(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	return g_channel_modes[channel] == IOTimerChanMode_NotUsed ? 0 : -EBUSY;
}

int io_timer_free_channel(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	uint8_t timer_idx = timer_io_channels[channel].timer_index;
	uint32_t base = io_timers[timer_idx].base;
	uint8_t cc_ch = timer_io_channels[channel].timer_channel;

	/* Set CC to 0 (output LOW) */
	tcc_putreg(base, SAM_TCC_CCBUF_OFFSET(cc_ch), 0);

	g_channel_modes[channel] = IOTimerChanMode_NotUsed;

	return OK;
}

int io_timer_get_channel_mode(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return IOTimerChanMode_NotUsed;
	}

	return g_channel_modes[channel];
}

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

uint32_t io_timer_channel_get_gpio_output(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return 0;
	}

	return timer_io_channels[channel].gpio_out;
}

uint32_t io_timer_channel_get_as_pwm_input(unsigned channel)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return 0;
	}

	if (timer_io_channels[channel].gpio_in != 0) {
		return timer_io_channels[channel].gpio_in;
	}

	return timer_io_channels[channel].gpio_out;
}

int io_timer_allocate_channel(unsigned channel, io_timer_channel_mode_t mode)
{
	if (channel >= MAX_TIMER_IO_CHANNELS) {
		return -EINVAL;
	}

	irqstate_t flags = enter_critical_section();

	if (g_channel_modes[channel] != IOTimerChanMode_NotUsed) {
		leave_critical_section(flags);
		return -EBUSY;
	}

	g_channel_modes[channel] = mode;
	leave_critical_section(flags);

	return OK;
}

int io_timer_unallocate_channel(unsigned channel)
{
	return io_timer_free_channel(channel);
}

void io_timer_trigger(unsigned channels_mask)
{
	/* TCC CCBUF updates are applied automatically at period boundary.
	 * For OneShot: issue CMD=UPDATE to force immediate buffer transfer.
	 */
	for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
		if ((channels_mask & (1 << ch)) &&
		    g_channel_modes[ch] == IOTimerChanMode_OneShot) {
			uint8_t timer_idx = timer_io_channels[ch].timer_index;
			uint32_t base = io_timers[timer_idx].base;

			tcc_putreg(base, SAM_TCC_CTRLBSET_OFFSET,
				   TCC_CTRLBSET_CMD_UPDATE);
			tcc_wait_syncbusy(base, TCC_SYNCBUSY_CTRLB);
		}
	}
}

void io_timer_update_dma_req(uint8_t timer, bool enable)
{
	(void)timer;
	(void)enable;
}

int io_timer_set_dshot_mode(uint8_t timer, unsigned dshot_pwm_freq)
{
	(void)timer;
	(void)dshot_pwm_freq;
	return -ENOSYS;
}

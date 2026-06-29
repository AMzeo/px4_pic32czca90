/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

#pragma once

/**
 * @file io_timer_hw_description.h
 *
 * PIC32CZ CA90 TCC hardware description for PWM output.
 * Provides constexpr helpers for timer_config.cpp board configuration.
 */

#include <px4_arch/io_timer.h>
#include <px4_arch/hw_description.h>
#include <px4_platform_common/constexpr_util.h>

#include "hardware/sam_tcc.h"
#include "hardware/pic32czca90_memorymap.h"

#define initIOTimerChannelCapture initIOTCCChannel

static inline constexpr uint32_t tccBaseAddress(TCC::TCCModule module)
{
	switch (module) {
	case TCC::TCC1: return SAM_TCC1_BASE;
	case TCC::TCC7: return SAM_TCC7_BASE;
	}

	return 0;
}

static inline constexpr uint32_t tccGclkId(TCC::TCCModule module)
{
	switch (module) {
	case TCC::TCC1: return TCC1_GCLK_ID;
	case TCC::TCC7: return TCC7_GCLK_ID;
	}

	return 0;
}

static inline constexpr uint32_t tccMclkId(TCC::TCCModule module)
{
	switch (module) {
	case TCC::TCC1: return MCLK_ID_APB_TCC1;
	case TCC::TCC7: return MCLK_ID_APB_TCC7;
	}

	return 0;
}

static inline constexpr uint32_t tccFirstMcIrq(TCC::TCCModule module)
{
	switch (module) {
	case TCC::TCC1: return SAM_IRQ_TCC1MC0;
	case TCC::TCC7: return SAM_IRQ_TCC7MC0;
	}

	return 0;
}

static inline constexpr uint8_t tccCcNum(TCC::TCCModule module)
{
	switch (module) {
	case TCC::TCC1: return 8;
	case TCC::TCC7: return 2;
	}

	return 0;
}

/**
 * Initialize a TCC timer descriptor for io_timers[] array.
 */
static inline constexpr io_timers_t initIOTCCTimer(TCC::TCCModule module)
{
	io_timers_t ret{};

	ret.base       = tccBaseAddress(module);
	ret.gclk_id    = tccGclkId(module);
	ret.mclk_id    = tccMclkId(module);
	ret.clock_freq = BOARD_GCLK1_FREQUENCY;
	ret.first_irq  = tccFirstMcIrq(module);
	ret.cc_num     = tccCcNum(module);

	return ret;
}

/**
 * Initialize a TCC channel descriptor for timer_io_channels[] array.
 *
 * GPIO pin encoding follows pic32czca90_pinmap.h format:
 *   Bits 31-28: Port (0=A..6=G)
 *   Bits 27-24: Peripheral function (F=5 for TCC)
 *   Bits 20-16: Pin number (0-31)
 *   Bits 7-0:   Flags (PMUXEN=1)
 */
static inline constexpr timer_io_channels_t initIOTCCChannel(
	const io_timers_t io_timers_conf[MAX_IO_TIMERS],
	TCC::TCCChannel tcc_channel, GPIO::GPIOPin pin)
{
	timer_io_channels_t ret{};

	uint32_t port_bits = ((uint32_t)pin.port) << 28;
	uint32_t func_bits = 5u << 24;
	uint32_t pin_bits  = ((uint32_t)pin.pin) << 16;
	uint32_t flags     = 0x01u; /* PORT_FLAG_PMUXEN */

	ret.gpio_out = port_bits | func_bits | pin_bits | flags;
	ret.gpio_in  = 0;

	ret.timer_index = 0xff;

	uint32_t target_base = tccBaseAddress(tcc_channel.module);

	for (unsigned i = 0; i < MAX_IO_TIMERS; ++i) {
		if (io_timers_conf[i].base == target_base) {
			ret.timer_index = (uint8_t)i;
			break;
		}
	}

	constexpr_assert(ret.timer_index != 0xff, "TCC module not found in io_timers[]");

	ret.timer_channel = (uint8_t)tcc_channel.channel;
	ret.masks = (uint16_t)(1u << (uint8_t)tcc_channel.channel);

	return ret;
}

#include <px4_platform/io_timer_init.h>

/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file timer_config.cpp
 *
 * PIC32CZ CA90 Curiosity Ultra TCC PWM output configuration.
 *
 * 4 motor channels across 2 TCC instances:
 *   Motor 1: TCC1 WO0 — PB10, EXT1 pin 7
 *   Motor 2: TCC1 WO1 — PB11, EXT1 pin 8
 *   Motor 3: TCC7 WO0 — PA22, EXT2 pin 7
 *   Motor 4: TCC7 WO1 — PA23, EXT2 pin 8
 *
 * Clock: GCLK1 = 150 MHz, prescaler DIV1.
 * At 400 Hz default rate: PER = 375,000; 1500 us = 225,000 ticks.
 */

#include <px4_arch/io_timer_hw_description.h>

const io_timers_t io_timers[MAX_IO_TIMERS] = {
	initIOTCCTimer(TCC::TCC1),
	initIOTCCTimer(TCC::TCC7),
};

const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
	initIOTCCChannel(io_timers, {TCC::TCC1, TCC::Channel0}, {GPIO::PortB, GPIO::Pin10}),
	initIOTCCChannel(io_timers, {TCC::TCC1, TCC::Channel1}, {GPIO::PortB, GPIO::Pin11}),
	initIOTCCChannel(io_timers, {TCC::TCC7, TCC::Channel0}, {GPIO::PortA, GPIO::Pin22}),
	initIOTCCChannel(io_timers, {TCC::TCC7, TCC::Channel1}, {GPIO::PortA, GPIO::Pin23}),
};

const io_timers_channel_mapping_t io_timers_channel_mapping =
	initIOTimerChannelMapping(io_timers, timer_io_channels);

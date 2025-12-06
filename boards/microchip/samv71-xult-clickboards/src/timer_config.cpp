/****************************************************************************
 *
 *   Copyright (C) 2024 PX4 Development Team. All rights reserved.
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
 * @file timer_config.cpp
 *
 * Configuration data for the SAMV71 PWM driver using TC (Timer/Counter).
 *
 * SAMV71-XULT PWM Output Configuration:
 *   TC0 CH0 (TC0) - Reserved for HRT
 *   TC0 CH1 (TC1) - PWM1: PA15 (TIOA1)
 *   TC0 CH2 (TC2) - RESERVED: PA26 conflicts with HSMCI DA2 (SD card D2)
 *   TC1 CH0 (TC3) - PWM2: PC23 (TIOA3)
 *   TC1 CH1 (TC4) - PWM3: PC26 (TIOA4)
 *   TC1 CH2 (TC5) - Reserved for RC Input capture: PC29 (TIOA5)
 *
 * NOTE: TC0 CH2 (PA26) cannot be used for PWM because PA26 is the HSMCI0 DA2
 *       data line required for 4-bit SD card mode. Using PA26 for PWM would
 *       corrupt SD card writes (bits 2 and 6 are carried on D2).
 */

#include <px4_arch/io_timer_hw_description.h>

/**
 * Timer block configuration
 *
 * Each timer represents a TC channel:
 * Timer1 = TC0 CH1 (TC block 0)
 * Timer3 = TC1 CH0, Timer4 = TC1 CH1 (TC block 1)
 * Note: Timer2 (TC0 CH2, PA26) NOT USED - conflicts with SD card
 */
const io_timers_t io_timers[MAX_IO_TIMERS] = {
	initIOTimer(Timer::Timer1),  /* TC block 0 - CH1 */
	initIOTimer(Timer::Timer3),  /* TC block 1 - CH0 */
	initIOTimer(Timer::Timer4),  /* TC block 1 - CH1 */
};

/**
 * Timer channel to GPIO pin mapping
 *
 * Channel 0: Timer1 (TC0 CH1) -> PA15 (TIOA1)
 * Channel 1: Timer3 (TC1 CH0) -> PC23 (TIOA3)
 * Channel 2: Timer4 (TC1 CH1) -> PC26 (TIOA4)
 */
const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
	initIOTimerChannel(io_timers, {Timer::Timer1, Timer::Channel1}, {GPIO::PortA, GPIO::Pin15}),
	initIOTimerChannel(io_timers, {Timer::Timer3, Timer::Channel1}, {GPIO::PortC, GPIO::Pin23}),
	initIOTimerChannel(io_timers, {Timer::Timer4, Timer::Channel1}, {GPIO::PortC, GPIO::Pin26}),
};

const io_timers_channel_mapping_t io_timers_channel_mapping =
	initIOTimerChannelMapping(io_timers, timer_io_channels);

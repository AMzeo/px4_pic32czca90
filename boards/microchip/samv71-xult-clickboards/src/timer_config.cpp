/****************************************************************************
 *
 *   Copyright (C) 2024-2025 PX4 Development Team. All rights reserved.
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

/**
 * @file timer_config.cpp
 *
 * Configuration data for the SAMV71 PWM driver using PWMC (PWM Controller).
 *
 * SAMV71-XULT PWMC Output Configuration (Option A):
 *   Motor 1 (CH3): PA7  - GPIO_PWMC0_H3 (Peripheral B)
 *   Motor 2 (CH1): PA2  - GPIO_PWMC0_H1 (Peripheral A)
 *   Motor 3 (CH2): PC19 - GPIO_PWMC0_H2 (Peripheral B)
 *   Motor 4 (CH0): PB0  - GPIO_PWMC0_H0 (Peripheral A)
 *
 * Clock Configuration:
 *   MCK = 150MHz, CPRE = 3 (MCK/8 = 18.75MHz)
 *   For 400Hz PWM: CPRD = 46875
 *
 * NOTE: TC0 CH0 is still used for HRT (high-resolution timer).
 *       TC1 CH2 (TC5, PC29) is reserved for RC Input capture.
 */

#include <px4_arch/io_timer_hw_description.h>

/**
 * PWM Controller (PWMC) module configuration
 *
 * Using PWM0 module for all 4 motor outputs.
 * PWM0 base address: 0x40020000, PID: 31
 */
const io_timers_t io_timers[MAX_IO_TIMERS] = {
	initIOPWMTimer(PWM::PWM0),
};

/**
 * PWMC channel to GPIO pin mapping
 *
 * Order determines motor number (index 0 = Motor 1, etc.)
 * Peripheral function (A or B) depends on specific pin - verified in samv71_pinmap.h
 *
 * Channel 0 (Motor 1): PWM0 CH3 -> PA7  (Peripheral B)
 * Channel 1 (Motor 2): PWM0 CH1 -> PA2  (Peripheral A)
 * Channel 2 (Motor 3): PWM0 CH2 -> PC19 (Peripheral B)
 * Channel 3 (Motor 4): PWM0 CH0 -> PB0  (Peripheral A)
 */
const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
	initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel3}, {GPIO::PortA, GPIO::Pin7},  PWMCPeripheral::B),  /* Motor 1 */
	initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel1}, {GPIO::PortA, GPIO::Pin2},  PWMCPeripheral::A),  /* Motor 2 */
	initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel2}, {GPIO::PortC, GPIO::Pin19}, PWMCPeripheral::B),  /* Motor 3 */
	initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel0}, {GPIO::PortB, GPIO::Pin0},  PWMCPeripheral::A),  /* Motor 4 */
};

const io_timers_channel_mapping_t io_timers_channel_mapping =
	initIOTimerChannelMapping(io_timers, timer_io_channels);

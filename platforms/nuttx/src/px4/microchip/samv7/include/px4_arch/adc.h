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

/**
 * @file adc.h
 *
 * SAMV7 ADC architecture definitions
 */

#pragma once

#include <board_config.h>
#include <hardware/sam_memorymap.h>

/* SAMV7 uses a single AFEC for system ADC - same base for HW version and system */
#if !defined(HW_REV_VER_ADC_BASE)
#  define HW_REV_VER_ADC_BASE SAM_AFEC0_BASE
#endif

#if !defined(SYSTEM_ADC_BASE)
#  define SYSTEM_ADC_BASE SAM_AFEC0_BASE
#endif

#include <px4_platform/adc.h>

__BEGIN_DECLS

/**
 * Initialize the ADC hardware.
 *
 * @param base_address  The base address of the ADC peripheral
 * @return              0 on success, negative errno on failure
 */
int px4_arch_adc_init(uint32_t base_address);

/**
 * Uninitialize the ADC hardware.
 *
 * @param base_address  The base address of the ADC peripheral
 */
void px4_arch_adc_uninit(uint32_t base_address);

/**
 * Sample a single ADC channel.
 *
 * @param base_address  The base address of the ADC peripheral
 * @param channel       The channel number (0-11 for AFEC0/AFEC1)
 * @return              The 12-bit ADC sample value, or UINT32_MAX on error
 */
uint32_t px4_arch_adc_sample(uint32_t base_address, unsigned channel);

/**
 * Get the ADC reference voltage.
 *
 * @return  The reference voltage in volts (typically 3.3V)
 */
float px4_arch_adc_reference_v(void);

/**
 * Get the temperature sensor channel mask.
 *
 * @return  Bitmask of channels used for temperature sensing (channel 11 for SAMV7)
 */
uint32_t px4_arch_adc_temp_sensor_mask(void);

/**
 * Get the full-scale digital count for the ADC.
 *
 * @return  The full-scale count (4096 for 12-bit ADC)
 */
uint32_t px4_arch_adc_dn_fullcount(void);

__END_DECLS

/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/* Stub ADC interface for PIC32CZ CA90 - ADC not yet implemented */

#pragma once

#include <stdint.h>

__BEGIN_DECLS

int px4_arch_adc_init(uint32_t base_address);
void px4_arch_adc_uninit(uint32_t base_address);
uint32_t px4_arch_adc_sample(uint32_t base_address, unsigned channel);
float px4_arch_adc_reference_v(void);
uint32_t px4_arch_adc_temp_sensor_mask(void);
uint32_t px4_arch_adc_dn_fullcount(void);

__END_DECLS

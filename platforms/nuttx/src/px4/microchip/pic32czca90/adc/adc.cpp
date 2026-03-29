/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file adc.cpp
 *
 * Stub ADC driver for PIC32CZ CA90 - ADC not yet implemented.
 */

#include <px4_platform_common/px4_config.h>
#include <stdint.h>
#include <errno.h>

int px4_arch_adc_init(uint32_t base_address)
{
	(void)base_address;
	return -ENOSYS;
}

void px4_arch_adc_uninit(uint32_t base_address)
{
	(void)base_address;
}

uint32_t px4_arch_adc_sample(uint32_t base_address, unsigned channel)
{
	(void)base_address;
	(void)channel;
	return UINT32_MAX;  /* No valid sample */
}

float px4_arch_adc_reference_v(void)
{
	return 3.3f;
}

uint32_t px4_arch_adc_temp_sensor_mask(void)
{
	return 0;
}

uint32_t px4_arch_adc_dn_fullcount(void)
{
	return 4096;  /* 12-bit ADC */
}

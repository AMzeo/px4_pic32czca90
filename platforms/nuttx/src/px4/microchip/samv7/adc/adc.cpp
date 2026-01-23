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
 * @file adc.cpp
 *
 * Driver for the SAMV7 AFEC (Analog Front End Controller).
 *
 * This is a low-rate driver for sampling voltages (battery monitoring).
 * It bypasses the NuttX ADC driver for simplicity and direct control.
 *
 * Based on Microchip Harmony 3 CSP AFEC implementation patterns.
 */

#include <board_config.h>
#include <stdint.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_adc.h>
#include <px4_arch/adc.h>

#include "arm_internal.h"
#include <hardware/sam_afec.h>
#include <sam_periphclks.h>
#include <sam_gpio.h>
#include <hardware/sam_pinmap.h>

#define _REG(_addr)  (*(volatile uint32_t *)(_addr))
#define REG(base_address, _reg) _REG((base_address) + (_reg))

/* AFEC Register accessors */
#define rCR(base)    REG(base, SAM_AFEC_CR_OFFSET)    /* Control Register */
#define rMR(base)    REG(base, SAM_AFEC_MR_OFFSET)    /* Mode Register */
#define rEMR(base)   REG(base, SAM_AFEC_EMR_OFFSET)   /* Extended Mode Register */
#define rCHER(base)  REG(base, SAM_AFEC_CHER_OFFSET)  /* Channel Enable */
#define rCHDR(base)  REG(base, SAM_AFEC_CHDR_OFFSET)  /* Channel Disable */
#define rCHSR(base)  REG(base, SAM_AFEC_CHSR_OFFSET)  /* Channel Status */
#define rLCDR(base)  REG(base, SAM_AFEC_LCDR_OFFSET)  /* Last Converted Data */
#define rISR(base)   REG(base, SAM_AFEC_ISR_OFFSET)   /* Interrupt Status */
#define rCSELR(base) REG(base, SAM_AFEC_CSELR_OFFSET) /* Channel Selection */
#define rCDR(base)   REG(base, SAM_AFEC_CDR_OFFSET)   /* Channel Data */
#define rACR(base)   REG(base, SAM_AFEC_ACR_OFFSET)   /* Analog Control */

int px4_arch_adc_init(uint32_t base_address)
{
	static bool once = false;

	if (!once) {
		once = true;

		irqstate_t flags = px4_enter_critical_section();

		/* Enable AFEC0 peripheral clock */
		sam_afec0_enableclk();

		/* Configure ADC pins as analog inputs
		 * IMPORTANT: Must explicitly configure GPIO for analog function
		 * PD30 = AFEC0_AD0 (Battery Voltage)
		 * PA18 = AFEC0_AD7 (Battery Current)
		 */
		sam_configgpio(GPIO_AFE0_AD0);  /* PD30 */
		sam_configgpio(GPIO_AFE0_AD7);  /* PA18 */

		/* Software reset */
		rCR(base_address) = AFEC_CR_SWRST;

		/* Configure Mode Register:
		 * - PRESCAL: Determines AFEC clock = MCK / (PRESCAL + 1)
		 *   SAMV71 MCK = 150MHz, target AFEC clock 4-40MHz
		 *   PRESCAL = 31 gives: 150MHz / 32 = 4.6875 MHz (within spec)
		 * - STARTUP: SUT64 = 64 periods of ADCClock for startup
		 * - TRANSFER: 2 periods
		 * - ONE: Must be set to 1 (per datasheet)
		 */
		rMR(base_address) = AFEC_MR_PRESCAL(31) |
				    AFEC_MR_STARTUP_64 |
				    AFEC_MR_TRANSFER(2) |
				    AFEC_MR_ONE;

		/* Extended Mode Register:
		 * - RES_NO_AVERAGE: 12-bit resolution, no oversampling
		 * - SIGNMODE: Single-ended unsigned (default)
		 * - TAG: Enable channel number in LCDR (useful for debug)
		 */
		rEMR(base_address) = AFEC_EMR_RES_NOAVG | AFEC_EMR_TAG;

		/* Analog Control Register:
		 * - PGA disabled: Battery voltage dividers output 0-3.3V,
		 *   no amplification needed and PGA could distort readings
		 * - IBCTL: Bias current control based on AFEC clock frequency
		 *   AFEC_CLK > 1MHz requires IBCTL = 3
		 *   With AFEC_CLK = 4.6875MHz, use IBCTL = 3
		 */
		rACR(base_address) = AFEC_ACR_IBCTL(3);

		px4_leave_critical_section(flags);

		/* Perform a test conversion to verify initialization */
		hrt_abstime now = hrt_absolute_time();
		rCHER(base_address) = AFEC_CH0;  /* Enable channel 0 */
		rCR(base_address) = AFEC_CR_START;

		while (!(rISR(base_address) & AFEC_INT_EOC0)) {
			if ((hrt_absolute_time() - now) > 500) {
				return -1;  /* Timeout */
			}
		}

		/* Read and discard test result (select channel first, then read CDR) */
		rCSELR(base_address) = AFEC_CSELR_CSEL(0);
		volatile uint32_t discard = rCDR(base_address);
		(void)discard;

		rCHDR(base_address) = AFEC_CH0;  /* Disable channel 0 */
	}

	return 0;
}

void px4_arch_adc_uninit(uint32_t base_address)
{
	/* Disable all channels */
	rCHDR(base_address) = AFEC_CHALL;

	/* Disable AFEC0 peripheral clock */
	sam_afec0_disableclk();
}

uint32_t px4_arch_adc_sample(uint32_t base_address, unsigned channel)
{
	if (channel > 11) {
		return UINT32_MAX;
	}

	irqstate_t flags = px4_enter_critical_section();

	/* Enable the channel */
	rCHER(base_address) = AFEC_CH(channel);

	/* Start conversion */
	rCR(base_address) = AFEC_CR_START;

	/* Wait for conversion complete */
	hrt_abstime now = hrt_absolute_time();

	while (!(rISR(base_address) & AFEC_INT_EOC(channel))) {
		if ((hrt_absolute_time() - now) > 50) {
			rCHDR(base_address) = AFEC_CH(channel);  /* Cleanup */
			px4_leave_critical_section(flags);
			return UINT32_MAX;  /* Timeout */
		}
	}

	/* Select channel and read result
	 * MUST select channel via CSELR before reading CDR
	 */
	rCSELR(base_address) = AFEC_CSELR_CSEL(channel);
	uint32_t result = rCDR(base_address) & AFEC_CDR_MASK;

	/* Disable the channel */
	rCHDR(base_address) = AFEC_CH(channel);

	px4_leave_critical_section(flags);

	return result;
}

float px4_arch_adc_reference_v()
{
	return BOARD_ADC_POS_REF_V;
}

uint32_t px4_arch_adc_temp_sensor_mask()
{
	/* SAMV7 has internal temp sensor on channel 11 */
	return (1 << 11);
}

uint32_t px4_arch_adc_dn_fullcount(void)
{
	return 1 << 12;  /* 12-bit ADC */
}

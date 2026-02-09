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

#pragma once

#include <drivers/drv_pwm_output.h>

#define DSHOT_MOTOR_PWM_BIT_WIDTH	20u

/**
 * SAMV7 DShot configuration per PWMC module.
 *
 * SAMV7 uses PWMC Synchronous Channel Mode (SCM.UPDM=2) + XDMAC
 * instead of STM32-style TIM DMA burst. XDMAC writes duty values
 * to the DMAR register; hardware distributes to synchronized channels.
 *
 * XDMAC hardware request IDs (from SAMV71 datasheet Table 47-1):
 *   PWM0 TX: XDMAC channel 13
 *   PWM1 TX: XDMAC channel 39
 */
typedef struct dshot_conf_t {
	uint8_t  xdmac_ch_tx;      /* XDMAC channel for PWM TX (13=PWM0, 39=PWM1) */
	uint8_t  xdmac_ch_rx[4];   /* Reserved for bidirectional DShot capture */
	uint32_t tc_capture_base;   /* Reserved for bidirectional TC capture base */
} dshot_conf_t;

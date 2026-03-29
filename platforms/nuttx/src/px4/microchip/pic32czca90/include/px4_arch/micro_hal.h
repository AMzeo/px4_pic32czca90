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
#pragma once

#include <px4_platform/micro_hal.h>
#include <nuttx/i2c/i2c_master.h>

__BEGIN_DECLS

#include "sam_port.h"
#include "hardware/sam_pinmap.h"

/* PIC32CZ CA90 uses PORT-based GPIO (SAMD5E5-derived), not PIO-based (SAMV7).
 *
 * Port pin encoding (port_pinset_t / uint32_t):
 *   Bits 31-28: Port (0=A, 1=B, 2=C, 3=D)
 *   Bits 27-24: Peripheral function
 *   Bits 20-16: Pin number (0-31)
 *   Bits  7-0:  Configuration flags
 */

#define PX4_CPU_UUID_BYTE_LENGTH                12
#define PX4_CPU_UUID_WORD32_LENGTH              (PX4_CPU_UUID_BYTE_LENGTH/sizeof(uint32_t))

#define PX4_CPU_MFGUID_BYTE_LENGTH              PX4_CPU_UUID_BYTE_LENGTH

#define PX4_CPU_UUID_WORD32_UNIQUE_H            2
#define PX4_CPU_UUID_WORD32_UNIQUE_M            1
#define PX4_CPU_UUID_WORD32_UNIQUE_L            0

#define PX4_CPU_UUID_WORD32_FORMAT_SIZE         (PX4_CPU_UUID_WORD32_LENGTH-1+(2*PX4_CPU_UUID_BYTE_LENGTH)+1)
#define PX4_CPU_MFGUID_FORMAT_SIZE              ((2*PX4_CPU_MFGUID_BYTE_LENGTH)+1)

#define px4_savepanic(fileno, context, length)  (0)

/* Bus numbering: PX4 uses 1-based, NuttX uses 0-based */
#define PX4_BUS_OFFSET       1

/* GPIO abstraction: map px4_arch_* to PIC32CZ CA90 PORT functions */
#define px4_arch_configgpio(pinset)             sam_portconfig(pinset)
#define px4_arch_unconfiggpio(pinset)           sam_portconfig(pinset)
#define px4_arch_gpioread(pinset)               sam_portread(pinset)
#define px4_arch_gpiowrite(pinset, value)       sam_portwrite(pinset, value)

/* GPIO interrupt not yet implemented */
#define px4_arch_gpiosetevent(pinset,r,f,e,fp,a)  (-ENOSYS)

/* PORT encoding masks for PX4 GPIO helper macros */
#define PORT_PORT_MASK          (0xFu << 28)
#define PORT_PIN_MASK_PX4       (0x1Fu << 16)

#define PX4_MAKE_GPIO_INPUT(gpio) \
	(((gpio) & (PORT_PORT_MASK | PORT_PIN_MASK_PX4)) | PORT_FLAG_INEN | PORT_FLAG_PULLEN)
#define PX4_MAKE_GPIO_EXTI(gpio) \
	(((gpio) & (PORT_PORT_MASK | PORT_PIN_MASK_PX4)) | PORT_FLAG_INEN)
#define PX4_MAKE_GPIO_OUTPUT_CLEAR(gpio) \
	(((gpio) & (PORT_PORT_MASK | PORT_PIN_MASK_PX4)) | PORT_FLAG_OUTPUT)
#define PX4_MAKE_GPIO_OUTPUT_SET(gpio) \
	(((gpio) & (PORT_PORT_MASK | PORT_PIN_MASK_PX4)) | PORT_FLAG_OUTPUT)
#define PX4_GPIO_PIN_OFF(def) \
	(((def) & (PORT_PORT_MASK | PORT_PIN_MASK_PX4)) | PORT_FLAG_INEN)

/* PIC32CZ CA90 runs at 300 MHz (CPU clock from GCLK0) */
#define TIMER_HRT_CYCLES_PER_US (BOARD_CPU_FREQUENCY/1000000)
#define TIMER_HRT_CYCLES_PER_MS (BOARD_CPU_FREQUENCY/1000)

/* CAN bootloader placeholders */
#define crc_HiLOC       0
#define crc_LoLOC       0
#define signature_LOC   0
#define bus_speed_LOC   0
#define node_id_LOC     0

#if defined(CONFIG_ARMV7M_DCACHE)
#  define PX4_ARCH_DCACHE_ALIGNMENT ARMV7M_DCACHE_LINESIZE
#  define px4_cache_aligned_data() aligned_data(ARMV7M_DCACHE_LINESIZE)
#  define px4_cache_aligned_alloc(s) memalign(ARMV7M_DCACHE_LINESIZE,(s))
#else
#  define px4_cache_aligned_data()
#  define px4_cache_aligned_alloc malloc
#endif

__END_DECLS

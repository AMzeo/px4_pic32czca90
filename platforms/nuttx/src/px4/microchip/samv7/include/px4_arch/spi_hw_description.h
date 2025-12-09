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

#include <px4_platform_common/spi.h>
#include <sam_gpio.h>

/* SAMV7 SPI Hardware Description
 * SAMV7 has multiple SPI peripherals
 * SPI0, SPI1 - Standard SPI interfaces
 */

/* SPI bus timing configuration */
#define SPI_BUS_CLOCK_DEFAULT   12000000  /* 12 MHz default clock */
#define SPI_BUS_CLOCK_MAX       50000000  /* 50 MHz maximum clock */

/* GPIO and SPI namespace for pin configuration */
namespace GPIO
{
enum Port {
	PortA,
	PortB,
	PortC,
	PortD,
	PortE,
};
enum Pin {
	Pin0, Pin1, Pin2, Pin3, Pin4, Pin5, Pin6, Pin7,
	Pin8, Pin9, Pin10, Pin11, Pin12, Pin13, Pin14, Pin15,
	Pin16, Pin17, Pin18, Pin19, Pin20, Pin21, Pin22, Pin23,
	Pin24, Pin25, Pin26, Pin27, Pin28, Pin29, Pin30, Pin31,
};
}

namespace SPI
{
enum class Bus {
	SPI0 = 1,  // PX4 uses 1-based, NuttX SAMV7 uses 0-based (converted in micro_hal.h)
	SPI1 = 2,
};

struct CS {
	GPIO::Port port;
	GPIO::Pin pin;
};

struct DRDY {
	GPIO::Port port;
	GPIO::Pin pin;
	uint32_t pinset{0};
};

using PinCS = GPIO::Pin;
using PinDRDY = GPIO::Pin;
using Port = GPIO::Port;
}

#if defined(CONFIG_SPI)

static constexpr px4_spi_bus_t initSPIBus(SPI::Bus bus, const px4_spi_bus_devices_t &devices)
{
	px4_spi_bus_t ret{};
	ret.bus = static_cast<int8_t>(bus);
	// Copy entire devices array
	for (size_t i = 0; i < SPI_BUS_MAX_DEVICES; i++) {
		ret.devices[i] = devices.devices[i];
	}
	ret.is_external = false;
	return ret;
}

static constexpr px4_spi_bus_t initSPIBusExternal(SPI::Bus bus, const px4_spi_bus_devices_t &devices)
{
	px4_spi_bus_t ret{};
	ret.bus = static_cast<int8_t>(bus);
	// Copy entire devices array
	for (size_t i = 0; i < SPI_BUS_MAX_DEVICES; i++) {
		ret.devices[i] = devices.devices[i];
	}
	ret.is_external = true;
	return ret;
}

#endif // CONFIG_SPI


/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

#pragma once

#include <px4_platform_common/spi.h>

namespace GPIO
{
enum Port {
	PortA,
	PortB,
	PortC,
	PortD,
	PortE,
	PortF,
	PortG,
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
	SPI3 = 3,
};

struct CS {
	GPIO::Port port;
	GPIO::Pin pin;
};

struct DRDY {
	GPIO::Port port;
	GPIO::Pin pin;
};
}

#if defined(CONFIG_SPI)

static inline constexpr px4_spi_bus_device_t initSPIDevice(uint32_t devid, SPI::CS cs, SPI::DRDY drdy = {})
{
	px4_spi_bus_device_t ret{};
	ret.cs_gpio = 0;
	ret.drdy_gpio = 0;
	ret.devid = devid;
	ret.devtype_driver = devid;
	return ret;
}

static inline constexpr px4_spi_bus_t initSPIBus(SPI::Bus bus, const px4_spi_bus_devices_t &devices)
{
	px4_spi_bus_t ret{};
	ret.bus = static_cast<int8_t>(bus);

	for (size_t i = 0; i < SPI_BUS_MAX_DEVICES; i++) {
		ret.devices[i] = devices.devices[i];
	}

	ret.is_external = false;
	return ret;
}

#endif /* CONFIG_SPI */

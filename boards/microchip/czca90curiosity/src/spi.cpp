/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

#include <board_config.h>
#include <px4_arch/micro_hal.h>
#include <px4_arch/spi_hw_description.h>
#include <px4_platform_common/spi.h>
#include <drivers/drv_sensor.h>
#include <nuttx/spi/spi.h>

static constexpr px4_spi_bus_device_t make_spidev(uint32_t drvtype, uint32_t cs_gpio,
		spi_drdy_gpio_t drdy_gpio = 0)
{
	return px4_spi_bus_device_t {
		.cs_gpio = cs_gpio,
		.drdy_gpio = drdy_gpio,
		.devid = PX4_SPIDEV_ID(PX4_SPI_DEVICE_ID, drvtype),
		.devtype_driver = static_cast<uint16_t>(drvtype),
	};
}

constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
	{
		.devices = {
			make_spidev(DRV_IMU_DEVTYPE_ICM20689, GPIO_SPI3_CS_IMU),
		},
		.power_enable_gpio = 0,
		.bus = static_cast<int8_t>(SPI::Bus::SPI3),
		.is_external = false,
		.requires_locking = false,
	},
};

extern "C" {

void pic32czca90_spi3select(FAR struct spi_dev_s *dev, uint32_t devid,
			    bool selected)
{
	for (const auto &device : px4_spi_buses[0].devices) {
		if (device.cs_gpio == 0) {
			continue;
		}

		if (device.devid == devid) {
			px4_arch_gpiowrite(device.cs_gpio, !selected);
			return;
		}
	}

	px4_arch_gpiowrite(GPIO_SPI3_CS_IMU, !selected);
}

uint8_t pic32czca90_spi3status(FAR struct spi_dev_s *dev, uint32_t devid)
{
	(void)dev;
	(void)devid;
	return SPI_STATUS_PRESENT;
}

} /* extern "C" */

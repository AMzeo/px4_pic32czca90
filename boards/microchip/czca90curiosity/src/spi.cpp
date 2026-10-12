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
#include "arm_internal.h"

/* SERCOM8 direct register access — warm-up dummy bytes before first
 * ICM-45686 WHO_AM_I read.  Without a preceding dummy transaction the
 * ICM SPI output shift register is unprimed and returns 0xE0 instead
 * of 0xE9.  No mode switch: SERCOM8 stays in Mode 0 throughout. */
#define _S8_BASE    0x45002000u
#define _S8_INTFLAG 0x18u
#define _S8_DATA    0x28u
#define _S8_DRE     (1u << 0)
#define _S8_TXC     (1u << 1)
#define _S8_RXC     (1u << 2)

static inline void _s8_send(uint8_t tx)
{
	int t = 200000;
	while (!(getreg8(_S8_BASE + _S8_INTFLAG) & _S8_DRE) && --t > 0) {}
	putreg32(tx, _S8_BASE + _S8_DATA);
}

static inline void _s8_wait_txc(void)
{
	int t = 200000;
	while (!(getreg8(_S8_BASE + _S8_INTFLAG) & _S8_TXC) && --t > 0) {}
	putreg8(_S8_TXC, _S8_BASE + _S8_INTFLAG);
	while (getreg8(_S8_BASE + _S8_INTFLAG) & _S8_RXC) {
		(void)getreg32(_S8_BASE + _S8_DATA);
	}
}

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
			make_spidev(DRV_IMU_DEVTYPE_ICM45686, GPIO_SPI8_CS_IMU, 0),
			make_spidev(DRV_IMU_DEVTYPE_ICM20689, GPIO_SPI8_CS_IMU, 0),
		},
		.power_enable_gpio = 0,
		.bus = static_cast<int8_t>(SPI::Bus::SPI8),
		.is_external = false,
		.requires_locking = false,
	},
};

extern "C" {

void pic32czca90_spi8select(FAR struct spi_dev_s *dev, uint32_t devid,
			    bool selected)
{
	static bool s_warmup_done = false;

	if (selected && !s_warmup_done && devid != 0) {
		s_warmup_done = true;
		/* Dummy 2-byte transaction before the driver's first real CS assertion.
		 * Primes the ICM-45686 SPI output shift register so WHO_AM_I returns
		 * 0xE9 rather than 0xE0 on the first read. */
		px4_arch_gpiowrite(GPIO_SPI8_CS_IMU, false);
		_s8_send(0xAA);
		_s8_send(0x55);
		_s8_wait_txc();
		px4_arch_gpiowrite(GPIO_SPI8_CS_IMU, true);
		up_mdelay(200); /* ICM-45686 internal reset after T0 takes >103ms */
	}

	for (const auto &device : px4_spi_buses[0].devices) {
		if (device.cs_gpio == 0) {
			continue;
		}

		if (device.devid == devid) {
			px4_arch_gpiowrite(device.cs_gpio, !selected);

			if (selected) { up_udelay(2); }

			return;
		}
	}

	px4_arch_gpiowrite(GPIO_SPI8_CS_IMU, !selected);

	if (selected) { up_udelay(2); }
}

uint8_t pic32czca90_spi8status(FAR struct spi_dev_s *dev, uint32_t devid)
{
	(void)dev;
	(void)devid;
	return SPI_STATUS_PRESENT;
}

} /* extern "C" */

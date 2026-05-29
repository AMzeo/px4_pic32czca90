/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

#pragma once

#include <px4_platform_common/i2c.h>

#if defined(CONFIG_I2C)

static constexpr px4_i2c_bus_t initI2CBusInternal(int bus)
{
	px4_i2c_bus_t ret{};
	ret.bus = bus;
	ret.is_external = false;
	return ret;
}

static constexpr px4_i2c_bus_t initI2CBusExternal(int bus)
{
	px4_i2c_bus_t ret{};
	ret.bus = bus;
	ret.is_external = true;
	return ret;
}

#endif /* CONFIG_I2C */

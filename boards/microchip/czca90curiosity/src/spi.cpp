/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/* SPI bus configuration for PIC32CZ CA90 Curiosity Ultra.
 * No SERCOM SPI buses are implemented yet (Stage P5).
 * This file exists to satisfy the px4_spi_buses[] link requirement that
 * CONFIG_SPI=y (selected by PIC32CZCA90_SQI1 for the spi_dev_s interface)
 * introduces via platforms/common/include/px4_platform_common/spi.h. */

#include <board_config.h>
#include <px4_platform_common/spi.h>

constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {};

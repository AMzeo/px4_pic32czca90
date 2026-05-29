/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * PIC32CZ CA90 Curiosity Ultra internal definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#include <nuttx/mtd/mtd.h>

#include "sam_port.h"
#include "hardware/sam_pinmap.h"

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* LEDs — DS70005522C Table 2-11, both active LOW (yellow) */

#define GPIO_nLED_BLUE    PORT_LED0   /* PB21, active LOW — armed/status */
#define GPIO_nLED_GREEN   PORT_LED1   /* PB22, active LOW — activity/fault */

/* SDMMC1 — card detect, PC28, active LOW.
 * Also defined in hardware/pic32czca90_pinmap.h for use by the chip-layer
 * driver (sam_sdmmc.c cannot include board_config.h directly). */

#ifndef PIN_SDMMC1_CD
#define PIN_SDMMC1_CD     PORT_SDMMC1_CD
#endif

#define BOARD_HAS_CONTROL_STATUS_LEDS      1
#define BOARD_ARMED_STATE_LED  LED_BLUE

/* I2C — SERCOM5 (PC25=SDA, PC26=SCL), registered as bus 5 in board_app_initialize.
 * PX4_NUMBER_I2C_BUSES must be >= bus number (5) so _bus_clocks[] array is large enough.
 * BOARD_I2C_LATEINIT prevents px4_platform_init() from calling px4_platform_i2c_init()
 * before GCLK2 generator is configured (would hang on SYNCBUSY). */
#define PX4_NUMBER_I2C_BUSES    5
#define BOARD_NUMBER_I2C_BUSES  1
#define BOARD_I2C_BUS_CLOCK_INIT {0, 0, 0, 0, 400000}
#define BOARD_I2C_LATEINIT      1

/* No ADC / battery monitoring yet — use digital brick to avoid ADC dependency */
#define BOARD_NUMBER_BRICKS          1
#define BOARD_NUMBER_DIGITAL_BRICKS  1
#define BOARD_ADC_BRICK_VALID        (1)

/* SQI1 flash (SST26VF032BAT, 4 MB) partition layout
 * Offsets in erase-sector units (1 sector = 4096 bytes).
 * Must match docs/sqi_filesystem.md. */
#define QSPI_PART_PARAMS_OFFSET      0
#define QSPI_PART_PARAMS_SECTORS     32    /* 128 KB */
#define QSPI_PART_PARAMS_TYPE        MTD_PARAMETERS
#define QSPI_PART_CALDATA_OFFSET     32
#define QSPI_PART_CALDATA_SECTORS    16    /* 64 KB */
#define QSPI_PART_CALDATA_TYPE       MTD_CALDATA
#define QSPI_PART_WAYPOINTS_OFFSET   48
#define QSPI_PART_WAYPOINTS_SECTORS  128   /* 512 KB */
#define QSPI_PART_WAYPOINTS_TYPE     MTD_WAYPOINTS
#define QSPI_NUM_PARTITIONS          3

/* SPI — SERCOM3 on EXT2 header (PC12/PC13/PC14/PC15, mux E)
 * CS is GPIO-controlled (not hardware SS).
 * DOPO=0: MOSI=PAD0(PC12), SCK=PAD1(PC13)
 * DIPO=3: MISO=PAD3(PC15)
 * CS: PC14 (GPIO output, active LOW, initial HIGH) */

#define PX4_SPI_BUS_SENSORS         3

#define GPIO_SPI3_CS_IMU \
    (PORT_PORTC | PORT_PIN(14) | PORT_FLAG_OUTPUT | PORT_FLAG_OUTVAL_HIGH)

#define PX4_SPIDEV_ICM_42688P       PX4_MK_SPI_SEL(PX4_SPI_BUS_SENSORS, 0)

/* No PWM channels */
#define DIRECT_PWM_OUTPUT_CHANNELS  0

/* High-resolution timer - uses DWT cycle counter (see platform hrt.c) */
#define HRT_TIMER               0
#define HRT_TIMER_CHANNEL       0

/* No DMA pool for now */
#define BOARD_DMA_ALLOC_POOL_SIZE 0

/* Board provides on_reset */
#define BOARD_HAS_ON_RESET 1

#define PX4_GPIO_INIT_LIST { \
		GPIO_nLED_BLUE, \
		GPIO_SPI3_CS_IMU, \
	}

/* Hardfault log path */
#define HARDFAULT_ULOG_PATH "/fs/microsd"
#define HARDFAULT_MAX_ULOG_FILE_LEN 80

/* Console buffer */
#define BOARD_ENABLE_CONSOLE_BUFFER

/* No IO timers for PWM yet */
#define BOARD_NUM_IO_TIMERS 0

__BEGIN_DECLS

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

extern void board_peripheral_reset(int ms);

#ifdef CONFIG_PIC32CZCA90_SQI1
extern int board_qspi_flash_init(void);
extern int board_qspi_create_partitions(struct mtd_dev_s *mtd);
#  ifdef CONFIG_PIC32CZCA90_SDMMC1
extern void board_qspi_set_sdmmc_active(bool active);
#  endif
#endif

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS

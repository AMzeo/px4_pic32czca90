/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file init.c
 *
 * PIC32CZ CA90 Curiosity Ultra board initialization for PX4.
 */

#include "board_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <syslog.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/clock.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <nuttx/usb/usbdev.h>
#include <nuttx/wqueue.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <arch/board/board.h>
#include "arm_internal.h"

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>

#ifdef CONFIG_PIC32CZCA90_SDMMC1
#  include "sam_sdmmc.h"
#endif

#ifdef CONFIG_PIC32CZCA90_SERCOM3_ISSPI
#  include "sam_spi.h"
#endif

#ifdef CONFIG_PIC32CZCA90_SERCOM5_ISI2C
#  include "sam_i2c_master.h"
#  include <nuttx/i2c/i2c_master.h>
#endif

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
__END_DECLS



/* LED1 heartbeat — LPWORK job, toggles at 1 Hz.
 * If LED1 stops blinking the scheduler has stalled.
 */
static struct work_s g_heartbeat_work;
static bool g_led1_state;

static void heartbeat_cb(FAR void *arg)
{
	g_led1_state = !g_led1_state;
	sam_portwrite(PORT_LED1, !g_led1_state);  /* PORT_LED1 active LOW */
	work_queue(LPWORK, &g_heartbeat_work, heartbeat_cb, NULL, MSEC2TICK(500));
}

/************************************************************************************
 * Name: arm_addregion
 *
 * Description:
 *   Add additional memory regions to the heap. Stub for boards with
 *   only single-region SRAM.
 ************************************************************************************/

#if CONFIG_MM_REGIONS > 1
void arm_addregion(void)
{
}
#endif

/************************************************************************************
 * Name: arm_usbinitialize
 *
 * Description:
 *   Called from NuttX up_initialize() when USB is enabled.
 *   Stub — no USB peripheral driver for PIC32CZ CA90 yet.
 ************************************************************************************/

#if defined(CONFIG_USBDEV) || defined(CONFIG_USBHOST)
void arm_usbinitialize(void)
{
}
#endif

/************************************************************************************
 * Name: board_read_VBUS_state
 ************************************************************************************/

int board_read_VBUS_state(void)
{
	return 0;
}

/************************************************************************************
 * USB device controller stubs — no USB peripheral driver for PIC32CZ CA90 yet.
 * These allow the build to link with CONFIG_USBDEV=y / CONFIG_CDCACM=y.
 ************************************************************************************/

#ifdef CONFIG_USBDEV
int usbdev_register(FAR struct usbdevclass_driver_s *driver)
{
	(void)driver;
	return -ENODEV;
}

int usbdev_unregister(FAR struct usbdevclass_driver_s *driver)
{
	(void)driver;
	return -ENODEV;
}
#endif

/************************************************************************************
 * Name: board_peripheral_reset
 ************************************************************************************/
__EXPORT void board_peripheral_reset(int ms)
{
	(void)ms;
}

/************************************************************************************
 * Name: board_on_reset
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
        (void)status;
}

/************************************************************************************
 * Name: sam_board_initialize
 *
 * Description:
 *   Called from NuttX __start() in sam_start.c after clocks and memory
 *   are configured but before any devices have been initialized.
 *   The name must match exactly what sam_start.c calls.
 ************************************************************************************/

__EXPORT void
sam_board_initialize(void)
{
	board_on_reset(-1);

	/* Configure LEDs */
	board_autoled_initialize();
	led_init();

	/* Configure GPIO pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.
 ****************************************************************************/

__EXPORT int board_app_initialize(uintptr_t arg)
{
	syslog(LOG_INFO, "[boot] PIC32CZ CA90 board_app_initialize\n");

	px4_platform_init();

	/* Initialize HRT (DWT-based for now) */
	hrt_init();

	/* Turn on LED0 to show NuttX booted */
	led_on(0);

	/* Start LED1 heartbeat: toggles at 1 Hz to confirm scheduler is alive */
	g_led1_state = false;
	work_queue(LPWORK, &g_heartbeat_work, heartbeat_cb, NULL, MSEC2TICK(500));
	syslog(LOG_INFO, "[boot] LED1 heartbeat queued\n");

#ifdef CONFIG_PIC32CZCA90_SQI1
	{
		/* SQI1 and SDMMC1 share the same physical pins (mux H=7 vs mux I=8).
		 * Mux for SQI1 first, init flash + MTD partitions, then the SDMMC1
		 * block below remuxes them to func I=8 via its own sam_portconfig calls. */
		syslog(LOG_INFO, "[boot] SQI1 init start\n");
		sam_portconfig(PORT_SQI1_CLK);
		sam_portconfig(PORT_SQI1_CS0);
		sam_portconfig(PORT_SQI1_IO0);
		sam_portconfig(PORT_SQI1_IO1);
		sam_portconfig(PORT_SQI1_IO2);
		sam_portconfig(PORT_SQI1_IO3);

		int sqi_ret = board_qspi_flash_init();
		if (sqi_ret < 0) {
			syslog(LOG_ERR, "[boot] board_qspi_flash_init: %d\n", sqi_ret);
		} else {
			syslog(LOG_INFO, "[boot] SQI1 flash init OK, creating partitions\n");
			sqi_ret = board_qspi_create_partitions(NULL);
			if (sqi_ret < 0) {
				syslog(LOG_ERR, "[boot] board_qspi_create_partitions: %d\n", sqi_ret);
			} else {
				syslog(LOG_INFO, "[boot] SQI1 partitions ready\n");
			}
		}
	}
#endif /* CONFIG_PIC32CZCA90_SQI1 */

#ifdef CONFIG_PIC32CZCA90_SDMMC1
	{
		/* Configure SDMMC1 peripheral pins — mux I (function 8).
		 * CLK is output-only; CMD and DAT0-3 are bidirectional (INEN set).
		 * CD is a plain GPIO input with pullup (no PMUX).
		 * These pins are shared with SQI1; sam_portconfig calls here remux
		 * them from func H (SQI1=7) to func I (SDMMC1=8).
		 */
		sam_portconfig(PORT_SDMMC1_CLK);
		sam_portconfig(PORT_SDMMC1_CMD);
		sam_portconfig(PORT_SDMMC1_DAT0);
#  ifdef CONFIG_PIC32CZCA90_SDMMC1_WIDTH_D1_D4
		sam_portconfig(PORT_SDMMC1_DAT1);
		sam_portconfig(PORT_SDMMC1_DAT2);
		sam_portconfig(PORT_SDMMC1_DAT3);
#  endif
		/* CD (PC28) is configured inside sam_sdmmc1_initialize(). */

		int ret = sam_sdmmc1_slotinitialize(0);
		if (ret < 0)
		{
			syslog(LOG_ERR, "[boot] sam_sdmmc1_slotinitialize: %d\n", ret);
		}
		else
		{
#ifdef CONFIG_PIC32CZCA90_SQI1
			board_qspi_set_sdmmc_active(true);
			syslog(LOG_INFO, "[boot] SDMMC1 ready, pin-mux arbitration active\n");
#else
			syslog(LOG_INFO, "[boot] SDMMC1 ready (SQI1 disabled, no pin conflict)\n");
#endif
		}
		/* Mount is handled by rcS: mount -t vfat /dev/mmcsd0 /fs/microsd */
	}
#endif /* CONFIG_PIC32CZCA90_SDMMC1 */

#ifdef CONFIG_PIC32CZCA90_SERCOM3_ISSPI
	{
		FAR struct spi_dev_s *spi3 = sam_spibus_initialize(3);
		if (!spi3) {
			syslog(LOG_ERR, "[boot] SPI3 init failed\n");
		} else {
			syslog(LOG_INFO, "[boot] SPI3 (SERCOM3) ready\n");
		}
	}
#endif

#ifdef CONFIG_PIC32CZCA90_SERCOM5_ISI2C
	{
		FAR struct i2c_master_s *i2c5 = sam_i2cbus_initialize(5);
		if (!i2c5) {
			syslog(LOG_ERR, "[boot] I2C5 init failed\n");
		} else {
			int ret = i2c_register(i2c5, 5);
			if (ret < 0) {
				syslog(LOG_ERR, "[boot] i2c_register(5): %d\n", ret);
			} else {
				syslog(LOG_INFO, "[boot] I2C5 (SERCOM5) registered as /dev/i2c5\n");
			}
		}
	}
#endif

	syslog(LOG_INFO, "[boot] PIC32CZ CA90 board initialization complete\n");

	return OK;
}

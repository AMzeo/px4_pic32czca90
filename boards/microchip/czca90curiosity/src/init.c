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
#include <nuttx/usb/usbdev.h>

#include <arch/board/board.h>
#include "arm_internal.h"

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
__END_DECLS

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
	UNUSED(ms);
}

/************************************************************************************
 * Name: board_on_reset
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
	UNUSED(status);
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

	/* Turn on the user LED to show we're alive */
	led_on(0);

	syslog(LOG_INFO, "[boot] PIC32CZ CA90 board initialization complete\n");

	return OK;
}

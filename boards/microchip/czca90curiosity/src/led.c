/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file led.c
 *
 * LED backend for PIC32CZ CA90 Curiosity Ultra.
 * LED0 on PC21, active high.
 */

#include <px4_platform_common/px4_config.h>

#include <stdbool.h>

#include "sam_port.h"
#include "hardware/sam_pinmap.h"
#include "board_config.h"

#include <nuttx/board.h>
#include <arch/board/board.h>

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
extern void led_toggle(int led);
__END_DECLS

static uint32_t g_ledmap[] = {
	GPIO_nLED_BLUE,   /* Indexed by LED_BLUE (PC21) */
};

__EXPORT void led_init(void)
{
	for (size_t l = 0; l < (sizeof(g_ledmap) / sizeof(g_ledmap[0])); l++) {
		sam_portconfig(g_ledmap[l]);
	}
}

static void phy_set_led(int led, bool state)
{
	/* LED0 is active HIGH on Curiosity Ultra */
	if (led == 0) {
		sam_portwrite(g_ledmap[led], state);
	}
}

static bool phy_get_led(int led)
{
	if (led == 0) {
		return sam_portread(g_ledmap[led]);
	}

	return false;
}

__EXPORT void led_on(int led)
{
	phy_set_led(led, true);
}

__EXPORT void led_off(int led)
{
	phy_set_led(led, false);
}

__EXPORT void led_toggle(int led)
{
	phy_set_led(led, !phy_get_led(led));
}

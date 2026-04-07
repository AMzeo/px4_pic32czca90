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
 * LED0 = PB21, active LOW (DS70005522C Table 2-11)  → GPIO_nLED_BLUE
 * LED1 = PB22, active LOW (DS70005522C Table 2-11)  → GPIO_nLED_GREEN
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

static const uint32_t g_ledmap[] = {
	GPIO_nLED_BLUE,    /* LED_BLUE  (0): PB21, armed/status  */
	GPIO_nLED_GREEN,   /* LED_GREEN (1): PB22, activity/fault */
};

__EXPORT void led_init(void)
{
	for (size_t l = 0; l < (sizeof(g_ledmap) / sizeof(g_ledmap[0])); l++) {
		sam_portconfig(g_ledmap[l]);
	}
}

static void phy_set_led(int led, bool on)
{
	/* Both LEDs are active LOW */
	if ((unsigned)led < (sizeof(g_ledmap) / sizeof(g_ledmap[0]))) {
		sam_portwrite(g_ledmap[led], !on);
	}
}

static bool phy_get_led(int led)
{
	if ((unsigned)led < (sizeof(g_ledmap) / sizeof(g_ledmap[0]))) {
		/* active LOW: LOW output means LED is on */
		return !sam_portread(g_ledmap[led]);
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

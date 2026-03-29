/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_reset.cpp
 * PIC32CZ CA90 board reset implementation
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/shutdown.h>
#include <systemlib/px4_macros.h>
#include <errno.h>
#include <nuttx/board.h>

#ifdef CONFIG_BOARDCTL_RESET

static const uint32_t modes[] = {
	/*                                        to  tb   */
	/* BOARD_RESET_MODE_CLEAR                  5   y   */  0,
	/* BOARD_RESET_MODE_BOOT_TO_BL             0   n   */  0xb007b007,
	/* BOARD_RESET_MODE_BOOT_TO_VALID_APP      0   y   */  0xb0070002,
	/* BOARD_RESET_MODE_CAN_BL                10   n   */  0xb0080000,
	/* BOARD_RESET_MODE_RTC_BOOT_FWOK          0   n   */  0xb0093a26
};

int board_configure_reset(reset_mode_e mode, uint32_t arg)
{
	int rv = -1;

	if (mode < arraySize(modes)) {
		arg = mode == BOARD_RESET_MODE_CAN_BL ? arg & ~0xff : 0;
		/* TODO: store in backup register for persistence across reset */
		(void)(modes[mode] | arg);
		rv = OK;
	}

	return rv;
}

int board_reset(int status)
{
	if (status == REBOOT_TO_BOOTLOADER) {
		board_configure_reset(BOARD_RESET_MODE_BOOT_TO_BL, 0);
	}

#if defined(BOARD_HAS_ON_RESET)
	board_on_reset(status);
#endif

	up_systemreset();

	return 0;
}

#endif /* CONFIG_BOARDCTL_RESET */

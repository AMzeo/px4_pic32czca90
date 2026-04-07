/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_mcu_version.c
 * PIC32CZ CA90 SoC version identification
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/defines.h>

/* TODO: Enable DSU APB clock (MCLK_ID_APB_DSU, not yet defined) before
 * reading CA90_DSU_DID.  Until then return a fixed stub to avoid bus stall. */
#define CA90_DSU_DID    0x44000120

/* Extract fields from DID */
#define DID_REVISION(x)     (((x) >> 8) & 0xF)

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	/* Stub: return fixed values until DSU clock is enabled */
	*revstr = "PIC32CZCA90";
	*rev = 'A';

	if (errata) {
		*errata = NULL;
	}

	return 0;
}

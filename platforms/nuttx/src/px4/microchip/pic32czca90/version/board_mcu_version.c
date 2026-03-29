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
#include <nuttx/arch.h>
#include "arm_internal.h"

/* PIC32CZ CA90 DSU DID register */
#define CA90_DSU_DID    0x41002018

/* Extract fields from DID */
#define DID_PROCESSOR(x)    (((x) >> 28) & 0xF)
#define DID_FAMILY(x)       (((x) >> 23) & 0x1F)
#define DID_SERIES(x)       (((x) >> 16) & 0x3F)
#define DID_DEVSEL(x)       ((x) & 0xFF)
#define DID_REVISION(x)     (((x) >> 8) & 0xF)

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	uint32_t did = getreg32(CA90_DSU_DID);
	uint32_t revision = DID_REVISION(did);

	*revstr = "PIC32CZCA90";

	switch (revision) {
	case 0:
		*rev = 'A';
		break;

	case 1:
		*rev = 'B';
		break;

	default:
		*rev = '?';
		break;
	}

	if (errata) {
		*errata = NULL;
	}

	return 0;
}

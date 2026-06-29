/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_mcu_version.c
 * PIC32CZ CA90 SoC version identification via DSU DID register
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/defines.h>
#include "arm_internal.h"
#include "hardware/sam_mclk.h"

#define CA90_DSU_DID       0x44000120u

#define DID_REVISION(x)    (((x) >> 28) & 0xFu)
#define DID_PRODUCT(x)     (((x) >> 20) & 0xFFu)
#define DID_DEVSEL(x)      (((x) >> 12) & 0xFFu)

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	uint32_t clkmsk = getreg32(SAM_MCLK_CLKMSK(0));
	clkmsk |= SAM_MCLK_CLKMSK_BIT(MCLK_ID_APB_DSU);
	putreg32(clkmsk, SAM_MCLK_CLKMSK(0));

	uint32_t did = getreg32(CA90_DSU_DID);
	uint32_t revision = DID_REVISION(did);

	*revstr = "PIC32CZCA90";
	*rev = (char)('A' + revision);

	if (errata) {
		*errata = NULL;
	}

	return (int)revision;
}

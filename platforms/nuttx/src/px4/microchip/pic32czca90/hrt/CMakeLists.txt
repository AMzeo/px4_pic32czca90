############################################################################
#
#   Copyright (c) 2024 PX4 Development Team. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
############################################################################

px4_add_library(arch_hrt
	hrt.c
)
target_compile_options(arch_hrt
	PRIVATE
		${MAX_CUSTOM_OPT_LEVEL}
		-Wno-cast-align
)

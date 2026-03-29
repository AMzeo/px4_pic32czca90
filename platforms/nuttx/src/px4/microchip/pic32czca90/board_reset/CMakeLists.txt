############################################################################
#
#   Copyright (c) 2024 PX4 Development Team. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
############################################################################

px4_add_library(arch_board_reset
	board_reset.cpp
)

if (NOT DEFINED CONFIG_BUILD_FLAT)
	target_link_libraries(arch_board_reset PRIVATE nuttx_karch)
else()
	target_link_libraries(arch_board_reset PRIVATE nuttx_arch)
endif()

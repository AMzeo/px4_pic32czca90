############################################################################
#
#   Copyright (c) 2024 PX4 Development Team. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
############################################################################

px4_add_library(arch_gpio
	pic32czca90_gpiosetevent.c
)

target_link_libraries(arch_gpio PRIVATE nuttx_arch)

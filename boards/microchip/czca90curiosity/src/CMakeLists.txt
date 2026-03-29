############################################################################
#
#   Copyright (c) 2024 PX4 Development Team. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
############################################################################

add_library(drivers_board
	init.c
	led.c
)

target_link_libraries(drivers_board
	PRIVATE
		arch_io_pins
		drivers__led
		nuttx_arch
		nuttx_drivers
		px4_layer
)

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
	qspi.c
	spi.cpp
)

# HRT accuracy test command — accessible from NSH as 'hrt_test'
px4_add_module(
	MODULE board_hrt_test
	MAIN hrt_test
	SRCS
		hrt_test.c
)

# NVM (FCR/FCW flash controller) register and erase/write test — 'nvm_test [rw]'
px4_add_module(
	MODULE board_nvm_test
	MAIN nvm_test
	SRCS
		nvm_test.c
	DEPENDS
		arch_io_pins
)

# SQI flash write path test — 'sqi_test'
px4_add_module(
	MODULE board_sqi_test
	MAIN sqi_test
	SRCS
		sqi_test.c
)


target_link_libraries(drivers_board
	PRIVATE
		arch_io_pins
		drivers__led
		nuttx_arch
		nuttx_drivers
		px4_layer
)

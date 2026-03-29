/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/* Minimal hw_description.h for PIC32CZ CA90 */

#pragma once

#include <stdint.h>

namespace GPIO
{
enum Port {
	PortA = 0,
	PortB,
	PortC,
	PortD,
};

enum Pin {
	Pin0 = 0, Pin1, Pin2, Pin3, Pin4, Pin5, Pin6, Pin7,
	Pin8, Pin9, Pin10, Pin11, Pin12, Pin13, Pin14, Pin15,
	Pin16, Pin17, Pin18, Pin19, Pin20, Pin21, Pin22, Pin23,
	Pin24, Pin25, Pin26, Pin27, Pin28, Pin29, Pin30, Pin31,
};

struct GPIOPin {
	Port port;
	Pin pin;
};
} // namespace GPIO

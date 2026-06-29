/****************************************************************************
 *
 *   Copyright (c) 2024-2025 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

#pragma once

#include <stdint.h>

namespace GPIO
{
enum Port {
	PortA = 0,
	PortB,
	PortC,
	PortD,
	PortE,
	PortF,
	PortG,
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

namespace TCC
{
enum TCCModule {
	TCC1 = 0,
	TCC7 = 1,
};

enum Channel {
	Channel0 = 0,
	Channel1,
	Channel2,
	Channel3,
	Channel4,
	Channel5,
	Channel6,
	Channel7,
};

struct TCCChannel {
	TCCModule module;
	Channel channel;
};
} // namespace TCC

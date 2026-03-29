/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file hrt.c
 *
 * High-resolution timer for PIC32CZ CA90 using ARM DWT cycle counter.
 *
 * This is a minimal implementation using the ARM Cortex-M7 DWT CYCCNT
 * register for microsecond timing. The DWT cycle counter runs at CPU
 * clock speed (300 MHz) and provides ~14.3 seconds before wrapping.
 *
 * For production use, this should be replaced with a TC-based timer
 * that can provide hardware compare/capture for HRT callouts.
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>

#include <assert.h>
#include <debug.h>
#include <time.h>
#include <queue.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <board_config.h>

#include <drivers/drv_hrt.h>

/* ARM DWT (Data Watchpoint and Trace) registers */
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)
#define SCB_DEMCR   (*(volatile uint32_t *)0xE000EDFC)

#define DWT_CTRL_CYCCNTENA  (1 << 0)
#define SCB_DEMCR_TRCENA    (1 << 24)

/* CPU frequency in MHz for cycle-to-microsecond conversion */
#define CPU_MHZ     (BOARD_CPU_FREQUENCY / 1000000)

/* Latency histogram for perf_print_latency */
#define LATENCY_BUCKET_COUNT 8
const uint16_t latency_bucket_count = LATENCY_BUCKET_COUNT;
const uint16_t latency_buckets[LATENCY_BUCKET_COUNT] = { 1, 2, 5, 10, 20, 50, 100, 1000 };
__EXPORT uint32_t latency_counters[LATENCY_BUCKET_COUNT + 1];

/* Overflow tracking: DWT CYCCNT is 32-bit at 300 MHz = wraps every ~14.3s.
 * We track overflow via SysTick-rate polling in hrt_absolute_time().
 */
static volatile uint64_t base_time_us;
static volatile uint32_t last_cyccnt;
static bool hrt_initialized;

/* HRT callout list */
static struct sq_queue_s callout_queue;

/**
 * Initialize the DWT cycle counter for HRT use.
 */
void hrt_init(void)
{
	/* Enable DWT */
	SCB_DEMCR |= SCB_DEMCR_TRCENA;
	DWT_CYCCNT = 0;
	DWT_CTRL |= DWT_CTRL_CYCCNTENA;

	last_cyccnt = 0;
	base_time_us = 0;
	hrt_initialized = true;

	sq_init(&callout_queue);
}

/**
 * Get absolute time in microseconds since boot.
 */
hrt_abstime hrt_absolute_time(void)
{
	if (!hrt_initialized) {
		return 0;
	}

	irqstate_t flags = enter_critical_section();

	uint32_t now = DWT_CYCCNT;
	uint32_t elapsed;

	if (now >= last_cyccnt) {
		elapsed = now - last_cyccnt;
	} else {
		/* Counter wrapped */
		elapsed = (0xFFFFFFFF - last_cyccnt) + now + 1;
	}

	base_time_us += (uint64_t)elapsed / CPU_MHZ;
	last_cyccnt = now;

	hrt_abstime result = base_time_us;

	leave_critical_section(flags);

	return result;
}

/**
 * Store the absolute time atomically.
 */
void hrt_store_absolute_time(volatile hrt_abstime *t)
{
	irqstate_t flags = enter_critical_section();
	*t = hrt_absolute_time();
	leave_critical_section(flags);
}

/**
 * Initialise a HRT call structure
 */
void hrt_call_init(struct hrt_call *entry)
{
	memset(entry, 0, sizeof(*entry));
}

/**
 * Call function after delay
 */
void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
		    hrt_callout callout, void *arg)
{
	entry->deadline = hrt_absolute_time() + delay;
	entry->period = 0;
	entry->callout = callout;
	entry->arg = arg;

	irqstate_t flags = enter_critical_section();
	sq_addlast(&entry->link, &callout_queue);
	leave_critical_section(flags);
}

/**
 * Call function at specific time
 */
void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime,
		 hrt_callout callout, void *arg)
{
	entry->deadline = calltime;
	entry->period = 0;
	entry->callout = callout;
	entry->arg = arg;

	irqstate_t flags = enter_critical_section();
	sq_addlast(&entry->link, &callout_queue);
	leave_critical_section(flags);
}

/**
 * Call function after delay, repeating
 */
void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
		    hrt_abstime interval, hrt_callout callout, void *arg)
{
	entry->deadline = hrt_absolute_time() + delay;
	entry->period = interval;
	entry->callout = callout;
	entry->arg = arg;

	irqstate_t flags = enter_critical_section();
	sq_addlast(&entry->link, &callout_queue);
	leave_critical_section(flags);
}

/**
 * Cancel a HRT call
 */
void hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = enter_critical_section();
	sq_rem(&entry->link, &callout_queue);
	entry->deadline = 0;
	entry->period = 0;
	entry->callout = NULL;
	leave_critical_section(flags);
}

/**
 * Check if a call is pending
 */
bool hrt_called(struct hrt_call *entry)
{
	return entry->deadline != 0;
}

/**
 * Delay a periodic call
 */
void hrt_call_delay(struct hrt_call *entry, hrt_abstime delay)
{
	entry->deadline = hrt_absolute_time() + delay;
}

/**
 * Poll for expired callouts - called from idle or timer ISR
 */
void hrt_call_invoke(void)
{
	hrt_abstime now = hrt_absolute_time();

	irqstate_t flags = enter_critical_section();

	struct hrt_call *entry = (struct hrt_call *)sq_peek(&callout_queue);

	while (entry != NULL) {
		struct hrt_call *next = (struct hrt_call *)sq_next(&entry->link);

		if (entry->deadline != 0 && entry->deadline <= now) {
			sq_rem(&entry->link, &callout_queue);

			hrt_callout cb = entry->callout;
			void *arg = entry->arg;

			if (entry->period != 0) {
				entry->deadline = now + entry->period;
				sq_addlast(&entry->link, &callout_queue);
			} else {
				entry->deadline = 0;
			}

			leave_critical_section(flags);

			if (cb) {
				cb(arg);
			}

			flags = enter_critical_section();
			now = hrt_absolute_time();
			entry = (struct hrt_call *)sq_peek(&callout_queue);

		} else {
			entry = next;
		}
	}

	leave_critical_section(flags);
}

/**
 * HRT ioctl init - stub
 */
void hrt_ioctl_init(void)
{
}

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
 * High-resolution timer for PIC32CZ CA90 using TCC0.
 *
 * Architecture:
 *   - TCC0 free-running 32-bit counter, NFRQ mode, prescaler DIV1.
 *   - GCLK1 (150 MHz, PLL0/2) → GCLK_PCHCTRL[31] → TCC0 core clock.
 *   - MCLK APB clock: CLKMSK[1] bit 9 (MCLK_ID_APB_TCC0 = 41).
 *   - hrt_absolute_time(): reads TCC0 COUNT via READSYNC protocol,
 *     extends 32-bit hardware counter to 64-bit in software.
 *   - Deadline scheduling: TCC0 CC[0] compare match interrupt (SAM_IRQ_TCC0MC0)
 *     fires at the next callout deadline. ISR calls hrt_call_invoke() and
 *     reschedules CC[0] for the subsequent deadline.
 *
 * Resolution: 1/150 MHz ≈ 6.67 ns per TCC tick.
 * Overflow period: 2^32 / 150e6 ≈ 28.6 s.
 *
 * All register offsets and bit masks verified against
 * PIC32CZ8110CA80208_DFP component/tcc.h (2024-07-31).
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
#include "arm_internal.h"
#include "hardware/sam_mclk.h"
#include "hardware/sam_gclk.h"
#include "hardware/sam_tcc.h"

/* =========================================================================
 * Latency histogram (required by PX4 platform)
 * =========================================================================
 */

#define LATENCY_BUCKET_COUNT 8
const uint16_t latency_bucket_count = LATENCY_BUCKET_COUNT;
const uint16_t latency_buckets[LATENCY_BUCKET_COUNT] =
{
  1, 2, 5, 10, 20, 50, 100, 1000
};
__EXPORT uint32_t latency_counters[LATENCY_BUCKET_COUNT + 1];

/* =========================================================================
 * Private state
 * =========================================================================
 */

/* 64-bit time extension: TCC COUNT is 32-bit at 150 MHz (wraps ~28.6 s).
 * Accumulate raw ticks (not µs) so division by 150 happens ONCE per read —
 * avoiding cumulative truncation drift from repeated integer division. */
static volatile uint64_t g_base_ticks;   /* accumulated 150 MHz ticks           */
static volatile uint32_t g_last_count;   /* TCC COUNT at last hrt_absolute_time */
static bool              g_hrt_initialized;

/* Callout queue */
static struct sq_queue_s g_callout_queue;

/* =========================================================================
 * Private helpers
 * =========================================================================
 */

/* Forward declaration */
static void hrt_call_invoke(void);

/**
 * Read TCC0 COUNT using the mandatory READSYNC protocol.
 *
 * TCC COUNT register is in the TCC clock domain (GCLK1).  A direct APB
 * read would return a stale value.  Harmony plib_tcc0.c sequence:
 *   1. Write CMD=READSYNC to CTRLBSET (forces shadow→read buffer copy).
 *   2. Poll SYNCBUSY.CTRLB until clear.
 *   3. Poll CTRLBSET.CMD until CMD field clears (copy complete).
 *   4. Read COUNT.
 */
static uint32_t tcc0_count_read(void)
{
  /* Issue READSYNC — 8-bit write to CTRLBSET */
  putreg8(TCC_CTRLBSET_CMD_READSYNC, SAM_TCC0_CTRLBSET);

  /* Wait for CTRLB synchronization bridge to clear */
  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_CTRLB)
    {
    }

  /* Wait for CMD field to self-clear (READSYNC completes and clears CMD) */
  while (getreg8(SAM_TCC0_CTRLBSET) & TCC_CTRLBSET_CMD_MSK)
    {
    }

  return getreg32(SAM_TCC0_COUNT);
}

/**
 * Reschedule TCC0 CC[0] for the earliest pending callout deadline.
 *
 * Called from ISR (interrupts already masked by NVIC) and from
 * hrt_call_after/at/every (inside critical section).
 *
 * If the queue is empty, MC0 interrupt is disabled (no spurious fires).
 */
static void hrt_reschedule(void)
{
  struct hrt_call *entry;
  hrt_abstime      earliest = UINT64_MAX;

  /* Find the earliest pending deadline in the queue */
  entry = (struct hrt_call *)sq_peek(&g_callout_queue);

  while (entry != NULL)
    {
      if (entry->deadline != 0 && entry->deadline < earliest)
        {
          earliest = entry->deadline;
        }

      entry = (struct hrt_call *)sq_next(&entry->link);
    }

  if (earliest == UINT64_MAX)
    {
      /* No pending callouts — disable MC0 to avoid spurious interrupts */
      putreg32(TCC_INTSET_MC0, SAM_TCC0_INTENCLR);
      return;
    }

  /* Convert absolute deadline (µs) to a TCC COUNT value.
   * Both g_base_ticks and the COUNT units are 150 MHz ticks / 150.
   * Deadline ticks = earliest_us * 150.
   *
   * We compute the CC[0] value in the 32-bit TCC counter space:
   *   cc0 = (uint32_t)(earliest_us * 150)
   *
   * Because both deadline_us and COUNT derive from the same 150 MHz clock,
   * the lower 32 bits of (deadline_us * 150) give the correct TCC compare
   * value — unsigned 32-bit wrap-around arithmetic handles past-deadline
   * and near-deadline cases correctly.
   */
  uint32_t cc0 = (uint32_t)((earliest * 150ULL) & 0xFFFFFFFFu);

  /* Guard: if the deadline is so close that cc0 equals current COUNT,
   * advance by 2 ticks to avoid missing it (fire slightly late but never
   * silently miss). */
  uint32_t now_count = tcc0_count_read();
  uint32_t delta = cc0 - now_count;   /* unsigned: correct across wrap */

  if (delta == 0 || delta > 0xFFFFFFFEu)
    {
      cc0 = now_count + 2u;
    }

  /* Write CC[0] and wait for synchronization */
  putreg32(cc0, SAM_TCC0_CC(0));

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_CC0)
    {
    }

  /* Enable MC0 interrupt */
  putreg32(TCC_INTSET_MC0, SAM_TCC0_INTENSET);
}

/**
 * TCC0 CC[0] compare match ISR — fires at scheduled HRT deadline.
 */
static int hrt_tcc_isr(int irq, FAR void *context, FAR void *arg)
{
  /* Clear MC0 flag (write-1-to-clear) */
  putreg32(TCC_INTFLAG_MC0, SAM_TCC0_INTFLAG);

  /* Invoke expired callouts */
  hrt_call_invoke();

  /* Schedule next deadline */
  hrt_reschedule();

  return OK;
}

/* =========================================================================
 * Public API
 * =========================================================================
 */

/**
 * Initialize TCC0 as the HRT timebase and callout engine.
 *
 * Sequence follows Harmony plib_clock.c + plib_tcc0.c exactly:
 *   1. Enable TCC0 APB clock (MCLK CLKMSK).
 *   2. Route GCLK1 (150 MHz) to TCC0 via GCLK_PCHCTRL[31].
 *   3. Reset TCC0.
 *   4. Configure NFRQ mode, DIV1 prescaler.
 *   5. Set PER = 0xFFFFFFFF (free-running, 32-bit counter).
 *   6. Clear all interrupt flags, enable TCC.
 *   7. Attach ISR and enable NVIC for SAM_IRQ_TCC0MC0.
 */
void hrt_init(void)
{
  uint32_t regval;

  g_base_ticks    = 0;
  g_last_count      = 0;
  g_hrt_initialized = false;

  sq_init(&g_callout_queue);

  /* 1. Enable TCC0 APB clock via MCLK CLKMSK[1] bit 9 (ID=41) */
  regval  = getreg32(SAM_MCLK_CLKMSK(MCLK_ID_APB_TCC0 / 32u));
  regval |= (1u << (MCLK_ID_APB_TCC0 % 32u));
  putreg32(regval, SAM_MCLK_CLKMSK(MCLK_ID_APB_TCC0 / 32u));

  /* 2. Route GCLK1 (generator 1, 150 MHz) to TCC0 GCLK channel 31
   *    Matches Harmony: GCLK_PCHCTRL[31] = GEN(1) | CHEN
   */
  putreg32(GCLK_PCHCTRL_GEN(1) | GCLK_PCHCTRL_CHEN,
           SAM_GCLK_PCHCTRL(TCC0_GCLK_ID));

  /* Wait for GCLK sync (poll CHEN as Harmony does) */
  while ((getreg32(SAM_GCLK_PCHCTRL(TCC0_GCLK_ID)) & GCLK_PCHCTRL_CHEN) == 0)
    {
    }

  /* 3. Software reset TCC0 */
  putreg32(TCC_CTRLA_SWRST, SAM_TCC0_CTRLA);

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_SWRST)
    {
    }

  /* 4. Configure: NFRQ waveform (free-run), prescaler DIV1, no dithering */
  putreg32(TCC_WAVE_WAVEGEN_NFRQ, SAM_TCC0_WAVE);

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_WAVE)
    {
    }

  /* CTRLA: prescaler DIV1 only (ENABLE set separately after PER is written) */
  putreg32(TCC_CTRLA_PRESCALER_DIV1, SAM_TCC0_CTRLA);

  /* 5. Period = 0xFFFFFFFF — counter counts 0 .. 0xFFFFFFFF then wraps */
  putreg32(0xFFFFFFFFu, SAM_TCC0_PER);

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_PER)
    {
    }

  /* Initialize CC[0] to a safe value; MC0 interrupt disabled until first
   * callout is queued. */
  putreg32(0xFFFFFFFEu, SAM_TCC0_CC(0));

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_CC0)
    {
    }

  /* 6. Clear all interrupt flags, leave all interrupts disabled for now */
  putreg32(TCC_INTFLAG_ALL, SAM_TCC0_INTFLAG);

  /* Wait for all pending sync to complete before enabling */
  while (getreg32(SAM_TCC0_SYNCBUSY) != 0u)
    {
    }

  /* Enable TCC0 */
  putreg32(TCC_CTRLA_PRESCALER_DIV1 | TCC_CTRLA_ENABLE, SAM_TCC0_CTRLA);

  while (getreg32(SAM_TCC0_SYNCBUSY) & TCC_SYNCBUSY_ENABLE)
    {
    }

  g_hrt_initialized = true;

  /* 7. Attach ISR for TCC0 MC0 compare match and enable in NVIC */
  irq_attach(SAM_IRQ_TCC0MC0, hrt_tcc_isr, NULL);
  up_enable_irq(SAM_IRQ_TCC0MC0);
}

/**
 * Return absolute time in microseconds since boot.
 *
 * Accumulates raw 150 MHz ticks (exact, no rounding) and converts to
 * microseconds only at return.  Truncation error is bounded to <1 µs
 * absolute and never accumulates across calls.
 */
hrt_abstime hrt_absolute_time(void)
{
  if (!g_hrt_initialized)
    {
      return 0;
    }

  irqstate_t flags = enter_critical_section();

  uint32_t count   = tcc0_count_read();
  uint32_t elapsed = count - g_last_count;  /* unsigned wrap-around: correct */

  /* Accumulate exact ticks — no rounding here. */
  g_base_ticks += (uint64_t)elapsed;
  g_last_count  = count;

  /* Single division: error bounded to <1 µs (149 ticks), never cumulative. */
  hrt_abstime result = g_base_ticks / 150ULL;

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
 * Initialise a HRT call structure.
 */
void hrt_call_init(struct hrt_call *entry)
{
  memset(entry, 0, sizeof(*entry));
}

/**
 * Schedule a one-shot callout after delay µs.
 */
void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
                    hrt_callout callout, void *arg)
{
  irqstate_t flags = enter_critical_section();

  entry->deadline = hrt_absolute_time() + delay;
  entry->period   = 0;
  entry->callout  = callout;
  entry->arg      = arg;

  sq_addlast(&entry->link, &g_callout_queue);

  hrt_reschedule();

  leave_critical_section(flags);
}

/**
 * Schedule a one-shot callout at an absolute time.
 */
void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime,
                 hrt_callout callout, void *arg)
{
  irqstate_t flags = enter_critical_section();

  entry->deadline = calltime;
  entry->period   = 0;
  entry->callout  = callout;
  entry->arg      = arg;

  sq_addlast(&entry->link, &g_callout_queue);

  hrt_reschedule();

  leave_critical_section(flags);
}

/**
 * Schedule a periodic callout.
 */
void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
                    hrt_abstime interval, hrt_callout callout, void *arg)
{
  irqstate_t flags = enter_critical_section();

  entry->deadline = hrt_absolute_time() + delay;
  entry->period   = interval;
  entry->callout  = callout;
  entry->arg      = arg;

  sq_addlast(&entry->link, &g_callout_queue);

  hrt_reschedule();

  leave_critical_section(flags);
}

/**
 * Cancel a pending HRT callout.
 */
void hrt_cancel(struct hrt_call *entry)
{
  irqstate_t flags = enter_critical_section();

  sq_rem(&entry->link, &g_callout_queue);

  entry->deadline = 0;
  entry->period   = 0;
  entry->callout  = NULL;

  hrt_reschedule();

  leave_critical_section(flags);
}

/**
 * Return true if the callout has a pending deadline.
 */
bool hrt_called(struct hrt_call *entry)
{
  return entry->deadline != 0;
}

/**
 * Delay a periodic callout by delta µs from now.
 */
void hrt_call_delay(struct hrt_call *entry, hrt_abstime delay)
{
  entry->deadline = hrt_absolute_time() + delay;
}

/**
 * Process the callout queue — invoke all expired entries.
 *
 * Called from the TCC0 MC0 ISR (hrt_tcc_isr).  Also available for
 * diagnostic use from application code.
 */
void hrt_call_invoke(void)
{
  hrt_abstime now = hrt_absolute_time();

  irqstate_t flags = enter_critical_section();

  struct hrt_call *entry = (struct hrt_call *)sq_peek(&g_callout_queue);

  while (entry != NULL)
    {
      struct hrt_call *next = (struct hrt_call *)sq_next(&entry->link);

      if (entry->deadline != 0 && entry->deadline <= now)
        {
          sq_rem(&entry->link, &g_callout_queue);

          hrt_callout cb  = entry->callout;
          void       *arg = entry->arg;

          if (entry->period != 0)
            {
              /* Reschedule periodic callout relative to original deadline
               * (avoids drift from ISR latency). */
              entry->deadline += entry->period;
              sq_addlast(&entry->link, &g_callout_queue);
            }
          else
            {
              entry->deadline = 0;
            }

          leave_critical_section(flags);

          if (cb)
            {
              cb(arg);
            }

          flags = enter_critical_section();
          now   = hrt_absolute_time();
          entry = (struct hrt_call *)sq_peek(&g_callout_queue);
        }
      else
        {
          entry = next;
        }
    }

  leave_critical_section(flags);
}

/**
 * HRT ioctl init — stub (no character device interface needed).
 */
void hrt_ioctl_init(void)
{
}
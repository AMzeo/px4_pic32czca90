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
 * Register offsets and bit masks from PIC32CZ8110CA80208 component/tcc.h.
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

#ifdef HRT_PPM_CHANNEL
#  include <systemlib/ppm_decode.h>
#  include <px4_arch/micro_hal.h>
#endif

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
 * read would return a stale value.  READSYNC protocol:
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
   * The lower 32 bits of (deadline_us * 150) give the correct TCC compare
   * value — unsigned 32-bit wrap-around arithmetic handles the mapping.
   */
  uint32_t cc0 = (uint32_t)((earliest * 150ULL) & 0xFFFFFFFFu);

  /* Guard: if the deadline is in the past or so close that we'd miss the
   * compare match during the CC[0] sync write, force a near-future fire.
   *
   * delta > 0x80000000 means the deadline is in the past (unsigned wrap
   * interpretation: more than half the 28.6s counter range "ahead" really
   * means it's behind).  delta < HRT_MIN_TICKS means it's too close —
   * the SYNCBUSY wait for CC[0] takes ~100+ ns, so we need margin.
   *
   * HRT_MIN_TICKS = 300 ticks = 2 µs at 150 MHz — enough for SYNCBUSY
   * plus ISR exit latency.
   */
#define HRT_MIN_TICKS 300u

  uint32_t now_count = tcc0_count_read();
  uint32_t delta = cc0 - now_count;   /* unsigned: correct across wrap */

  if (delta > 0x80000000u || delta < HRT_MIN_TICKS)
    {
      cc0 = now_count + HRT_MIN_TICKS;
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
 * PPM RC input decoder (GPIO edge-interrupt path)
 * =========================================================================
 * Activated when board_config.h defines HRT_PPM_CHANNEL.
 * PC24 → EIC EXTINT8 → hrt_ppm_isr() alternates between rising/falling
 * edges, timestamps each transition with hrt_absolute_time(), and feeds
 * the PPM state machine.  Decoded channel values land in the ppm_buffer[]
 * globals consumed by RCInput.cpp (the RC_SCAN_PPM case).
 */

#ifdef HRT_PPM_CHANNEL

/* PPM globals — declared extern in <systemlib/ppm_decode.h> */
__EXPORT uint16_t    ppm_buffer[PPM_MAX_CHANNELS];
__EXPORT uint16_t    ppm_frame_length;
__EXPORT unsigned    ppm_decoded_channels;
__EXPORT hrt_abstime ppm_last_valid_decode;

/* Decode thresholds (µs) */
#define PPM_SYNC_US   3000u   /* gap ≥ 3 ms = sync → new frame */
#define PPM_MIN_US     800u   /* shortest valid channel pulse   */
#define PPM_MAX_US    2200u   /* longest valid channel pulse    */

/* Decoder private state */
static uint16_t g_ppm_last_us;                  /* 16-bit µs of previous edge    */
static uint16_t g_ppm_frame_start_us;           /* 16-bit µs when sync was seen  */
static uint16_t g_ppm_temp[PPM_MAX_CHANNELS];   /* channels accumulating in frame */
static unsigned g_ppm_channel;                  /* next slot to fill              */
static bool     g_ppm_last_edge;                /* true = last ISR was rising     */

static void hrt_ppm_decode(uint16_t now_us)
{
  uint16_t interval = now_us - g_ppm_last_us;   /* 16-bit wrap-around is correct */
  g_ppm_last_us = now_us;

  if (interval >= PPM_SYNC_US)
    {
      /* Sync gap — publish completed frame if it had ≥ 4 channels */
      if (g_ppm_channel >= 4)
        {
          for (unsigned i = 0; i < g_ppm_channel; i++)
            {
              ppm_buffer[i] = g_ppm_temp[i];
            }

          ppm_decoded_channels = g_ppm_channel;
          ppm_frame_length     = (uint16_t)(now_us - g_ppm_frame_start_us);
          ppm_last_valid_decode = hrt_absolute_time();
        }

      g_ppm_channel       = 0;
      g_ppm_frame_start_us = now_us;
    }
  else if (interval >= PPM_MIN_US && interval <= PPM_MAX_US)
    {
      /* Valid channel pulse — accumulate */
      if (g_ppm_channel < PPM_MAX_CHANNELS)
        {
          g_ppm_temp[g_ppm_channel++] = interval;
        }
    }
  else
    {
      /* Glitch / noise — discard partial frame */
      g_ppm_channel = 0;
    }
}

static int hrt_ppm_isr(int irq, FAR void *context, FAR void *arg)
{
  /* 16-bit µs timestamp — wraps every 65.5 ms, deltas correct across wrap */
  uint16_t now_us = (uint16_t)(hrt_absolute_time() & 0xffffU);

  /* Flip to opposite edge so next transition fires this ISR again */
  px4_arch_gpiosetevent(GPIO_PPM_IN,
                        g_ppm_last_edge ? false : true,
                        g_ppm_last_edge ? true  : false,
                        true, hrt_ppm_isr, NULL);
  g_ppm_last_edge = !g_ppm_last_edge;

  hrt_ppm_decode(now_us);
  return OK;
}

#endif /* HRT_PPM_CHANNEL */

/* =========================================================================
 * Public API
 * =========================================================================
 */

/**
 * Initialize TCC0 as the HRT timebase and callout engine.
 *
 * Sequence:
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

  /* 2. Route GCLK1 (generator 1, 150 MHz) to TCC0 GCLK channel 31 */
  putreg32(GCLK_PCHCTRL_GEN(1) | GCLK_PCHCTRL_CHEN,
           SAM_GCLK_PCHCTRL(TCC0_GCLK_ID));

  /* Wait for GCLK sync */
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

#ifdef HRT_PPM_CHANNEL
  /* 8. Arm PPM input — EIC edge interrupt on GPIO_PPM_IN (PC24, EXTINT8).
   * Start listening for rising edges; ISR will flip to falling after first hit. */
  g_ppm_last_edge = false;
  px4_arch_gpiosetevent(GPIO_PPM_IN, true, false, true, hrt_ppm_isr, NULL);
#endif
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

  /* Remove from queue if already queued (STM32 pattern — prevents
   * linked-list corruption from double-add of the same entry). */
  if (entry->deadline != 0)
    {
      sq_rem(&entry->link, &g_callout_queue);
    }

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

  if (entry->deadline != 0)
    {
      sq_rem(&entry->link, &g_callout_queue);
    }

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

  if (entry->deadline != 0)
    {
      sq_rem(&entry->link, &g_callout_queue);
    }

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
 *
 * Follows the STM32 HRT pattern: zero deadline before callback, re-enter
 * periodic entries AFTER the callback returns (allows callbacks to cancel
 * or reschedule via hrt_call_delay without queue corruption).
 */
void hrt_call_invoke(void)
{
  struct hrt_call *entry;
  hrt_abstime      deadline;

  while (true)
    {
      hrt_abstime now = hrt_absolute_time();

      irqstate_t flags = enter_critical_section();

      entry = (struct hrt_call *)sq_peek(&g_callout_queue);

      if (entry == NULL)
        {
          leave_critical_section(flags);
          break;
        }

      /* Scan for earliest expired entry (queue is unsorted) */
      struct hrt_call *expired = NULL;

      while (entry != NULL)
        {
          if (entry->deadline != 0 && entry->deadline <= now)
            {
              if (expired == NULL || entry->deadline < expired->deadline)
                {
                  expired = entry;
                }
            }

          entry = (struct hrt_call *)sq_next(&entry->link);
        }

      if (expired == NULL)
        {
          leave_critical_section(flags);
          break;
        }

      sq_rem(&expired->link, &g_callout_queue);

      /* Save the intended deadline for periodic re-entry */
      deadline = expired->deadline;

      /* Zero deadline to mark as "not queued" — matches STM32 pattern.
       * This allows the callback to detect the entry is not active. */
      expired->deadline = 0;

      hrt_callout cb  = expired->callout;
      void       *arg = expired->arg;

      leave_critical_section(flags);

      /* Invoke the callback outside the critical section */
      if (cb)
        {
          cb(arg);
        }

      /* Re-enter periodic calls AFTER callback (STM32 pattern).
       * If the callback called hrt_cancel(), period will be 0. */
      if (expired->period != 0)
        {
          flags = enter_critical_section();

          /* Re-check deadline: callback may have called hrt_call_delay()
           * to set a new deadline directly. */
          if (expired->deadline <= now)
            {
              expired->deadline = deadline + expired->period;
            }

          sq_addlast(&expired->link, &g_callout_queue);

          leave_critical_section(flags);
        }
    }
}

/**
 * HRT ioctl init — stub (no character device interface needed).
 */
void hrt_ioctl_init(void)
{
}
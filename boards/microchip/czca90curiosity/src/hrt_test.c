/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file hrt_test.c
 *
 * NSH command to verify TCC0-based HRT accuracy on PIC32CZ CA90.
 *
 * Tests:
 *   time     - Print current hrt_absolute_time()
 *   mono     - 1000-sample monotonicity check (must never go backwards)
 *   delay N  - Compare up_mdelay(N ms) to HRT delta
 *   stress   - Multi-interval accuracy table vs up_mdelay reference
 *   cross    - Cross-check HRT against SysTick tick count (CONFIG_USEC_PER_TICK)
 *
 * Usage:
 *   nsh> hrt_test time
 *   nsh> hrt_test mono
 *   nsh> hrt_test delay 1000
 *   nsh> hrt_test stress
 *   nsh> hrt_test cross
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/module.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <arch/board/board.h>

#include <drivers/drv_hrt.h>

/* =========================================================================
 * Helpers
 * =========================================================================
 */

/**
 * Print hrt_absolute_time() with full precision.
 */
static void cmd_time(void)
{
  hrt_abstime t = hrt_absolute_time();
  printf("hrt_absolute_time = %llu us  (%.6f s since boot)\n",
         (unsigned long long)t,
         (double)t / 1.0e6);
}

/**
 * Call hrt_absolute_time() N times and verify strict monotonicity.
 * Fails on first backwards or unchanged value.
 */
static void cmd_mono(void)
{
  const int N = 2000;
  hrt_abstime prev = hrt_absolute_time();
  int backward = 0;
  int stall    = 0;  /* same value twice = timer frozen */

  printf("Monotonicity check: %d samples...\n", N);

  for (int i = 0; i < N; i++)
    {
      hrt_abstime cur = hrt_absolute_time();

      if (cur < prev)
        {
          printf("  FAIL [%d]: backwards! prev=%llu cur=%llu (delta=%lld)\n",
                 i,
                 (unsigned long long)prev,
                 (unsigned long long)cur,
                 (long long)(cur - prev));
          backward++;
        }

      /* A stall of more than 10 us between consecutive reads is suspicious
       * (READSYNC takes ~6 GCLK1 cycles = ~40 ns; even with interrupts
       *  a 10 µs gap would indicate the ISR is blocking long). */
      if (cur == prev)
        {
          stall++;
        }

      prev = cur;
    }

  if (backward == 0 && stall == 0)
    {
      printf("  PASS: all %d samples monotonically increasing.\n", N);
    }
  else
    {
      if (backward > 0)
        printf("  FAIL: %d backward jumps detected.\n", backward);
      if (stall > 0)
        printf("  NOTE: %d identical consecutive samples (timer stalls).\n", stall);
    }
}

/**
 * Measure accuracy of a single up_mdelay(ms) interval.
 *
 * up_mdelay() uses a calibrated busy-wait loop independent of HRT,
 * making it a clean reference for cross-checking TCC0 ticks.
 */
static void cmd_delay(int ms)
{
  hrt_abstime t0, t1;
  int64_t measured_us, expected_us, error_us;
  double error_pct;

  expected_us = (int64_t)ms * 1000LL;

  printf("Delay test: up_mdelay(%d ms)  expected %lld us\n",
         ms, (long long)expected_us);

  t0 = hrt_absolute_time();
  up_mdelay((unsigned int)ms);
  t1 = hrt_absolute_time();

  measured_us = (int64_t)(t1 - t0);
  error_us    = measured_us - expected_us;
  error_pct   = (double)error_us / (double)expected_us * 100.0;

  printf("  Measured : %lld us\n",  (long long)measured_us);
  printf("  Error    : %+lld us  (%+.3f%%)\n", (long long)error_us, error_pct);

  if (error_pct > -2.0 && error_pct < 2.0)
    {
      printf("  Result   : PASS (within 2%%)\n");
    }
  else if (error_pct > -5.0 && error_pct < 5.0)
    {
      printf("  Result   : MARGINAL (within 5%% — check PLL lock)\n");
    }
  else
    {
      printf("  Result   : FAIL (>5%% — clock frequency mismatch?)\n");
    }
}

/**
 * Stress test across a range of intervals.
 */
static void cmd_stress(void)
{
  static const int delays_ms[] = {1, 5, 10, 50, 100, 500, 1000, 2000};
  const int n = (int)(sizeof(delays_ms) / sizeof(delays_ms[0]));
  int pass = 0, marginal = 0, fail = 0;

  printf("Stress test — HRT vs up_mdelay reference\n");
  printf("%-10s %-14s %-14s %-12s %-10s %s\n",
         "Delay(ms)", "Expected(us)", "Measured(us)", "Error(us)", "Error(%)", "Result");
  printf("%-10s %-14s %-14s %-12s %-10s %s\n",
         "----------", "--------------", "--------------", "------------",
         "----------", "------");

  for (int i = 0; i < n; i++)
    {
      int ms = delays_ms[i];
      int64_t expected = (int64_t)ms * 1000LL;

      hrt_abstime t0 = hrt_absolute_time();
      up_mdelay((unsigned int)ms);
      hrt_abstime t1 = hrt_absolute_time();

      int64_t measured = (int64_t)(t1 - t0);
      int64_t error    = measured - expected;
      double  pct      = (double)error / (double)expected * 100.0;

      const char *result;
      if (pct > -2.0 && pct < 2.0)
        { result = "PASS";     pass++;     }
      else if (pct > -5.0 && pct < 5.0)
        { result = "MARGINAL"; marginal++; }
      else
        { result = "FAIL";     fail++;     }

      printf("%-10d %-14lld %-14lld %-+12lld %-+10.3f %s\n",
             ms, (long long)expected, (long long)measured,
             (long long)error, pct, result);
    }

  printf("\nSummary: %d PASS, %d MARGINAL, %d FAIL\n", pass, marginal, fail);

  if (fail == 0 && marginal == 0)
    printf("HRT accuracy: PRODUCTION READY (all within 2%%)\n");
  else if (fail == 0)
    printf("HRT accuracy: MARGINAL — investigate PLL0 lock and GCLK1 frequency\n");
  else
    printf("HRT accuracy: FAIL — TCC0 clock source or frequency is wrong\n");
}

/**
 * Cross-check: compare HRT microseconds to NuttX tick count.
 *
 * NuttX SysTick fires at CONFIG_USEC_PER_TICK intervals (default 1000 us = 1 kHz).
 * clock_systime_ticks() returns accumulated tick count.
 *
 * Method:
 *   1. Sample both HRT and tick count simultaneously.
 *   2. Wait ~2 seconds via up_mdelay.
 *   3. Sample again.
 *   4. Compare elapsed HRT µs to elapsed_ticks × USEC_PER_TICK.
 *
 * If they agree within <0.5%, both timers are running at correct rates.
 * A mismatch indicates a GCLK/MCLK misconfiguration in one of them.
 */
static void cmd_cross(void)
{
  const int WAIT_MS = 2000;

  printf("Cross-check: HRT (TCC0 @150MHz) vs SysTick (%d us/tick)\n",
         CONFIG_USEC_PER_TICK);
  printf("Waiting %d ms...\n", WAIT_MS);

  hrt_abstime hrt0   = hrt_absolute_time();
  clock_t     tick0  = clock_systime_ticks();

  up_mdelay((unsigned int)WAIT_MS);

  hrt_abstime hrt1   = hrt_absolute_time();
  clock_t     tick1  = clock_systime_ticks();

  int64_t hrt_elapsed_us   = (int64_t)(hrt1 - hrt0);
  int64_t tick_elapsed_us  = (int64_t)(tick1 - tick0) * CONFIG_USEC_PER_TICK;

  int64_t delta_us   = hrt_elapsed_us - tick_elapsed_us;
  double  delta_pct  = (double)delta_us / (double)tick_elapsed_us * 100.0;

  printf("\n  HRT elapsed   : %lld us  (%.3f ms)\n",
         (long long)hrt_elapsed_us, (double)hrt_elapsed_us / 1000.0);
  printf("  Tick elapsed  : %lld us  (%lld ticks × %d us)\n",
         (long long)tick_elapsed_us, (long long)(tick1 - tick0), CONFIG_USEC_PER_TICK);
  printf("  Difference    : %+lld us  (%+.3f%%)\n",
         (long long)delta_us, delta_pct);

  if (delta_pct > -0.5 && delta_pct < 0.5)
    printf("\n  Result: PASS — TCC0 and SysTick agree within 0.5%%\n");
  else if (delta_pct > -2.0 && delta_pct < 2.0)
    printf("\n  Result: MARGINAL — %+.3f%% skew (check BOARD_CPU_FREQUENCY)\n", delta_pct);
  else
    printf("\n  Result: FAIL — %+.3f%% skew — one clock is misconfigured\n", delta_pct);
}

/**
 * Dump hardware registers to diagnose actual CPU/SysTick/clock state.
 *
 * Reads:
 *   SYST_RVR         — SysTick reload (determines actual interrupt frequency)
 *   MCLK.CLKDIV[0]   — CPU Clock Divider (offset 0x0C)
 *   MCLK.CLKDIV[1]   — Second domain divider (offset 0x10; NOT the CPU divider)
 *   GCLK0/GCLK1 GENCTRL — source and divider for each generator
 *
 * Expected: SYST_RVR=2999999, CLKDIV[0]=1 (CPU=300 MHz), CLKDIV[1]=2
 */
static void cmd_regs(void)
{
  /* SysTick registers (CoreSight fixed-map at 0xE000E000) */
  uint32_t syst_csr = *(volatile uint32_t *)0xE000E010u;  /* SYST_CSR */
  uint32_t syst_rvr = *(volatile uint32_t *)0xE000E014u;  /* SYST_RVR */
  uint32_t syst_cvr = *(volatile uint32_t *)0xE000E018u;  /* SYST_CVR */

  /* MCLK (CA90 base 0x44052000):
   *   0x0C: CLKDIV0 — CPU clock divider. Reads 1 (reset default). WRITE = BusFault (PAC).
   *   0x10: CLKDIV[1] — accessible, set to 2 for CKRDY barrier.
   *   0x14: NOT accessible on CA90 — bus stall on read. DO NOT ACCESS. */
  uint32_t mclk_div0   = *(volatile uint32_t *)0x4405200Cu; /* CLKDIV0: CPU div, reads 1 */
  uint32_t mclk_rsvd10 = *(volatile uint32_t *)0x44052010u; /* CLKDIV[1]: set to 2 */

  /* GCLK GENCTRL (CA90 base 0x44050000; GENCTRL[n] at 0x0020 + n*4) */
  uint32_t gclk0 = *(volatile uint32_t *)0x44050020u;
  uint32_t gclk1 = *(volatile uint32_t *)0x44050024u;

  unsigned hz300 = (syst_rvr > 0u) ? (300000000u / (syst_rvr + 1u)) : 0u;
  unsigned cpu_mhz = (mclk_div0 > 0u) ? (300u / mclk_div0) : 300u;

  printf("--- SysTick ---\n");
  printf("  SYST_CSR [0xE000E010] = 0x%08x"
         "  CLKSRC=%d TICKINT=%d ENABLE=%d\n",
         (unsigned)syst_csr,
         (int)((syst_csr >> 2) & 1u),
         (int)((syst_csr >> 1) & 1u),
         (int)(syst_csr & 1u));
  printf("  SYST_RVR [0xE000E014] = %u (0x%x)\n",
         (unsigned)syst_rvr, (unsigned)syst_rvr);
  printf("    at 300 MHz CPU -> %u Hz (expected 100)\n", hz300);
  printf("  SYST_CVR [0xE000E018] = %u\n", (unsigned)syst_cvr);

  printf("--- MCLK (base 0x44052000) ---\n");
  printf("  [0x4405200C] CLKDIV0       = %u  (CPU divider, reset=1)"
         " -> GCLK0(300MHz) / %u = %u MHz CPU\n",
         (unsigned)mclk_div0,
         (unsigned)(mclk_div0 ? mclk_div0 : 1u),
         cpu_mhz);
  printf("  [0x44052010] CLKDIV[1]     = %u  (set to 2 for CKRDY barrier;"
         " 0x14 bus-stalls — DS map wrong)\n",
         (unsigned)mclk_rsvd10);

  printf("--- GCLK generators (base 0x44050000) ---\n");
  printf("  GCLK0 [0x44050020] = 0x%08x"
         "  SRC=%u GENEN=%d DIV=%u\n",
         (unsigned)gclk0,
         (unsigned)(gclk0 & 0x1Fu),
         (int)((gclk0 >> 8) & 1u),
         (unsigned)((gclk0 >> 16) & 0xFFFFu));
  printf("  GCLK1 [0x44050024] = 0x%08x"
         "  SRC=%u GENEN=%d DIV=%u\n",
         (unsigned)gclk1,
         (unsigned)(gclk1 & 0x1Fu),
         (int)((gclk1 >> 8) & 1u),
         (unsigned)((gclk1 >> 16) & 0xFFFFu));

  printf("--- Compiled constants ---\n");
  printf("  BOARD_CPU_FREQUENCY = %u MHz (used for SYST_RVR)\n",
         (unsigned)(BOARD_CPU_FREQUENCY / 1000000u));
  printf("  CONFIG_USEC_PER_TICK = %d us/tick -> CLK_TCK = %d Hz\n",
         CONFIG_USEC_PER_TICK,
         (int)(1000000 / CONFIG_USEC_PER_TICK));
  printf("  Expected SYST_RVR = %u\n",
         (unsigned)(BOARD_CPU_FREQUENCY / (1000000u / CONFIG_USEC_PER_TICK) - 1u));
  printf("--- Cross-check ---\n");
  if (syst_rvr == 2999999u && mclk_div0 == 1u)
    printf("  OK: SYST_RVR=300MHz, CLKDIV0(0x0C)=1 -> CPU=300 MHz\n");
  else
    printf("  UNEXPECTED: SYST_RVR=%u CLKDIV0=%u\n",
           (unsigned)syst_rvr, (unsigned)mclk_div0);
  printf("  CLKDIV[1](0x10)=%u (expect 2; 0x14=bus-stall: DS map wrong)\n",
         (unsigned)mclk_rsvd10);
}

/**
 * Test hrt_call_after (one-shot) and hrt_call_every (periodic) callouts.
 *
 * This directly tests the CC[0] compare match mechanism that PX4 WorkQueues
 * depend on.  If this test fails, no WorkQueue-based module will fire.
 */
static volatile uint32_t g_oneshot_count;
static volatile uint32_t g_periodic_count;
static volatile hrt_abstime g_periodic_last;
static volatile int64_t g_periodic_max_jitter;

static struct hrt_call g_test_oneshot;
static struct hrt_call g_test_periodic;

static void oneshot_callback(void *arg)
{
  (void)arg;
  g_oneshot_count++;
}

static void periodic_callback(void *arg)
{
  (void)arg;
  g_periodic_count++;

  hrt_abstime now = hrt_absolute_time();

  if (g_periodic_last != 0)
    {
      int64_t delta = (int64_t)(now - g_periodic_last);
      int64_t jitter = delta - 1000; /* expected 1000 µs period */

      if (jitter < 0) jitter = -jitter;

      if (jitter > g_periodic_max_jitter)
        {
          g_periodic_max_jitter = jitter;
        }
    }

  g_periodic_last = now;
}

static void cmd_callout(void)
{
  printf("=== HRT Callout Mechanism Test ===\n\n");

  /* Test 1: One-shot hrt_call_after with 500µs, 1ms, 10ms, 100ms delays */
  printf("Test 1: One-shot hrt_call_after\n");

  static const int oneshot_delays[] = {500, 1000, 5000, 10000, 100000};
  const int n_oneshots = 5;

  for (int i = 0; i < n_oneshots; i++)
    {
      g_oneshot_count = 0;
      hrt_call_init(&g_test_oneshot);

      hrt_abstime t0 = hrt_absolute_time();
      hrt_call_after(&g_test_oneshot, oneshot_delays[i],
                     oneshot_callback, NULL);

      /* Wait up to 2x the expected delay */
      int wait_ms = (oneshot_delays[i] / 1000) * 3 + 10;
      up_mdelay(wait_ms);

      hrt_abstime t1 = hrt_absolute_time();

      if (g_oneshot_count == 1)
        {
          printf("  delay=%6d us: PASS (fired in %lld us, waited %d ms)\n",
                 oneshot_delays[i],
                 (long long)(t1 - t0),
                 wait_ms);
        }
      else
        {
          printf("  delay=%6d us: FAIL (count=%u, expected 1)\n",
                 oneshot_delays[i], (unsigned)g_oneshot_count);
        }

      hrt_cancel(&g_test_oneshot);
    }

  /* Test 2: Periodic hrt_call_every at 1000µs (1ms) — use HRT elapsed as reference */
  printf("\nTest 2: Periodic hrt_call_every (1000 us period)\n");

  g_periodic_count = 0;
  g_periodic_last = 0;
  g_periodic_max_jitter = 0;
  hrt_call_init(&g_test_periodic);

  hrt_abstime t2_start = hrt_absolute_time();

  hrt_call_every(&g_test_periodic, 1000, 1000,
                 periodic_callback, NULL);

  up_mdelay(100);

  hrt_cancel(&g_test_periodic);

  hrt_abstime t2_end = hrt_absolute_time();
  uint32_t count = g_periodic_count;
  int64_t max_jit = g_periodic_max_jitter;
  int64_t elapsed_us = (int64_t)(t2_end - t2_start);
  uint32_t expected = (uint32_t)(elapsed_us / 1000);

  printf("  HRT elapsed: %lld us (up_mdelay(100) is slow on this board)\n",
         (long long)elapsed_us);
  printf("  Fired %u times (expected %u based on actual elapsed)\n",
         (unsigned)count, (unsigned)expected);
  printf("  Max jitter: %lld us\n", (long long)max_jit);

  if (count == 0)
    {
      printf("  Result: CRITICAL FAIL — callout never fired!\n");
    }
  else if (count == 1)
    {
      printf("  Result: FAIL — fired once then stopped (reschedule bug)\n");
    }
  else
    {
      int32_t error_pct = (int32_t)((int64_t)(count - expected) * 100 /
                                    (int64_t)expected);

      if (error_pct >= -10 && error_pct <= 10)
        printf("  Result: PASS (rate within 10%% of expected)\n");
      else
        printf("  Result: MARGINAL (rate error %d%%)\n", (int)error_pct);
    }

  /* Test 3: Short-period burst — 250µs period (tests rapid CC[0] reprogram) */
  printf("\nTest 3: Periodic hrt_call_every (250 us period)\n");

  g_periodic_count = 0;
  g_periodic_last = 0;
  g_periodic_max_jitter = 0;
  hrt_call_init(&g_test_periodic);

  hrt_abstime t3_start = hrt_absolute_time();

  hrt_call_every(&g_test_periodic, 250, 250,
                 periodic_callback, NULL);

  up_mdelay(50);

  hrt_cancel(&g_test_periodic);

  hrt_abstime t3_end = hrt_absolute_time();
  count = g_periodic_count;
  max_jit = g_periodic_max_jitter;
  elapsed_us = (int64_t)(t3_end - t3_start);
  expected = (uint32_t)(elapsed_us / 250);

  printf("  HRT elapsed: %lld us\n", (long long)elapsed_us);
  printf("  Fired %u times (expected %u based on actual elapsed)\n",
         (unsigned)count, (unsigned)expected);
  printf("  Max jitter: %lld us\n", (long long)max_jit);

  if (count <= 1)
    printf("  Result: CRITICAL FAIL\n");
  else
    {
      int32_t error_pct = (int32_t)((int64_t)(count - expected) * 100 /
                                    (int64_t)expected);

      if (error_pct >= -10 && error_pct <= 10)
        printf("  Result: PASS (rate within 10%%)\n");
      else
        printf("  Result: MARGINAL (rate error %d%%)\n", (int)error_pct);
    }

  printf("\n=== Summary ===\n");
  printf("If Test 2/3 show CRITICAL FAIL, the TCC0 CC[0] compare-match\n");
  printf("interrupt is not firing for short-period deadlines.\n");
  printf("Note: up_mdelay() is ~46%% slow on this board; test uses HRT\n");
  printf("elapsed time as the reference, not up_mdelay duration.\n");
}

/* =========================================================================
 * Entry point
 * =========================================================================
 */

__EXPORT int hrt_test_main(int argc, char *argv[])
{
  if (argc < 2)
    {
      printf("Usage: hrt_test <command> [args]\n");
      printf("Commands:\n");
      printf("  time       - Print current HRT timestamp\n");
      printf("  mono       - Monotonicity check (2000 samples)\n");
      printf("  delay N    - Accuracy vs up_mdelay(N ms)\n");
      printf("  stress     - Accuracy table across 1..2000 ms\n");
      printf("  cross      - Cross-check HRT vs SysTick over 2 s\n");
      printf("  regs       - Dump SysTick/MCLK/GCLK hardware registers\n");
      printf("  callout    - Test HRT callout mechanism (one-shot + periodic)\n");
      return 0;
    }

  if (strcmp(argv[1], "time") == 0)
    {
      cmd_time();
    }
  else if (strcmp(argv[1], "mono") == 0)
    {
      cmd_mono();
    }
  else if (strcmp(argv[1], "delay") == 0)
    {
      if (argc < 3)
        {
          printf("Usage: hrt_test delay <milliseconds>\n");
          return -1;
        }

      int ms = atoi(argv[2]);

      if (ms < 1 || ms > 30000)
        {
          printf("Error: delay must be 1..30000 ms\n");
          return -1;
        }

      cmd_delay(ms);
    }
  else if (strcmp(argv[1], "stress") == 0)
    {
      cmd_stress();
    }
  else if (strcmp(argv[1], "cross") == 0)
    {
      cmd_cross();
    }
  else if (strcmp(argv[1], "regs") == 0)
    {
      cmd_regs();
    }
  else if (strcmp(argv[1], "callout") == 0)
    {
      cmd_callout();
    }
  else
    {
      printf("Unknown command: %s\n", argv[1]);
      return -1;
    }

  return 0;
}
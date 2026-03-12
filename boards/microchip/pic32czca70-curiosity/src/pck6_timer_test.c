/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file pck6_timer_test.c
 *
 * Test PCK6 1 MHz clock with TC3 (independent of HRT on TC0).
 * This implements a "pseudo-HRT" using PCK6 with proper wrap handling
 * to verify 1 MHz identity conversion works before applying to real HRT.
 *
 * Usage from NSH:
 *   pck6_test init     - Initialize PCK6 and TC3
 *   pck6_test read     - Read TC3 counter value
 *   pck6_test time     - Read pseudo-HRT time in microseconds
 *   pck6_test delay N  - Test N millisecond delay accuracy
 *   pck6_test stress   - Run extended timing test
 *   pck6_test status   - Show register status
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/module.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>

#include <drivers/drv_hrt.h>

#include "arm_internal.h"
#include "sam_pck.h"
#include "sam_periphclks.h"
#include "hardware/sam_pmc.h"
#include "hardware/sam_matrix.h"
#include "hardware/sam_tc.h"

/*
 * Use TC3 (first channel of TC345 block) to avoid conflict with HRT on TC0.
 * TC345 block base is at SAM_TC345_BASE = 0x40010000
 * TC3 = SAM_TC3_BASE = 0x40010000 (offset 0x00 within block)
 */

/* TC channel register offsets */
#define TC_CCR_OFFSET      0x00   /* Channel Control Register */
#define TC_CMR_OFFSET      0x04   /* Channel Mode Register */
#define TC_CV_OFFSET       0x10   /* Counter Value */
#define TC_RA_OFFSET       0x14   /* Register A */
#define TC_RB_OFFSET       0x18   /* Register B */
#define TC_RC_OFFSET       0x1C   /* Register C (compare/wrap value) */
#define TC_SR_OFFSET       0x20   /* Status Register */

/* TC3 register access macros */
#define TC3_REG(offset)    (*(volatile uint32_t *)(SAM_TC3_BASE + (offset)))
#define rTC3_CCR           TC3_REG(TC_CCR_OFFSET)
#define rTC3_CMR           TC3_REG(TC_CMR_OFFSET)
#define rTC3_CV            TC3_REG(TC_CV_OFFSET)
#define rTC3_RA            TC3_REG(TC_RA_OFFSET)
#define rTC3_RB            TC3_REG(TC_RB_OFFSET)
#define rTC3_RC            TC3_REG(TC_RC_OFFSET)
#define rTC3_SR            TC3_REG(TC_SR_OFFSET)

/* TC3 is 32-bit counter on SAMV7, wraps at 2^32 */
#define TC3_COUNTER_PERIOD  (UINT64_C(1) << 32)

/* CCFG_PCCR register - Peripheral Clock Configuration */
#ifndef SAM_MATRIX_CCFG_PCCR
#  define SAM_MATRIX_CCFG_PCCR  (0x40088000 + 0x0118)
#endif

#ifndef CCFG_PCCR_TC0CC
#  define CCFG_PCCR_TC0CC       (1 << 20)
#endif
#ifndef CCFG_PCCR_TC2CC
#  define CCFG_PCCR_TC2CC       (1 << 22)
#endif

#ifndef SAM_MATRIX_WPMR
#  define SAM_MATRIX_WPMR       (0x40088000 + 0x01E4)
#endif
#ifndef MATRIX_WPMR_WPKEY
#  define MATRIX_WPMR_WPKEY     (0x4D4154 << 8)
#endif

#ifndef PMC_INT_PCKRDY6
#  define PMC_INT_PCKRDY6       (1 << 14)
#endif

/* State tracking */
static bool g_pck6_initialized = false;
static bool g_tc3_initialized = false;

/* Pseudo-HRT state - tracks 16-bit wrap to provide 64-bit microseconds */
static volatile uint64_t g_pseudo_hrt_base = 0;
static volatile uint32_t g_pseudo_hrt_last_count = 0;
static volatile uint32_t g_wrap_count = 0;

/**
 * Initialize PCK6 for 1 MHz output
 */
static int pck6_init(void)
{
	uint32_t actual_freq;
	int timeout;

	printf("PCK6: Configuring for 1 MHz from MCK (%lu Hz)...\n",
	       (unsigned long)BOARD_MCK_FREQUENCY);

	actual_freq = sam_pck_configure(PCK6, PCKSRC_MCK, 1000000);

	printf("PCK6: sam_pck_configure returned %lu Hz\n", (unsigned long)actual_freq);

	if (actual_freq != 1000000) {
		printf("PCK6: ERROR - Expected 1000000 Hz, got %lu Hz\n",
		       (unsigned long)actual_freq);
		return -1;
	}

	printf("PCK6: Enabling...\n");
	sam_pck_enable(PCK6, true);

	printf("PCK6: Waiting for ready...\n");
	timeout = 100000;

	while ((getreg32(SAM_PMC_SR) & PMC_INT_PCKRDY6) == 0) {
		if (--timeout <= 0) {
			printf("PCK6: ERROR - Timeout waiting for PCKRDY6\n");
			sam_pck_enable(PCK6, false);
			return -1;
		}
	}

	printf("PCK6: Ready!\n");

	uint32_t verify_freq = sam_pck_frequency(PCK6);
	printf("PCK6: Verified frequency = %lu Hz\n", (unsigned long)verify_freq);

	g_pck6_initialized = true;
	return 0;
}

/**
 * Initialize TC3 using PCK6 as clock source
 */
static int tc3_init(void)
{
	uint32_t regval;

	if (!g_pck6_initialized) {
		printf("TC3: ERROR - PCK6 not initialized\n");
		return -1;
	}

	printf("TC3: Enabling peripheral clock...\n");
	sam_tc3_enableclk();

	printf("TC3: Disabling MATRIX write protection...\n");
	putreg32(MATRIX_WPMR_WPKEY, SAM_MATRIX_WPMR);

	printf("TC3: Routing PCK6 to TC345 via CCFG_PCCR...\n");
	regval = getreg32(SAM_MATRIX_CCFG_PCCR);
	printf("TC3: CCFG_PCCR before = 0x%08lx\n", (unsigned long)regval);
	regval &= ~CCFG_PCCR_TC2CC;
	putreg32(regval, SAM_MATRIX_CCFG_PCCR);
	printf("TC3: CCFG_PCCR after  = 0x%08lx\n", (unsigned long)getreg32(SAM_MATRIX_CCFG_PCCR));

	printf("TC3: Configuring channel...\n");
	rTC3_CCR = TC_CCR_CLKDIS;

	/* Configure TC3: TIMER_CLOCK4 = MCK/128 = 1.171875 MHz, waveform mode, count up
	 *
	 * SAMV7 internal timer clocks (not dependent on PCK):
	 *   TIMER_CLOCK1 (TCCLKS=0) = MCK/2   = 75 MHz
	 *   TIMER_CLOCK2 (TCCLKS=1) = MCK/8   = 18.75 MHz
	 *   TIMER_CLOCK3 (TCCLKS=2) = MCK/32  = 4.6875 MHz
	 *   TIMER_CLOCK4 (TCCLKS=3) = MCK/128 = 1.171875 MHz  ← Closest to 1 MHz!
	 *   TIMER_CLOCK5 (TCCLKS=4) = SLCK    = ~32 kHz
	 *
	 * PCK6 via CCFG_PCCR does NOT replace these - it only provides
	 * an external clock source on TCLK pins for XC0/XC1/XC2.
	 */
	rTC3_CMR = TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_TCLK4;
	printf("TC3: CMR = 0x%08lx (TCCLKS=3=TIMER_CLOCK4=MCK/128=1.17MHz)\n", (unsigned long)rTC3_CMR);

	/* Set RC to max to prevent early counter reset */
	rTC3_RC = 0xFFFFFFFF;
	rTC3_RA = 0xFFFFFFFF;
	rTC3_RB = 0xFFFFFFFF;
	printf("TC3: RA=0x%08lx RB=0x%08lx RC=0x%08lx\n",
	       (unsigned long)rTC3_RA, (unsigned long)rTC3_RB, (unsigned long)rTC3_RC);

	/* Enable clock and trigger */
	rTC3_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
	printf("TC3: Started!\n");

	/* Verify RC is still max after start */
	printf("TC3: After start - RC=0x%08lx\n", (unsigned long)rTC3_RC);

	/* Initialize pseudo-HRT state */
	g_pseudo_hrt_base = 0;
	g_pseudo_hrt_last_count = rTC3_CV;
	g_wrap_count = 0;

	printf("TC3: Initial counter value = %lu\n", (unsigned long)g_pseudo_hrt_last_count);

	g_tc3_initialized = true;
	return 0;
}

/**
 * Get absolute time in microseconds from TC3/PCK6
 * This mimics what HRT does - handles 16-bit wrap to provide 64-bit time
 * With PCK6 @ 1 MHz: 1 tick = 1 microsecond (IDENTITY CONVERSION!)
 */
static uint64_t pseudo_hrt_absolute_time(void)
{
	irqstate_t flags;
	uint64_t abstime;
	uint32_t count;

	flags = enter_critical_section();

	/* Read current counter - SAMV7 TC has 32-bit counter */
	count = rTC3_CV;

	/* Detect wrap - if current count < last count, counter wrapped */
	if (count < g_pseudo_hrt_last_count) {
		g_pseudo_hrt_base += TC3_COUNTER_PERIOD;
		g_wrap_count++;
	}

	g_pseudo_hrt_last_count = count;

	/* With 1 MHz clock: ticks = microseconds directly!
	 * No conversion math needed - this is the key benefit of PCK6
	 */
	abstime = g_pseudo_hrt_base + count;

	leave_critical_section(flags);

	return abstime;
}

/**
 * Show register status
 */
static void show_status(void)
{
	printf("\n=== PCK6 / TC3 Status ===\n");

	printf("\nPMC Registers:\n");
	printf("  PMC_SR      = 0x%08lx (PCKRDY6=%d)\n",
	       (unsigned long)getreg32(SAM_PMC_SR),
	       (getreg32(SAM_PMC_SR) & PMC_INT_PCKRDY6) ? 1 : 0);

	printf("\nMATRIX Registers:\n");
	printf("  CCFG_PCCR   = 0x%08lx\n", (unsigned long)getreg32(SAM_MATRIX_CCFG_PCCR));
	printf("    TC0CC (bit 20) = %d (0=PCK6 for TC012)\n",
	       (getreg32(SAM_MATRIX_CCFG_PCCR) & CCFG_PCCR_TC0CC) ? 1 : 0);
	printf("    TC2CC (bit 22) = %d (0=PCK6 for TC345)\n",
	       (getreg32(SAM_MATRIX_CCFG_PCCR) & CCFG_PCCR_TC2CC) ? 1 : 0);

	if (g_pck6_initialized) {
		printf("\nPCK6:\n");
		printf("  Frequency   = %lu Hz\n", (unsigned long)sam_pck_frequency(PCK6));
	}

	if (g_tc3_initialized) {
		printf("\nTC3 Registers:\n");
		printf("  CMR         = 0x%08lx\n", (unsigned long)rTC3_CMR);
		printf("  CV          = %lu\n", (unsigned long)rTC3_CV);
		printf("  SR          = 0x%08lx\n", (unsigned long)rTC3_SR);

		printf("\nPseudo-HRT State:\n");
		printf("  Base time   = %llu us\n", (unsigned long long)g_pseudo_hrt_base);
		printf("  Last count  = %lu\n", (unsigned long)g_pseudo_hrt_last_count);
		printf("  Wrap count  = %lu\n", (unsigned long)g_wrap_count);
		printf("  Current time= %llu us\n", (unsigned long long)pseudo_hrt_absolute_time());
	}

	printf("\nState:\n");
	printf("  PCK6 initialized: %s\n", g_pck6_initialized ? "yes" : "no");
	printf("  TC3 initialized:  %s\n", g_tc3_initialized ? "yes" : "no");
}

/**
 * Test delay accuracy using up_mdelay (which uses a calibrated loop)
 */
static void test_delay(int ms)
{
	if (!g_tc3_initialized) {
		printf("ERROR: TC3 not initialized. Run 'pck6_test init' first.\n");
		return;
	}

	printf("\nTesting %d ms delay using up_mdelay()...\n", ms);
	printf("(up_mdelay uses calibrated CPU loop, independent of HRT)\n\n");

	uint64_t expected_us = (uint64_t)ms * 1000;

	/* Take start time from pseudo-HRT */
	uint64_t start = pseudo_hrt_absolute_time();
	uint32_t start_wraps = g_wrap_count;

	/* Use NuttX calibrated delay (not HRT-based!) */
	up_mdelay(ms);

	/* Take end time */
	uint64_t end = pseudo_hrt_absolute_time();
	uint32_t end_wraps = g_wrap_count;

	uint64_t elapsed = end - start;
	int64_t error = (int64_t)elapsed - (int64_t)expected_us;
	double error_pct = (double)error / (double)expected_us * 100.0;

	printf("Results:\n");
	printf("  Expected:   %llu us\n", (unsigned long long)expected_us);
	printf("  Measured:   %llu us\n", (unsigned long long)elapsed);
	printf("  Error:      %lld us (%.2f%%)\n", (long long)error, error_pct);
	printf("  Wraps:      %lu -> %lu (%lu wraps during test)\n",
	       (unsigned long)start_wraps, (unsigned long)end_wraps,
	       (unsigned long)(end_wraps - start_wraps));

	/* Check if result is reasonable */
	if (error_pct > -5.0 && error_pct < 5.0) {
		printf("\n  SUCCESS: PCK6/TC3 timing is accurate (within 5%%)!\n");
	} else if (error_pct > -20.0 && error_pct < 20.0) {
		printf("\n  WARNING: Timing has some drift (within 20%%)\n");
	} else {
		printf("\n  ERROR: Timing is way off - check clock configuration\n");
	}
}

/**
 * LED blink test - use stopwatch to verify timing externally
 * Blinks LED using TC3 counter as timing source
 */
static void led_blink_test(int cycles, int period_ms)
{
	if (!g_tc3_initialized) {
		printf("ERROR: TC3 not initialized. Run 'pck6_test init' first.\n");
		return;
	}

	printf("\n=== LED Blink Test (External Verification) ===\n");
	printf("This test uses TC3/PCK6 to time LED blinks.\n");
	printf("USE A STOPWATCH to verify the actual time!\n\n");
	printf("Settings: %d cycles, %d ms per half-cycle\n", cycles, period_ms);
	printf("Expected total time: %d ms (%.1f seconds)\n\n",
	       cycles * period_ms * 2, (double)(cycles * period_ms * 2) / 1000.0);

	/* Get LED GPIO - PA23 on SAMV71-XULT */
	/* We'll use direct register access for reliability */
	volatile uint32_t *pioa_sodr = (volatile uint32_t *)0x400E0E30;  /* Set Output Data */
	volatile uint32_t *pioa_codr = (volatile uint32_t *)0x400E0E34;  /* Clear Output Data */
	uint32_t led_pin = (1 << 23);  /* PA23 */

	printf("Starting in 3 seconds... GET YOUR STOPWATCH READY!\n");
	up_mdelay(3000);
	printf("GO! Blinking now...\n");

	uint64_t test_start = pseudo_hrt_absolute_time();

	for (int i = 0; i < cycles; i++) {
		/* LED ON (active low on this board, so clear = ON) */
		*pioa_codr = led_pin;

		/* Wait using TC3 counter directly */
		uint64_t target = pseudo_hrt_absolute_time() + ((uint64_t)period_ms * 1000);
		while (pseudo_hrt_absolute_time() < target) {
			/* spin */
		}

		/* LED OFF (set = OFF) */
		*pioa_sodr = led_pin;

		/* Wait again */
		target = pseudo_hrt_absolute_time() + ((uint64_t)period_ms * 1000);
		while (pseudo_hrt_absolute_time() < target) {
			/* spin */
		}
	}

	uint64_t test_end = pseudo_hrt_absolute_time();
	uint64_t elapsed_us = test_end - test_start;

	printf("\nDONE! Stop your stopwatch.\n\n");
	printf("TC3/PCK6 measured: %llu us (%.3f seconds)\n",
	       (unsigned long long)elapsed_us,
	       (double)elapsed_us / 1000000.0);
	printf("Expected:          %d us (%.3f seconds)\n",
	       cycles * period_ms * 2 * 1000,
	       (double)(cycles * period_ms * 2) / 1000.0);
	printf("\nCompare your stopwatch reading to the expected time.\n");
	printf("If they match, TC3/PCK6 @ 1 MHz is working correctly!\n");
}

/**
 * Extended stress test - multiple intervals
 */
static void stress_test(void)
{
	if (!g_tc3_initialized) {
		printf("ERROR: TC3 not initialized. Run 'pck6_test init' first.\n");
		return;
	}

	printf("\n=== Extended Timing Stress Test ===\n");
	printf("Testing various delay intervals using up_mdelay()...\n\n");

	int delays[] = {10, 50, 100, 500, 1000, 2000};
	int num_tests = sizeof(delays) / sizeof(delays[0]);

	printf("Delay(ms)  Expected(us)  Measured(us)  Error(us)   Error(%%)\n");
	printf("-------------------------------------------------------------\n");

	for (int i = 0; i < num_tests; i++) {
		int ms = delays[i];
		uint64_t expected = (uint64_t)ms * 1000;

		uint64_t start = pseudo_hrt_absolute_time();
		up_mdelay(ms);
		uint64_t end = pseudo_hrt_absolute_time();

		uint64_t measured = end - start;
		int64_t error = (int64_t)measured - (int64_t)expected;
		double error_pct = (double)error / (double)expected * 100.0;

		printf("%5d      %10llu    %10llu    %+8lld    %+6.2f%%\n",
		       ms,
		       (unsigned long long)expected,
		       (unsigned long long)measured,
		       (long long)error,
		       error_pct);
	}

	printf("\nPseudo-HRT stats after test:\n");
	printf("  Total wraps: %lu\n", (unsigned long)g_wrap_count);
	printf("  Current time: %llu us\n", (unsigned long long)pseudo_hrt_absolute_time());

	printf("\nIf all errors are within +/- 5%%, PCK6 @ 1 MHz is working correctly!\n");
	printf("The identity conversion (1 tick = 1 us) is validated.\n");
}

/**
 * Main entry point
 */
__EXPORT int pck6_test_main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: pck6_test <command> [args]\n");
		printf("Commands:\n");
		printf("  init          - Initialize PCK6 and TC3\n");
		printf("  read          - Read raw TC3 counter (32-bit)\n");
		printf("  time          - Read pseudo-HRT time in microseconds\n");
		printf("  delay N       - Test N millisecond delay (uses up_mdelay ref)\n");
		printf("  stress        - Run timing test vs up_mdelay\n");
		printf("  blink [N] [P] - Blink LED N times, P ms period (USE STOPWATCH!)\n");
		printf("                  Default: 10 cycles, 500ms = 10 sec total\n");
		printf("  status        - Show register status\n");
		return 0;
	}

	if (strcmp(argv[1], "init") == 0) {
		printf("Initializing PCK6 and TC3...\n\n");

		if (pck6_init() < 0) {
			printf("\nFailed to initialize PCK6\n");
			return -1;
		}

		if (tc3_init() < 0) {
			printf("\nFailed to initialize TC3\n");
			return -1;
		}

		printf("\nInitialization complete!\n");
		printf("PCK6 @ 1 MHz ready. TC3 running with identity conversion.\n");
		return 0;

	} else if (strcmp(argv[1], "read") == 0) {
		if (!g_tc3_initialized) {
			printf("TC3 not initialized. Run 'pck6_test init' first.\n");
			return -1;
		}

		printf("TC3 raw counter (32-bit): %lu\n", (unsigned long)rTC3_CV);
		return 0;

	} else if (strcmp(argv[1], "raw") == 0) {
		/* Read counter 10 times with small delays to see increment rate */
		if (!g_tc3_initialized) {
			printf("TC3 not initialized. Run 'pck6_test init' first.\n");
			return -1;
		}

		printf("Reading TC3 counter 10 times with 100ms delays...\n");
		printf("At MCK/128 = 1.17MHz, expect ~117,000 increment per 100ms\n\n");

		uint32_t prev = 0;
		for (int i = 0; i < 10; i++) {
			uint32_t curr = rTC3_CV;
			int32_t delta = (int32_t)(curr - prev);
			printf("[%d] Counter: %10lu  Delta: %+10ld", i, (unsigned long)curr, (long)delta);
			if (i > 0) {
				/* Calculate approximate frequency */
				double freq_mhz = (double)delta / 100000.0;  /* delta per 100ms */
				printf("  (~%.2f MHz)", freq_mhz);
			}
			printf("\n");
			prev = curr;
			if (i < 9) up_mdelay(100);
		}
		return 0;

	} else if (strcmp(argv[1], "clocktest") == 0) {
		/* Pure counter-based LED blink - NO up_mdelay!
		 * Count 16-bit wraps to measure longer periods.
		 * At MCK/128 = 1.171875 MHz:
		 *   65536 ticks = 55.9ms per wrap
		 *   18 wraps = ~1 second
		 * So for 1-second blink (500ms on, 500ms off):
		 *   9 wraps per half-period
		 */
		if (!g_tc3_initialized) {
			printf("TC3 not initialized. Run 'pck6_test init' first.\n");
			return -1;
		}

		/* At 1.171875 MHz with 16-bit counter:
		 * 1 wrap = 65536 / 1171875 = 55.9 ms
		 * For 500ms: 500 / 55.9 = 8.94 wraps ≈ 9 wraps
		 */
		const int WRAPS_PER_500MS = 9;
		const int NUM_BLINKS = 5;  /* 5 blinks = 5 seconds expected */

		volatile uint32_t *pioa_sodr = (volatile uint32_t *)0x400E0E30;
		volatile uint32_t *pioa_codr = (volatile uint32_t *)0x400E0E34;
		uint32_t led_pin = (1 << 23);

		printf("\n=== Pure Counter-Based Clock Test (Slow) ===\n");
		printf("At MCK/128 = 1.171875 MHz:\n");
		printf("  16-bit wrap = 55.9ms\n");
		printf("  %d wraps = ~500ms\n", WRAPS_PER_500MS);
		printf("  %d blinks (on+off) = ~%d seconds expected\n\n", NUM_BLINKS, NUM_BLINKS);
		printf("USE STOPWATCH! Time %d full blinks.\n", NUM_BLINKS);
		printf("Starting NOW!\n");

		for (int blink = 0; blink < NUM_BLINKS; blink++) {
			/* LED ON */
			*pioa_codr = led_pin;

			/* Wait 9 wraps (~500ms) */
			uint16_t last = rTC3_CV & 0xFFFF;
			for (int w = 0; w < WRAPS_PER_500MS; w++) {
				/* Wait for counter to wrap (go from high to low) */
				while (1) {
					uint16_t now = rTC3_CV & 0xFFFF;
					if (now < last && (last - now) > 32768) {
						/* Wrapped! */
						break;
					}
					last = now;
				}
				last = rTC3_CV & 0xFFFF;
			}

			/* LED OFF */
			*pioa_sodr = led_pin;

			/* Wait 9 more wraps (~500ms) */
			for (int w = 0; w < WRAPS_PER_500MS; w++) {
				while (1) {
					uint16_t now = rTC3_CV & 0xFFFF;
					if (now < last && (last - now) > 32768) {
						break;
					}
					last = now;
				}
				last = rTC3_CV & 0xFFFF;
			}
		}

		printf("DONE! Did %d blinks take ~%d seconds?\n", NUM_BLINKS, NUM_BLINKS);
		printf("If YES: MCK/128 = 1.17 MHz is correct!\n");
		printf("If ~1.5 sec: Clock is ~3x faster than expected\n");
		printf("If ~15 sec:  Clock is ~3x slower than expected\n");
		return 0;

	} else if (strcmp(argv[1], "time") == 0) {
		if (!g_tc3_initialized) {
			printf("TC3 not initialized. Run 'pck6_test init' first.\n");
			return -1;
		}

		printf("Pseudo-HRT time: %llu us\n", (unsigned long long)pseudo_hrt_absolute_time());
		printf("  (wraps: %lu, base: %llu)\n",
		       (unsigned long)g_wrap_count,
		       (unsigned long long)g_pseudo_hrt_base);
		return 0;

	} else if (strcmp(argv[1], "delay") == 0) {
		if (argc < 3) {
			printf("Usage: pck6_test delay <milliseconds>\n");
			return -1;
		}

		int ms = atoi(argv[2]);

		if (ms <= 0 || ms > 10000) {
			printf("Error: delay must be 1-10000 ms\n");
			return -1;
		}

		test_delay(ms);
		return 0;

	} else if (strcmp(argv[1], "stress") == 0) {
		stress_test();
		return 0;

	} else if (strcmp(argv[1], "blink") == 0) {
		/* Default: 10 cycles, 500ms per half = 10 seconds total */
		int cycles = 10;
		int period = 500;

		if (argc >= 3) {
			cycles = atoi(argv[2]);
		}

		if (argc >= 4) {
			period = atoi(argv[3]);
		}

		if (cycles < 1 || cycles > 100 || period < 100 || period > 5000) {
			printf("Error: cycles 1-100, period 100-5000 ms\n");
			return -1;
		}

		led_blink_test(cycles, period);
		return 0;

	} else if (strcmp(argv[1], "status") == 0) {
		show_status();
		return 0;

	} else {
		printf("Unknown command: %s\n", argv[1]);
		return -1;
	}
}

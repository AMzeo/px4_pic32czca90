/****************************************************************************
 * SPI8 (SERCOM8) hardware verification test.
 * Tests: register state, GCLK routing, optional loopback, ICM45686 WHO_AM_I.
 *
 * Usage:  spi_test
 * Loopback: short PD27 (MISO) → PD24 (MOSI) on J401 before running.
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "arm_internal.h"

#ifdef CONFIG_PIC32CZCA90_SERCOM8_ISSPI

#define SERCOM8_BASE   0x45002000u  /* SAM_SERCOM8_BASE */
#define GCLK_BASE      0x44050000u
#define MCLK_CLKMSK0   0x4405203Cu  /* SAM_MCLK_BASE(0x44052000) + CLKMSK[0](0x3C) */
#define MCLK_AHB_PORT  (1u << 22)   /* PORT_MCLK_ID_AHB=22 → CLKMSK[0] bit 22 */

/* SERCOM SPI register offsets */
#define SPI_CTRLA      0x00
#define SPI_CTRLB      0x04
#define SPI_BAUD       0x0C
#define SPI_INTFLAG    0x18
#define SPI_STATUS     0x1A
#define SPI_SYNCBUSY   0x1C
#define SPI_DATA       0x28

/* INTFLAG bits */
#define SPI_DRE  (1u << 0)
#define SPI_TXC  (1u << 1)
#define SPI_RXC  (1u << 2)

/* PORT D — CA90 has TWO bus interfaces for PORT (DFP: PORT_AHB_BASE=0x40000000,
 * PORT_BASE=0x44840000). AHB writes to PORTD (group 3) are unreliable: OUTSET
 * silently fails (pin unchanged) and OUTCLR crashes with PRECISERR. Use APB for
 * OUTSET/OUTCLR — sam_port.c does the same and drives 307k+ ICM45686 transfers
 * without fault. AHB reads (IN, DIR) work correctly. */
#define PORTD_AHB_BASE 0x40000180u   /* PORT_AHB_BASE (0x40000000) + group3*0x80 */
#define PORTD_APB_BASE 0x44840180u   /* PORT_BASE     (0x44840000) + group3*0x80 */

/* Reads — use APB for PORTD group 3 (AHB IN reads also unreliable, confirmed) */
#define PORTD_DIR      (PORTD_APB_BASE + 0x00)
#define PORTD_OUT      (PORTD_APB_BASE + 0x10)
#define PORTD_IN       (PORTD_APB_BASE + 0x20)
/* Writes — APB (AHB writes to PORTD crash or are silently ignored on CA90) */
#define PORTD_OUTCLR   (PORTD_APB_BASE + 0x14)
#define PORTD_OUTSET   (PORTD_APB_BASE + 0x18)

/* Config registers — APB base (reads and writes both work here) */
#define PORTD_PINCFG   (PORTD_APB_BASE + 0x40)
#define PORTD_PMUX     (PORTD_APB_BASE + 0x30)

/* ICM-45686 */
#define ICM45686_WHOAMI_REG   0x72
#define ICM45686_WHOAMI_VAL   0xE9
#define SPI_READ_BIT          0x80

/* BMM150 (GeoMagnetic Click) — same J401 wiring, Mode 3 compatible */
#define BMM150_CHIPID_REG     0x40
#define BMM150_CHIPID_VAL     0x32

/* CS = PD26 */
#define CS_PIN  26u
#define CS_MASK (1u << CS_PIN)

/* ── helpers ────────────────────────────────────────────────────────────── */

static void spi_wait_dre(void)
{
	int t = 200000;
	while (!(getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_DRE) && --t > 0);
}

static void spi_wait_rxc(void)
{
	int t = 200000;
	while (!(getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_RXC) && --t > 0);
}

static uint8_t spi_xfer(uint8_t tx)
{
	spi_wait_dre();
	putreg32(tx, SERCOM8_BASE + SPI_DATA);
	spi_wait_rxc();
	return (uint8_t)(getreg32(SERCOM8_BASE + SPI_DATA) & 0xFFu);
}



/* ── main ───────────────────────────────────────────────────────────────── */

int spi_test_main(int argc, char *argv[])
{
	/* Disable the write buffer: ACTLR.DISDEFWBUF (bit 1).
	 * Default: buffered Device writes → IMPRECISERR → PC in fault frame is WRONG.
	 * With DISDEFWBUF=1: every write is synchronous → fault PC = exact bad instruction.
	 * Performance hit is fine for a diagnostic test; restored on return. */
	uint32_t actlr_save = getreg32(0xE000E008u);
	putreg32(actlr_save | 2u, 0xE000E008u);

	/* --wake: T0 sequence to switch ICM-45686 from I2C to SPI mode */
	if (argc >= 2 && strcmp(argv[1], "--wake") == 0) {
		if (getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_RXC) {
			(void)getreg32(SERCOM8_BASE + SPI_DATA);
		}
		putreg32(CS_MASK, PORTD_OUTCLR);
		(void)spi_xfer(0xAA);
		(void)spi_xfer(0x55);
		putreg32(CS_MASK, PORTD_OUTSET);
		usleep(10000);
		putreg32(CS_MASK, PORTD_OUTCLR);
		(void)spi_xfer(ICM45686_WHOAMI_REG | SPI_READ_BIT);
		uint8_t wa = spi_xfer(0x00);
		putreg32(CS_MASK, PORTD_OUTSET);
		printf("[icm_wake] WHO_AM_I=0x%02X %s\n", wa,
		       wa == ICM45686_WHOAMI_VAL ? "OK" : "FAIL");
		putreg32(actlr_save, 0xE000E008u);
		return wa == ICM45686_WHOAMI_VAL ? 0 : -1;
	}

	/* Dump MCLK CLKMSK0 so we can see which peripheral clocks are enabled.
	 * PORT AHB = bit 22, PORT APB = bit 23 (DFP: PORT_MCLK_ID_AHB=22/APB=23). */
	printf("MCLK CLKMSK0 = 0x%08lx (bit22=PORT_AHB=%lu bit23=PORT_APB=%lu)\n",
		(unsigned long)getreg32(MCLK_CLKMSK0),
		(getreg32(MCLK_CLKMSK0) >> 22) & 1,
		(getreg32(MCLK_CLKMSK0) >> 23) & 1);

	/* If a previous run left SERCOM8 with wrong DOPO/DIPO (swap test without restore),
	 * fix it now. Requires sensors stopped first (DMA idle). */
	{
		uint32_t ci = getreg32(SERCOM8_BASE + SPI_CTRLA);
		if (((ci >> 16) & 3) != 0 || ((ci >> 20) & 3) != 3) {
			printf("WARNING: SERCOM8 DOPO=%lu DIPO=%lu — restoring to DOPO=0 DIPO=3\n",
				(ci >> 16) & 3, (ci >> 20) & 3);
			uint32_t ci_fix = (ci & ~((0x3u << 16) | (0x3u << 20))) | (3u << 20);
			putreg32(ci & ~(1u << 1), SERCOM8_BASE + SPI_CTRLA);  /* ENABLE=0 */
			{ int t = 200000; while ((getreg32(SERCOM8_BASE + SPI_SYNCBUSY) & (1u << 1)) && --t > 0); }
			putreg32(ci_fix, SERCOM8_BASE + SPI_CTRLA);
			{ int t = 200000; while ((getreg32(SERCOM8_BASE + SPI_SYNCBUSY) & (1u << 1)) && --t > 0); }
		}
	}

	/* ICM-45686 interface selection: chip commits to SPI mode on first CS LOW.
	 * If CS was HIGH at power-on the chip may be in I2C mode (SDO=AD0=0V).
	 * Pulse CS LOW for 2 ms → chip selects SPI → CS HIGH → proceed. */
	putreg8(getreg8(PORTD_PINCFG + CS_PIN) | 2u, PORTD_PINCFG + CS_PIN);  /* INEN=1 */
	putreg32(CS_MASK, PORTD_OUTCLR);   /* CS LOW  — SPI mode selection */
	usleep(2000);
	putreg32(CS_MASK, PORTD_OUTSET);   /* CS HIGH — end of selection pulse */
	usleep(2000);  /* chip settling */

	{
		uint32_t in0  = getreg32(PORTD_IN);
		uint32_t out0 = getreg32(PORTD_OUT);
		printf("CS after OUTSET: PD26 OUT=%lu IN=%lu  %s\n",
			(out0 >> 26) & 1, (in0 >> 26) & 1,
			((in0 >> 26) & 1) ? "HIGH (ok)"
			: ((out0 >> 26) & 1) ? "STUCK LOW — sensor/breakout pulling CS down"
			: "OUTSET FAILED — write did not take effect");
	}

	printf("\n=== SPI8 (SERCOM8) Hardware Verification ===\n\n");

	/* ── 1. SERCOM8 registers ─────────────────────────────────────────── */
	uint32_t ctrla    = getreg32(SERCOM8_BASE + SPI_CTRLA);
	uint32_t ctrlb    = getreg32(SERCOM8_BASE + SPI_CTRLB);
	uint8_t  baud     = getreg8(SERCOM8_BASE + SPI_BAUD);
	uint8_t  intflag  = getreg8(SERCOM8_BASE + SPI_INTFLAG);
	uint32_t syncbusy = getreg32(SERCOM8_BASE + SPI_SYNCBUSY);

	printf("CTRLA:    0x%08lx\n", (unsigned long)ctrla);
	printf("  ENABLE=%lu  MODE=%lu  DOPO=%lu  DIPO=%lu  CPOL=%lu  CPHA=%lu\n",
		(ctrla >> 1) & 1, (ctrla >> 2) & 7,
		(ctrla >> 16) & 3, (ctrla >> 20) & 3,
		(ctrla >> 29) & 1, (ctrla >> 28) & 1);
	printf("CTRLB:    0x%08lx  RXEN=%lu  CHSIZE=%lu\n",
		(unsigned long)ctrlb, (ctrlb >> 17) & 1, ctrlb & 7);
	printf("BAUD:     %u -> %lu Hz clock\n", baud,
		(unsigned long)(100000000u / (2u * ((uint32_t)baud + 1u))));
	printf("INTFLAG:  0x%02x  (DRE=%u TXC=%u RXC=%u)\n",
		intflag,
		(intflag >> 0) & 1, (intflag >> 1) & 1, (intflag >> 2) & 1);
	printf("SYNCBUSY: 0x%08lx\n\n", (unsigned long)syncbusy);

	int ok = 1;

	if (!(ctrla & (1u << 1))) {
		printf("ERROR: CTRLA.ENABLE = 0 — SERCOM8 not enabled!\n");
		ok = 0;
	}
	if (((ctrla >> 2) & 7) != 3) {
		printf("ERROR: CTRLA.MODE = %lu — expected 3 (SPI master)!\n",
			(ctrla >> 2) & 7);
		ok = 0;
	}
	if (!(ctrlb & (1u << 17))) {
		printf("ERROR: CTRLB.RXEN = 0 — receiver disabled!\n");
		ok = 0;
	}
	if (((ctrla >> 16) & 3) != 0) {
		printf("ERROR: DOPO = %lu — expected 0 (MOSI=PAD0, SCK=PAD1)!\n",
			(ctrla >> 16) & 3);
		ok = 0;
	}
	if (((ctrla >> 20) & 3) != 3) {
		printf("ERROR: DIPO = %lu — expected 3 (MISO=PAD3)!\n",
			(ctrla >> 20) & 3);
		ok = 0;
	}

	/* ── 2. GCLK routing ─────────────────────────────────────────────── */
	uint32_t pchctrl29 = getreg32(GCLK_BASE + 0x80 + 29 * 4);
	uint32_t genctrl2  = getreg32(GCLK_BASE + 0x20 + 2  * 4);

	printf("GCLK PCHCTRL[29] (SERCOM8 core): 0x%08lx\n", (unsigned long)pchctrl29);
	printf("  CHEN=%lu  GEN=%lu  (expect CHEN=1 GEN=2 -> 0x00000042)\n",
		(pchctrl29 >> 6) & 1, pchctrl29 & 0xF);
	printf("GCLK GENCTRL[2]:                 0x%08lx\n", (unsigned long)genctrl2);
	printf("  GENEN=%lu  SRC=%lu  DIV=%lu  (expect GENEN=1 SRC=6 DIV=3)\n\n",
		(genctrl2 >> 8) & 1, genctrl2 & 0x1F, (genctrl2 >> 16) & 0xFFFF);

	if (!((pchctrl29 >> 6) & 1)) {
		printf("ERROR: SERCOM8 GCLK channel NOT enabled — SPI clock dead!\n");
		ok = 0;
	}
	if ((pchctrl29 & 0xF) != 2) {
		printf("ERROR: SERCOM8 GCLK gen = %lu, expected 2!\n",
			pchctrl29 & 0xF);
		ok = 0;
	}

	/* ── 3. PORT D pin mux ───────────────────────────────────────────── */
	printf("PORT D pin configuration:\n");
	for (int i = 24; i <= 27; i++) {
		uint8_t pincfg = getreg8(PORTD_PINCFG + i);
		uint8_t pmuxbyte = getreg8(PORTD_PMUX + i / 2);
		uint8_t func = (i & 1) ? (pmuxbyte >> 4) : (pmuxbyte & 0xF);
		const char *expect = (i == 26) ? "GPIO CS (PMUXEN=0)" : "PMUXEN+func D=3";
		printf("  PD%d: PINCFG=0x%02x  func=%u  %s\n",
			i, pincfg, func, expect);
	}

	uint32_t dir = getreg32(PORTD_DIR);
	printf("  PORTD DIR: 0x%08lx\n\n", (unsigned long)dir);

	/* ── 4. Clock live test ──────────────────────────────────────────── */
	printf("Clock live test: send 0x00 and count RXC wait loops...\n");
	spi_wait_dre();
	putreg32(0x00, SERCOM8_BASE + SPI_DATA);
	volatile uint32_t count = 0;
	int t = 1000000;
	while (!(getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_RXC) && --t > 0) {
		count++;
	}
	if (t == 0) {
		printf("ERROR: RXC never set — clock may not be running!\n");
		ok = 0;
	} else {
		(void)getreg32(SERCOM8_BASE + SPI_DATA);
		printf("RXC wait loops: %lu — %s\n\n",
			(unsigned long)count,
			count > 0 ? "PASS (clock is running)" : "SUSPICIOUS (instant)");
	}

	if (!ok) {
		printf("FATAL: register checks failed\n");
		return -1;
	}

	/* ── 5. Loopback test (optional — short PD27->PD24) ─────────────── */
	printf("=== Loopback test (short MISO PD27 to MOSI PD24 on J401) ===\n");
	uint8_t lb_ok = 0;
	uint8_t patterns[] = {0xAA, 0x55, 0xF0, 0x0F};
	for (int i = 0; i < 4; i++) {
		uint8_t rx = spi_xfer(patterns[i]);
		printf("  Sent 0x%02X -> got 0x%02X  %s\n",
			patterns[i], rx,
			rx == patterns[i] ? "MATCH" : "(no loopback)");
		if (rx == patterns[i]) {
			lb_ok = 1;
		}
	}
	if (!lb_ok) {
		printf("  (No loopback — connect PD27->PD24 to confirm MOSI output)\n");
	}

	/* ── 6. MISO state diagnostic — SKIPPED (isolated crash source) ─── */
#if 0
	printf("\n=== MISO (PD27) state diagnostic ===\n");
	uint32_t portd_in = getreg32(PORTD_IN);
	printf("  PORTD IN = 0x%08lx  PD27=%lu  PD26=%lu\n",
		(unsigned long)portd_in,
		(portd_in >> 27) & 1,
		(portd_in >> 26) & 1);
	if (!((portd_in >> 26) & 1)) {
		printf("  WARNING: PD26 (CS) is LOW — background task may still hold CS!\n");
		printf("  Run 'sensors stop; icm45686 stop' before spi_test for clean state.\n");
	}
	putreg32(CS_MASK, PORTD_OUTCLR);
	for (volatile int d = 0; d < 5000; d++);
	uint32_t portd_in = getreg32(PORTD_IN);
	printf("  After CS assert (no clocking): PD27 MISO=%lu  PD26=%lu\n",
		(portd_in >> 27) & 1, (portd_in >> 26) & 1);
	putreg32(CS_MASK, PORTD_OUTSET);
	for (volatile int d = 0; d < 5000; d++);
#endif

	/* ── 7. ICM-45686 WHO_AM_I ───────────────────────────────────────── */
	printf("\n=== ICM-45686 WHO_AM_I (DOPO=0/DIPO=3, current wiring) ===\n");

	/* Force CS high right before the test (background task may have grabbed it) */
	putreg32(CS_MASK, PORTD_OUTSET);

	/* Keep SERCOM8 at its current BAUD (8.3 MHz) — spi_sercom_disable() stalls
	 * the bus while sam_spi.c DMA descriptor is still configured, causing WDT reset.
	 * 8.3 MHz works fine for both ICM45686 and BMM150 on short flying wires. */
	if (getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_RXC) {
		(void)getreg32(SERCOM8_BASE + SPI_DATA);  /* flush stale RX */
	}

	/* ICM-45686 SPI output needs 2 dummy transactions to wake after mode-select pulse.
	 * Without these, the first real read returns 0x00. */
	for (int w = 0; w < 2; w++) {
		putreg32(CS_MASK, PORTD_OUTCLR);
		for (volatile int d = 0; d < 500; d++);
		(void)spi_xfer(0x00);
		(void)spi_xfer(0x00);
		putreg32(CS_MASK, PORTD_OUTSET);
		for (volatile int d = 0; d < 1000; d++);
	}

	/* Assert CS, transfer, deassert */
	putreg32(CS_MASK, PORTD_OUTCLR);
	for (volatile int d = 0; d < 500; d++);
	uint8_t dummy  = spi_xfer(ICM45686_WHOAMI_REG | SPI_READ_BIT);
	uint8_t whoami = spi_xfer(0xFF);
	putreg32(CS_MASK, PORTD_OUTSET);

	printf("  Sent 0x%02X (WHO_AM_I|READ), dummy=0x%02X\n",
		ICM45686_WHOAMI_REG | SPI_READ_BIT, dummy);
	printf("  WHO_AM_I = 0x%02X  (expected 0x%02X)\n", whoami, ICM45686_WHOAMI_VAL);

	if (whoami == ICM45686_WHOAMI_VAL) {
		printf("*** ICM-45686 DETECTED — SPI working! ***\n");
		goto restore_baud;
	}

	if (whoami == 0xFF) {
		printf("  0xFF: MISO floating high\n");
		printf("  Check: MISO wire to PD27? Sensor powered?\n");
		goto restore_baud;
	}

	if (whoami == 0x00 || whoami == 0xFF) {
		/* Scan common WHO_AM_I addresses to identify actual chip */
		printf("  Scanning WHO_AM_I addresses:\n");
		static const uint8_t scan_regs[] = {0x0F, 0x40, 0x72, 0x75, 0x77};
		for (int i = 0; i < (int)(sizeof(scan_regs)); i++) {
			putreg32(CS_MASK, PORTD_OUTCLR);
			for (volatile int d = 0; d < 500; d++);
			(void)spi_xfer(scan_regs[i] | SPI_READ_BIT);
			uint8_t rv = spi_xfer(0xFF);
			putreg32(CS_MASK, PORTD_OUTSET);
			printf("  Reg[0x%02X] = 0x%02X%s\n", scan_regs[i], rv,
				rv == 0x47 ? "  <- ICM-42688-P" :
				rv == 0xE9 ? "  <- ICM-45686" :
				rv == 0x33 ? "  <- ICM-42605" :
				rv == 0x98 ? "  <- ICM-20689" :
				rv == 0x6B ? "  <- MPU-6050/6500" :
				(rv != 0x00 && rv != 0xFF) ? "  <- non-zero (check datasheet)" : "");
		}
	} else {
		printf("  0x%02X: partial/corrupt response\n", whoami);
	}

	/* ── 8. Swap DOPO/DIPO — DISABLED (corrupts SERCOM8 CTRLA without restore) ── */
#if 0
	/* Re-enable if needed for wire-swap diagnosis. Requires adding CTRLA restore. */
	/* ... swap test code ... */
#endif

restore_baud:
	/* Deassert CS — SERCOM8 config untouched (no CTRLA/BAUD writes to avoid bus stall) */
	putreg32(CS_MASK, PORTD_OUTSET);

	/* ── 9. BMM150 (GeoMagnetic Click) CHIP_ID ─────────────────────────── */
	/* BMM150 supports SPI Mode 0 AND Mode 3 — our CTRLA (Mode 3) works as-is.
	 * IMPORTANT: BMM150 powers up in SUSPEND mode. In SPI suspend mode, the
	 * data bus is high-Z → CHIP_ID reads as 0xFF. Must write POWER_CONTROL=0x01
	 * first to leave suspend, wait 3 ms, THEN read CHIP_ID. */
	printf("\n=== BMM150 (GeoMagnetic Click) CHIP_ID ===\n");
	printf("  Connect GeoMagnetic Click to J401 (same MOSI/MISO/SCK/CS wiring).\n");
	printf("  If ICM45686 is connected instead, expect 0xFF here.\n");

	/* Same SERCOM8 config as section 7 — no BAUD change, flush stale RX only */
	if (getreg8(SERCOM8_BASE + SPI_INTFLAG) & SPI_RXC) {
		(void)getreg32(SERCOM8_BASE + SPI_DATA);
	}

	putreg32(CS_MASK, PORTD_OUTSET);
	for (volatile int d = 0; d < 500; d++);

	/* Step 1: write POWER_CONTROL=0x01 to leave suspend mode.
	 * Register 0x4B, bit 0 = PowerControl. Write: bit7=0 (already 0). */
	printf("  Powering up (POWER_CONTROL=0x01)...\n");
	putreg32(CS_MASK, PORTD_OUTCLR);
	for (volatile int d = 0; d < 500; d++);
	(void)spi_xfer(0x4B);    /* POWER_CONTROL reg, write (bit7=0) */
	(void)spi_xfer(0x01);    /* PowerControl=1: leave suspend */
	putreg32(CS_MASK, PORTD_OUTSET);

	/* Step 2: wait 3 ms for startup */
	usleep(3000);

	/* Step 3: read CHIP_ID — must return 0x32 now */
	putreg32(CS_MASK, PORTD_OUTCLR);
	for (volatile int d = 0; d < 500; d++);
	uint8_t bmm_d     = spi_xfer(BMM150_CHIPID_REG | SPI_READ_BIT); /* send 0xC0 */
	uint8_t bmm_chipid = spi_xfer(0x00);
	putreg32(CS_MASK, PORTD_OUTSET);

	printf("  Sent 0x%02X (READ|CHIP_ID), dummy=0x%02X\n",
		BMM150_CHIPID_REG | SPI_READ_BIT, bmm_d);
	printf("  CHIP_ID = 0x%02X  (expected 0x%02X)\n", bmm_chipid, BMM150_CHIPID_VAL);

	if (bmm_chipid == BMM150_CHIPID_VAL) {
		printf("*** BMM150 DETECTED — SPI driver solid! ***\n");
	} else if (bmm_chipid == 0xFF) {
		printf("  0xFF: MISO floating — sensor not connected, not powered, or wiring issue\n");
		printf("  (Device was powered up before this read; 0xFF = bus idle, not suspend)\n");
	} else {
		printf("  0x%02X: unexpected (wrong sensor, wrong SPI mode, or wiring issue)\n", bmm_chipid);
	}

	/* Power BMM150 back to suspend */
	putreg32(CS_MASK, PORTD_OUTCLR);
	for (volatile int d = 0; d < 500; d++);
	(void)spi_xfer(0x4B);  /* POWER_CONTROL, write */
	(void)spi_xfer(0x00);  /* PowerControl=0: suspend */
	putreg32(CS_MASK, PORTD_OUTSET);

	/* Restore write-buffer mode (SERCOM8 config unchanged — never reconfigured) */
	putreg32(actlr_save, 0xE000E008u);

	return 0;
}

#else

int spi_test_main(int argc, char *argv[])
{
	printf("CONFIG_PIC32CZCA90_SERCOM8_ISSPI not set\n");
	return -1;
}

#endif /* CONFIG_PIC32CZCA90_SERCOM8_ISSPI */

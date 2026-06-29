/****************************************************************************
 * SPI bus debug test — sends bytes and prints what comes back.
 * Usage: spi_test
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <nuttx/spi/spi.h>

#ifdef CONFIG_PIC32CZCA90_SERCOM3_ISSPI
#include "sam_spi.h"
#include "arm_internal.h"
#include "hardware/pic32czca90_memorymap.h"
#include "hardware/sam_sercom_spi.h"

int spi_test_main(int argc, char *argv[])
{
	uintptr_t base = SAM_SERCOM3_BASE;

	printf("=== SPI3 Register Dump ===\n");
	printf("CTRLA:    0x%08lx\n", (unsigned long)getreg32(base + 0x00));
	printf("CTRLB:    0x%08lx\n", (unsigned long)getreg32(base + 0x04));
	printf("BAUD:     0x%02x\n", getreg8(base + 0x0C));
	printf("INTFLAG:  0x%02x\n", getreg8(base + 0x18));
	printf("STATUS:   0x%04x\n", getreg16(base + 0x1A));
	printf("SYNCBUSY: 0x%08lx\n", (unsigned long)getreg32(base + 0x1C));

	/* Dump PORT C pin config for PC12(MOSI), PC13(SCK), PC15(MISO)
	 * PORT base = 0x44840000, group size = 0x80 */
	uintptr_t portc = 0x44840000u + (2u * 0x80u); /* PORTC = 0x44840100 */
	printf("\n=== PORTC Pin Config ===\n");
	printf("PC12 PINCFG: 0x%02x (expect PMUXEN+INEN=0x01 for MOSI output)\n",
		getreg8(portc + 0x40 + 12));
	printf("PC13 PINCFG: 0x%02x (expect PMUXEN=0x01 for SCK)\n",
		getreg8(portc + 0x40 + 13));
	printf("PC14 PINCFG: 0x%02x (expect 0x00 for GPIO CS)\n",
		getreg8(portc + 0x40 + 14));
	printf("PC15 PINCFG: 0x%02x (expect PMUXEN+INEN=0x03 for MISO)\n",
		getreg8(portc + 0x40 + 15));
	printf("PC12 PMUX:   0x%02x (expect func E=4 in low/high nibble)\n",
		getreg8(portc + 0x30 + 12/2));
	printf("PC14 PMUX:   0x%02x (expect func E=4 in high nibble for PC15)\n",
		getreg8(portc + 0x30 + 14/2));
	printf("PORTC DIR:   0x%08lx (bits 12,13 set for MOSI,SCK output)\n",
		(unsigned long)getreg32(portc + 0x00));

	/* Check if SERCOM is enabled */
	uint32_t ctrla = getreg32(base + 0x00);
	if (!(ctrla & (1u << 1))) {
		printf("ERROR: SERCOM3 not enabled (CTRLA.ENABLE=0)!\n");
		return -1;
	}

	/* Verify GCLK channel 24 (SERCOM3 core) */
	uintptr_t gclk_base = 0x44050000u;
	uint32_t pchctrl24 = getreg32(gclk_base + 0x80 + 24*4); /* PCHCTRL[24] */
	printf("\nGCLK PCHCTRL[24] (SERCOM3 core): 0x%08lx (expect GEN=2, CHEN=1 → 0x00000042)\n",
		(unsigned long)pchctrl24);
	if (!(pchctrl24 & 0x40)) {
		printf("*** ERROR: SERCOM3 GCLK channel NOT ENABLED! SPI clock not running! ***\n");
	}

	/* Also check channel 22 (SERCOM1 = console) for comparison */
	uint32_t pchctrl22 = getreg32(gclk_base + 0x80 + 22*4);
	printf("GCLK PCHCTRL[22] (SERCOM1 console): 0x%08lx\n", (unsigned long)pchctrl22);

	/* Check GCLK generator 2 status */
	uint32_t genctrl2 = getreg32(gclk_base + 0x20 + 2*4); /* GENCTRL[2] */
	printf("GCLK GENCTRL[2]: 0x%08lx (expect GENEN=1, SRC=6=PLL0, DIV=3)\n",
		(unsigned long)genctrl2);

	/* Timing test: count loop iterations until RXC sets */
	volatile uint32_t count = 0;
	while (!(getreg8(base + 0x18) & (1u << 0))); /* wait DRE */
	putreg32(0x42, base + 0x28); /* write dummy byte */
	while (!(getreg8(base + 0x18) & (1u << 2))) { count++; } /* wait RXC */
	(void)getreg32(base + 0x28); /* discard */
	printf("RXC wait loops: %lu (expect >0 if clock running, 0 = instant/fake)\n",
		(unsigned long)count);

	printf("\n=== SPI Loopback Test (connect MOSI pin 16 to MISO pin 17 on EXT2) ===\n");

	/* Wait for DRE */
	int timeout = 100000;
	while (!(getreg8(base + 0x18) & (1u << 0)) && --timeout > 0);
	if (timeout == 0) {
		printf("ERROR: DRE timeout (INTFLAG=0x%02x)\n", getreg8(base + 0x18));
		return -1;
	}
	printf("DRE ready (INTFLAG=0x%02x)\n", getreg8(base + 0x18));

	/* Send 0xAA */
	printf("Sending 0xAA...\n");
	putreg32(0xAA, base + 0x28);

	/* Wait for RXC */
	timeout = 100000;
	while (!(getreg8(base + 0x18) & (1u << 2)) && --timeout > 0);
	if (timeout == 0) {
		printf("ERROR: RXC timeout (INTFLAG=0x%02x)\n", getreg8(base + 0x18));
		return -1;
	}

	uint8_t rx = (uint8_t)(getreg32(base + 0x28) & 0xFF);
	printf("Received: 0x%02X (expected 0xAA for loopback)\n", rx);

	/* Send 0x55 */
	while (!(getreg8(base + 0x18) & (1u << 0)));
	putreg32(0x55, base + 0x28);
	while (!(getreg8(base + 0x18) & (1u << 2)));
	rx = (uint8_t)(getreg32(base + 0x28) & 0xFF);
	printf("Sent 0x55, Received: 0x%02X\n", rx);

	/* Send 0xFF */
	while (!(getreg8(base + 0x18) & (1u << 0)));
	putreg32(0xFF, base + 0x28);
	while (!(getreg8(base + 0x18) & (1u << 2)));
	rx = (uint8_t)(getreg32(base + 0x28) & 0xFF);
	printf("Sent 0xFF, Received: 0x%02X\n", rx);

	if (rx == 0xFF || rx == 0x00) {
		printf("\nLoopback: MISO is %s (expected if no wire / sensor CS not asserted)\n",
			rx == 0xFF ? "HIGH" : "LOW");
	} else {
		printf("\nLoopback OK!\n");
	}

	/* === WHO_AM_I test with manual CS assertion === */
	printf("\n=== ICM-20689 WHO_AM_I Read (with CS) ===\n");

	/* Assert CS (PC14 LOW) */
	putreg32((1u << 14), 0x44840100u + 0x14); /* PORTC OUTCLR, pin 14 */
	printf("CS asserted (PC14 LOW)\n");

	/* Send WHO_AM_I register read: 0x75 | 0x80 (read bit) = 0xF5 */
	while (!(getreg8(base + 0x18) & (1u << 0)));
	putreg32(0xF5, base + 0x28);
	while (!(getreg8(base + 0x18) & (1u << 2)));
	uint8_t dummy = (uint8_t)(getreg32(base + 0x28) & 0xFF);
	printf("Sent 0xF5 (WHO_AM_I|READ), got dummy: 0x%02X\n", dummy);

	/* Send dummy 0xFF to clock out the response */
	while (!(getreg8(base + 0x18) & (1u << 0)));
	putreg32(0xFF, base + 0x28);
	while (!(getreg8(base + 0x18) & (1u << 2)));
	uint8_t whoami = (uint8_t)(getreg32(base + 0x28) & 0xFF);

	/* Deassert CS (PC14 HIGH) */
	putreg32((1u << 14), 0x44840100u + 0x18); /* PORTC OUTSET, pin 14 */
	printf("CS deasserted (PC14 HIGH)\n");

	printf("WHO_AM_I response: 0x%02X (expect 0x98 for ICM-20689)\n", whoami);
	if (whoami == 0x98) {
		printf("*** ICM-20689 DETECTED! SPI communication working! ***\n");
	} else if (whoami == 0xFF) {
		printf("Got 0xFF — sensor not responding (check Click board SPI jumpers)\n");
	} else {
		printf("Got unexpected value — might be different sensor or partial response\n");
	}

	return 0;
}

#else
int spi_test_main(int argc, char *argv[])
{
	printf("SPI3 not enabled in config\n");
	return -1;
}
#endif

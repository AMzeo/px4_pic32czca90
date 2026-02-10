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
 * @file qspi.c
 *
 * Board-level QSPI flash initialization for SAMV71-XULT.
 *
 * Uses SAMV7 QSPI in SPI compatibility mode (CONFIG_SAMV7_QSPI_SPI_MODE)
 * which wraps the QSPI hardware as a standard SPI device for the NuttX
 * W25 MTD driver.
 *
 * Data flow:
 *   sam_qspi_spi_initialize(0) -> struct spi_dev_s*
 *   w25_initialize(spi)        -> struct mtd_dev_s*
 *   register_mtddriver("/dev/mtdqspi") -> visible in NuttX VFS
 *
 * Note: SAMV71-XULT board has S25FL116K (Spansion, JEDEC 01 40 15, 2MB)
 * not SST26VF064B as documented. The W25 driver handles S25FL1xx via
 * compatible JEDEC command set (same memory type 0x40 as W25Q series).
 */

#include <nuttx/config.h>

#ifdef CONFIG_SAMV7_QSPI_SPI_MODE

#include <stdio.h>
#include <errno.h>

#include <nuttx/spi/spi.h>
#include <nuttx/mtd/mtd.h>

#include "sam_qspi_spi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define QSPI_FLASH_DEVPATH  "/dev/mtdqspi"

/****************************************************************************
 * Public Functions -- Board callbacks required by sam_qspi_spi.c
 ****************************************************************************/

/****************************************************************************
 * Name: sam_qspi_select
 *
 * Description:
 *   Board-specific chip select for QSPI SPI mode. On SAMV71-XULT, CS is
 *   hardware-managed on PA11 -- this is a required stub.
 *
 ****************************************************************************/

void sam_qspi_select(uint32_t devid, bool selected)
{
	/* CS is hardware-managed by the QSPI peripheral on PA11.
	 * With CSMODE_LASTXFER, CS asserts on TDR write and deasserts
	 * when LASTXFER is set on the last byte of an exchange.
	 */
	(void)devid;
	(void)selected;
}

/****************************************************************************
 * Name: sam_qspi_status
 *
 * Description:
 *   Return SPI device status. The flash is soldered on the board,
 *   so it is always present.
 *
 ****************************************************************************/

uint8_t sam_qspi_status(struct spi_dev_s *dev, uint32_t devid)
{
	(void)dev;
	(void)devid;
	return SPI_STATUS_PRESENT;
}

/****************************************************************************
 * Name: board_qspi_flash_init
 *
 * Description:
 *   Initialize the onboard QSPI flash via SPI compatibility mode and
 *   register it as /dev/mtdqspi.
 *
 *   Called from board_app_initialize() in init.c. Non-fatal on failure
 *   (board continues booting with SD card storage only).
 *
 * Returned Value:
 *   OK on success, negative errno on failure.
 *
 ****************************************************************************/

int board_qspi_flash_init(void)
{
	struct spi_dev_s *spi;
	struct mtd_dev_s *mtd;
	struct mtd_geometry_s geo;
	int ret;

	printf("[boot] QSPI flash init: S25FL116K via SPI mode\n");

	/* Step 1: Initialize QSPI peripheral in SPI compatibility mode */

	spi = sam_qspi_spi_initialize(0);

	if (spi == NULL) {
		printf("[boot] QSPI SPI init failed\n");
		return -ENODEV;
	}

	/* Step 2: JEDEC ID probe to verify SPI communication.
	 * Expected: S25FL116K -> 01 40 15
	 *           0xFF/0xFF/0xFF = no response (CS, clock, or wiring issue)
	 *           0x00/0x00/0x00 = bus stuck low
	 */
	{
		uint8_t jedec[3];

		SPI_LOCK(spi, true);
		SPI_SETFREQUENCY(spi, 1000000);  /* 1MHz for safe probing */
		SPI_SETMODE(spi, SPIDEV_MODE0);
		SPI_SETBITS(spi, 8);
		SPI_SELECT(spi, SPIDEV_FLASH(0), true);
		SPI_SEND(spi, 0x9f);  /* RDID command */
		jedec[0] = (uint8_t)SPI_SEND(spi, 0xff);
		jedec[1] = (uint8_t)SPI_SEND(spi, 0xff);
		jedec[2] = (uint8_t)SPI_SEND(spi, 0xff);
		SPI_SELECT(spi, SPIDEV_FLASH(0), false);
		SPI_LOCK(spi, false);

		printf("[boot] QSPI JEDEC probe: %02x %02x %02x\n",
		       jedec[0], jedec[1], jedec[2]);

		if ((jedec[0] == 0xff && jedec[1] == 0xff && jedec[2] == 0xff) ||
		    (jedec[0] == 0x00 && jedec[1] == 0x00 && jedec[2] == 0x00)) {
			printf("[boot] QSPI flash not responding (bad JEDEC ID)\n");
			return -ENODEV;
		}
	}

	/* Step 3: Initialize W25 MTD driver.
	 * Handles S25FL116K (Spansion, JEDEC 01 40 15) via compatible
	 * command set -- same memory type 0x40 as Winbond W25Q series.
	 * Returns NULL if JEDEC mismatch.
	 */

	mtd = w25_initialize(spi);

	if (mtd == NULL) {
		printf("[boot] W25/S25FL init failed (JEDEC mismatch or SPI error)\n");
		return -ENODEV;
	}

	/* Step 4: Query geometry to log flash size */

	ret = mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));

	if (ret < 0) {
		printf("[boot] QSPI flash MTDIOC_GEOMETRY failed: %d\n", ret);

	} else {
		printf("[boot] QSPI flash: %lu KB (%lu sectors of %lu bytes)\n",
		       (unsigned long)(geo.neraseblocks * geo.erasesize / 1024),
		       (unsigned long)geo.neraseblocks,
		       (unsigned long)geo.erasesize);
	}

	/* Step 5: Register MTD device */

	ret = register_mtddriver(QSPI_FLASH_DEVPATH, mtd, 0755, NULL);

	if (ret < 0) {
		printf("[boot] register_mtddriver(%s) failed: %d\n",
		       QSPI_FLASH_DEVPATH, ret);
		return ret;
	}

	printf("[boot] QSPI flash registered at %s\n", QSPI_FLASH_DEVPATH);
	return OK;
}

#endif /* CONFIG_SAMV7_QSPI_SPI_MODE */

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
 * @file init.c
 *
 * SAMV71-XULT-specific early startup code.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "board_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <nuttx/mm/gran.h>
#include <nuttx/i2c/i2c_master.h>
#include <chip.h>
#include <arch/board/board.h>
#include "arm_internal.h"

#ifdef CONFIG_SAMV7_HSMCI0
#  include "sam_hsmci.h"
#  include "board_hsmci.h"
#endif

#include <px4_arch/io_timer.h>
#include <drivers/drv_hrt.h>

/* PCK6 configuration for 1 MHz HRT clock */
#include "sam_pck.h"
#include "hardware/sam_matrix.h"
#include "hardware/sam_pmc.h"
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>

/* SAMV7 MATRIX CCFG_PCCR register - not in upstream NuttX, define locally */
#ifndef SAM_MATRIX_CCFG_PCCR
#  define SAM_MATRIX_CCFG_PCCR      (SAM_MATRIX_BASE + 0x0118)
#endif
#ifndef MATRIX_CCFG_PCCR_TC0CC
#  define MATRIX_CCFG_PCCR_TC0CC    (1 << 21)  /* TC0 clock selection */
#endif
#include <px4_platform/board_dma_alloc.h>
#include <sys/mount.h>
#include <sys/stat.h>

# if defined(FLASH_BASED_PARAMS)
#  include <parameters/flashparams/flashfs.h>
#endif

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);

/* Nocache region symbols from linker script */
extern uint32_t _s_nocache;
extern uint32_t _e_nocache;

/* Board MPU nocache region init (sam_mpuinit.c) */
extern void board_mpu_nocache_init(void);

__END_DECLS

#ifdef CONFIG_SAMV7_HSMCI0
/****************************************************************************
 * Name: samv71_sdcard_initialize
 *
 * Description:
 *   Bring up the SAMV71 HSMCI (slot 0) and mount /fs/microsd.
 *
 ****************************************************************************/
static int samv71_sdcard_initialize(void)
{
	int ret;

	printf("[sdcard] samv71_sdcard_initialize ENTRY\n");

	/* Initialize HSMCI with board-specific glue */
	ret = sam_hsmci_initialize(HSMCI0_SLOTNO, HSMCI0_MINOR, GPIO_HSMCI0_CD, IRQ_HSMCI0_CD);

	if (ret < 0) {
		printf("[sdcard] sam_hsmci_initialize FAILED: %d\n", ret);
		return ret;
	}

	printf("[sdcard] sam_hsmci_initialize returned OK\n");

	/* Wait for card initialization to complete.
	 * Card initialization happens asynchronously through callbacks after
	 * sam_hsmci_initialize returns. We need to wait for this to complete
	 * before rcS tries to mount the filesystem.
	 */
	printf("[sdcard] Waiting 1000ms for async card init...\n");
	up_mdelay(1000);

	printf("[sdcard] Wait complete, creating mount points...\n");

	/* Create mount point directory for rcS */
	(void)mkdir("/fs", 0777);
	(void)mkdir("/fs/microsd", 0777);

	/* Mount the SD card */
	printf("[sdcard] Mounting /dev/mmcsd0 to /fs/microsd...\n");
	ret = mount("/dev/mmcsd0", "/fs/microsd", "vfat", 0, NULL);
	if (ret < 0) {
		printf("[sdcard] Mount failed: %d\n", errno);
	} else {
		printf("[sdcard] Mount SUCCESS\n");
	}

	printf("[sdcard] samv71_sdcard_initialize complete\n");
	return OK;
}
#endif /* CONFIG_SAMV7_HSMCI0 */

/************************************************************************************
 * Name: board_peripheral_reset
 *
 * Description:
 *
 ************************************************************************************/
__EXPORT void board_peripheral_reset(int ms)
{
	UNUSED(ms);
}

/************************************************************************************
 * Name: board_on_reset
 *
 * Description:
 * Optionally provided function called on entry to board_system_reset
 * It should perform any house keeping prior to the rest.
 *
 * status - 1 if resetting to boot loader
 *          0 if just resetting
 *
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
	/* No PWM channels configured yet for SAMV71 - TODO */
	(void)status;  /* Unused */
}

/************************************************************************************
 * HRT PCK6 configuration (shared with hrt.c via weak symbol)
 ************************************************************************************/
volatile bool g_samv7_hrt_pck6_configured = false;

static void configure_hrt_pck6(void)
{
	uint32_t actual_freq;
	uint32_t regval;
	int timeout = 100000;

	g_samv7_hrt_pck6_configured = false;

	actual_freq = sam_pck_configure(PCK6, PCKSRC_MCK, 1000000);

	if (actual_freq != 1000000) {
		syslog(LOG_ERR, "[hrt] PCK6 configure failed (%lu Hz)\n",
		       (unsigned long)actual_freq);
		return;
	}

	sam_pck_enable(PCK6, true);

	while ((getreg32(SAM_PMC_SR) & PMC_INT_PCKRDY6) == 0) {
		if (--timeout <= 0) {
			syslog(LOG_ERR, "[hrt] PCK6 ready timeout\n");
			sam_pck_enable(PCK6, false);
			return;
		}
	}

	/* Disable MATRIX write protection (if previously enabled) */
	putreg32(MATRIX_WPMR_WPKEY, SAM_MATRIX_WPMR);

	/* Route TC0 clock to PCK6 (TC0CC = 0) */
	regval = getreg32(SAM_MATRIX_CCFG_PCCR);
	regval &= ~MATRIX_CCFG_PCCR_TC0CC;
	putreg32(regval, SAM_MATRIX_CCFG_PCCR);

	g_samv7_hrt_pck6_configured = true;
	syslog(LOG_INFO, "[hrt] PCK6 configured for 1 MHz TC0 clock\n");
}

/************************************************************************************
 * Name: sam_boardinitialize
 *
 * Description:
 *   All SAM V71 architectures must provide the following entry point.  This entry
 *   point is called early in the initialization -- after all memory has been
 *   configured and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

__EXPORT void
sam_boardinitialize(void)
{
	board_on_reset(-1); /* Reset PWM first thing */

	/* Zero out the nocache region (as it is NOLOAD) */
	uint32_t *dest;
	for (dest = &_s_nocache; dest < &_e_nocache; ) {
		*dest++ = 0;
	}

	/* Configure MPU nocache region for DMA buffers - MUST be done here,
	 * before sam_mpu_initialize() enables the MPU and before D-cache
	 * is enabled. This ensures DMA buffers are properly marked as
	 * non-cacheable from the start.
	 */
	board_mpu_nocache_init();

	/* Configure HRT clock before any timer users come up */
	configure_hrt_pck6();

	/* configure LEDs */
	board_autoled_initialize();
	led_init();

	/* configure pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));

	/* configure USB interfaces */
	sam_usbinitialize();

	/* I2C initialization moved to board_app_initialize to avoid early interrupt issues */
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 ****************************************************************************/

__EXPORT int board_app_initialize(uintptr_t arg)
{
	/* Use printf for early debug - goes directly to console */
	printf("[boot] SAMV71 board_app_initialize ENTRY\n");

	/* Note: MPU nocache region is configured in sam_boardinitialize()
	 * BEFORE the MPU is enabled and D-cache is turned on.
	 */

	px4_platform_init();

	printf("[boot] px4_platform_init done\n");

	/* IMPORTANT: Initialize DMA allocator BEFORE SD card!
	 * The SD card async probe uses DMA buffers from the nocache region.
	 */
	printf("[boot] Initializing DMA allocator...\n");
	if (board_dma_alloc_init() < 0) {
		printf("[boot] DMA alloc init FAILED!\n");
	} else {
		printf("[boot] DMA alloc init OK\n");
	}

#ifdef CONFIG_SAMV7_HSMCI0
	printf("[boot] Starting HSMCI (SD card)...\n");
	int sd_ret = samv71_sdcard_initialize();
	printf("[boot] samv71_sdcard_initialize returned: %d\n", sd_ret);
	if (sd_ret < 0) {
		printf("[boot] SD initialization failed (continuing)\n");
	}
#else
	printf("[boot] CONFIG_SAMV7_HSMCI0 NOT defined - SD card disabled!\n");
#endif

	/* Initialize I2C buses - must be after px4_platform_init */
#ifdef CONFIG_SAMV7_TWIHS0
	struct i2c_master_s *i2c0 = sam_i2cbus_initialize(0);
	if (i2c0 == NULL) {
		printf("[boot] ERROR: Failed to initialize I2C bus 0\n");
	} else {
		int ret = i2c_register(i2c0, 0);
		if (ret < 0) {
			printf("[boot] ERROR: Failed to register I2C bus 0: %d\n", ret);
		} else {
			printf("[boot] I2C bus 0 ready (/dev/i2c0)\n");
		}
	}
#endif

	drv_led_start();

	led_off(LED_RED);
	led_on(LED_GREEN); // Indicate Power
	led_off(LED_BLUE);

	if (board_hardfault_init(2, true) != 0) {
		led_on(LED_RED);
		syslog(LOG_ERR, "[boot] Hardfault init FAILED\n");
	}

	syslog(LOG_INFO, "[boot] Parameters will be stored on /fs/microsd/params\n");

	syslog(LOG_INFO, "[boot] Board initialization complete\n");

	return OK;
}

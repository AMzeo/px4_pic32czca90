/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file init.c
 *
 * PIC32CZ CA90 Curiosity Ultra board initialization for PX4.
 */

#include "board_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <syslog.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/clock.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <nuttx/usb/usbdev.h>
#include <nuttx/wqueue.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <arch/board/board.h>
#include "arm_internal.h"

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>

#ifdef CONFIG_PIC32CZCA90_SDMMC1
#  include "sam_sdmmc.h"
#endif

#ifdef CONFIG_PIC32CZCA90_EIC
#  include "sam_eic.h"
#endif

#include "hardware/sam_tcc.h"
#include "hardware/sam_gclk.h"
#include "hardware/sam_mclk.h"
#include "hardware/pic32czca90_memorymap.h"

#ifdef CONFIG_PIC32CZCA90_SERCOM3_ISSPI
#  include "sam_spi.h"
#endif

#ifdef CONFIG_PIC32CZCA90_SERCOM5_ISI2C
#  include "sam_i2c_master.h"
#  include <nuttx/i2c/i2c_master.h>
#endif

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
__END_DECLS



/**
 * Early TCC1 init — must run before any module tries io_timer_init_timer.
 * Same timing context as hrt_init (TCC0): no ISRs, no bus traffic.
 */
static void board_tcc1_early_init(void)
{
	/* MCLK: enable TCC1 APB clock (ID=42 → CLKMSK[1] bit 10) */
	uint32_t reg = SAM_MCLK_BASE + SAM_MCLK_CLKMSK_OFFSET(MCLK_ID_APB_TCC1 / 32u);
	putreg32(getreg32(reg) | (1u << (MCLK_ID_APB_TCC1 % 32u)), reg);

	/* GCLK: route GCLK1 (150 MHz) to TCC1 channel 32 */
	uint32_t pchctrl = SAM_GCLK_BASE + SAM_GCLK_PCHCTRL_OFFSET(TCC1_GCLK_ID);
	putreg32(GCLK_PCHCTRL_GEN(1) | GCLK_PCHCTRL_CHEN, pchctrl);

	while (!(getreg32(pchctrl) & GCLK_PCHCTRL_CHEN)) {
	}

	/* SWRST */
	putreg32(TCC_CTRLA_SWRST, SAM_TCC1_BASE + SAM_TCC_CTRLA_OFFSET);

	while (getreg32(SAM_TCC1_BASE + SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_SWRST) {
	}

	/* WAVE = NPWM */
	putreg32(TCC_WAVE_WAVEGEN_NPWM, SAM_TCC1_BASE + SAM_TCC_WAVE_OFFSET);

	while (getreg32(SAM_TCC1_BASE + SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_WAVE) {
	}

	/* PER = 150 MHz / 400 Hz = 375000 */
	putreg32(375000u, SAM_TCC1_BASE + SAM_TCC_PER_OFFSET);

	while (getreg32(SAM_TCC1_BASE + SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_PER) {
	}

	/* CC[0..7] = 0 (safe off) */
	for (uint8_t i = 0; i < 8; i++) {
		putreg32(0, SAM_TCC1_BASE + SAM_TCC_CC_OFFSET(i));

		while (getreg32(SAM_TCC1_BASE + SAM_TCC_SYNCBUSY_OFFSET) & (1u << (8 + i))) {
		}
	}

	/* Enable */
	putreg32(TCC_CTRLA_ENABLE, SAM_TCC1_BASE + SAM_TCC_CTRLA_OFFSET);

	while (getreg32(SAM_TCC1_BASE + SAM_TCC_SYNCBUSY_OFFSET) & TCC_SYNCBUSY_ENABLE) {
	}
}

/* LED1 heartbeat — LPWORK job, toggles at 1 Hz.
 * If LED1 stops blinking the scheduler has stalled.
 */
static struct work_s g_heartbeat_work;
static bool g_led1_state;

static void heartbeat_cb(FAR void *arg)
{
	g_led1_state = !g_led1_state;
	sam_portwrite(PORT_LED1, !g_led1_state);  /* PORT_LED1 active LOW */
	work_queue(LPWORK, &g_heartbeat_work, heartbeat_cb, NULL, MSEC2TICK(500));
}

/************************************************************************************
 * Name: arm_addregion
 *
 * Description:
 *   Add additional memory regions to the heap. Stub for boards with
 *   only single-region SRAM.
 ************************************************************************************/

#if CONFIG_MM_REGIONS > 1
void arm_addregion(void)
{
}
#endif

/************************************************************************************
 * Name: board_read_VBUS_state
 *
 * Description:
 *   J200 Target USB has no VBUS sense GPIO — always report connected.
 ************************************************************************************/

int board_read_VBUS_state(void)
{
	return 0;
}

/************************************************************************************
 * Name: board_peripheral_reset
 ************************************************************************************/
__EXPORT void board_peripheral_reset(int ms)
{
	(void)ms;
}

/************************************************************************************
 * Name: board_on_reset
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
        (void)status;
}

/************************************************************************************
 * Name: sam_board_initialize
 *
 * Description:
 *   Called from NuttX __start() in sam_start.c after clocks and memory
 *   are configured but before any devices have been initialized.
 *   The name must match exactly what sam_start.c calls.
 ************************************************************************************/

__EXPORT void
sam_board_initialize(void)
{
	board_on_reset(-1);

	/* Configure LEDs */
	board_autoled_initialize();
	led_init();

	/* Configure GPIO pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.
 ****************************************************************************/

__EXPORT int board_app_initialize(uintptr_t arg)
{
	px4_platform_init();

	/* Initialize HRT (TCC0, 150 MHz free-running + CC[0] compare ISR) */
	hrt_init();

	/* Initialize TCC1 for PWM — must be early (same context as HRT/TCC0) */
	board_tcc1_early_init();

#ifdef CONFIG_PIC32CZCA90_EIC
	sam_eic_initialize();
#endif

	/* Turn on LED0 to show NuttX booted */
	led_on(0);

	/* Start LED1 heartbeat: toggles at 1 Hz to confirm scheduler is alive */
	g_led1_state = false;
	work_queue(LPWORK, &g_heartbeat_work, heartbeat_cb, NULL, MSEC2TICK(500));

#ifdef CONFIG_PIC32CZCA90_SQI1
	{
		/* SQI1 and SDMMC1 share the same physical pins (mux H=7 vs mux I=8).
		 * Mux for SQI1 first, init flash + MTD partitions, then the SDMMC1
		 * block below remuxes them to func I=8 via its own sam_portconfig calls. */
		sam_portconfig(PORT_SQI1_CLK);
		sam_portconfig(PORT_SQI1_CS0);
		sam_portconfig(PORT_SQI1_IO0);
		sam_portconfig(PORT_SQI1_IO1);
		sam_portconfig(PORT_SQI1_IO2);
		sam_portconfig(PORT_SQI1_IO3);

		int sqi_ret = board_qspi_flash_init();
		if (sqi_ret < 0) {
			syslog(LOG_ERR, "[boot] board_qspi_flash_init: %d\n", sqi_ret);
		} else {
			sqi_ret = board_qspi_create_partitions(NULL);
			if (sqi_ret < 0) {
				syslog(LOG_ERR, "[boot] board_qspi_create_partitions: %d\n", sqi_ret);
			}
		}
	}
#endif /* CONFIG_PIC32CZCA90_SQI1 */

#ifdef CONFIG_PIC32CZCA90_SDMMC1
	{
		/* Configure SDMMC1 peripheral pins — mux I (function 8).
		 * CLK is output-only; CMD and DAT0-3 are bidirectional (INEN set).
		 * CD is a plain GPIO input with pullup (no PMUX).
		 * These pins are shared with SQI1; sam_portconfig calls here remux
		 * them from func H (SQI1=7) to func I (SDMMC1=8).
		 */
		sam_portconfig(PORT_SDMMC1_CLK);
		sam_portconfig(PORT_SDMMC1_CMD);
		sam_portconfig(PORT_SDMMC1_DAT0);
#  ifdef CONFIG_PIC32CZCA90_SDMMC1_WIDTH_D1_D4
		sam_portconfig(PORT_SDMMC1_DAT1);
		sam_portconfig(PORT_SDMMC1_DAT2);
		sam_portconfig(PORT_SDMMC1_DAT3);
#  endif
		/* CD (PC28) is configured inside sam_sdmmc1_initialize(). */

		int ret = sam_sdmmc1_slotinitialize(0);
		if (ret < 0)
		{
			syslog(LOG_ERR, "[boot] sam_sdmmc1_slotinitialize: %d\n", ret);
		}
		else
		{
#ifdef CONFIG_PIC32CZCA90_SQI1
			board_qspi_set_sdmmc_active(true);
			syslog(LOG_INFO, "[boot] SDMMC1 ready, pin-mux arbitration active\n");
#else
			syslog(LOG_INFO, "[boot] SDMMC1 ready (SQI1 disabled, no pin conflict)\n");
#endif
		}
		/* Mount is handled by rcS: mount -t vfat /dev/mmcsd0 /fs/microsd */
	}
#endif /* CONFIG_PIC32CZCA90_SDMMC1 */

#ifdef CONFIG_PIC32CZCA90_SERCOM3_ISSPI
	{
		FAR struct spi_dev_s *spi3 = sam_spibus_initialize(3);
		if (!spi3) {
			syslog(LOG_ERR, "[boot] SPI3 init failed\n");
		} else {
			syslog(LOG_INFO, "[boot] SPI3 (SERCOM3) ready\n");
		}
	}
#endif

#ifdef CONFIG_PIC32CZCA90_SERCOM5_ISI2C
	{
		FAR struct i2c_master_s *i2c5 = sam_i2cbus_initialize(5);
		if (!i2c5) {
			syslog(LOG_ERR, "[boot] I2C5 init failed\n");
		} else {
			int ret = i2c_register(i2c5, 5);
			if (ret < 0) {
				syslog(LOG_ERR, "[boot] i2c_register(5): %d\n", ret);
			} else {
				syslog(LOG_INFO, "[boot] I2C5 (SERCOM5) ready\n");
			}
		}
	}
#endif

	return OK;
}

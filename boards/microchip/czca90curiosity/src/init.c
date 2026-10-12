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
#include <unistd.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <nuttx/usb/usbdev.h>
#include <nuttx/wqueue.h>
#include <nuttx/kthread.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <arch/board/board.h>
#include "arm_internal.h"
#include "sam_sqi.h"

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>

#ifdef CONFIG_PIC32CZCA90_SDMMC0
#  include "sam_sdmmc.h"
#endif

#ifdef CONFIG_PIC32CZCA90_EIC
#  include "sam_eic.h"
#endif

#ifdef CONFIG_PIC32CZCA90_WDT
#  include "sam_wdt.h"
#  include <nuttx/timers/watchdog.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#endif

#ifdef CONFIG_PIC32CZCA90_PROGMEM
#  include "sam_progmem.h"
#endif

#include "hardware/sam_tcc.h"
#include "hardware/sam_gclk.h"
#include "hardware/sam_mclk.h"
#include "hardware/pic32czca90_memorymap.h"

#ifdef CONFIG_PIC32CZCA90_SERCOM8_ISSPI
#  include "sam_spi.h"
#  include <nuttx/spi/spi.h>
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

static bool g_led1_state;

#ifdef CONFIG_PIC32CZCA90_SQI1
extern uint32_t g_sqi_swrst_timeouts;
extern uint32_t g_sqi_clkstable_timeouts;
#endif

static int heartbeat_task(int argc, char *argv[])
{
	for (; ;) {
		g_led1_state = !g_led1_state;
		sam_portwrite(PORT_LED1, !g_led1_state);  /* PORT_LED1 active LOW */

		usleep(500000);
	}

	return 0;
}

#ifdef CONFIG_PIC32CZCA90_WDT
static int wdt_pet_task(int argc, char *argv[])
{
	int fd = open(CONFIG_WATCHDOG_DEVPATH, O_RDONLY);

	if (fd < 0) {
		_alert("WDT: failed to open %s: %d\n", CONFIG_WATCHDOG_DEVPATH, errno);
		return -errno;
	}

	ioctl(fd, WDIOC_START, 0);

	for (; ;) {
		ioctl(fd, WDIOC_KEEPALIVE, 0);
		usleep(1000000);
	}

	close(fd);
	return 0;
}
#endif

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

	/* Configure GPIO pins — GPIO_SPI8_CS_IMU (PD26) starts LOW here */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));

	/* ICM-45686 SPI mode selection: chip commits to SPI mode when CS is LOW
	 * during its power-on reset (POR) window (~1-11ms after VDD).  CS just
	 * went LOW via px4_gpio_init above.  Hold LOW for 15ms to cover the POR
	 * window, then drive HIGH so the first driver transaction gets a clean
	 * CS HIGH→LOW edge. */
	up_udelay(15000);
	sam_portwrite(GPIO_SPI8_CS_IMU, true);  /* CS HIGH — SPI mode locked in */
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

#if defined(CONFIG_PIC32CZCA90_PROGMEM) && defined(CONFIG_BOARD_CRASHDUMP)
	sam_progmem_initialize();
	if (board_hardfault_init(2, true) != 0) {
		led_on(LED_RED);
	}
#endif

	/* Initialize TCC1 for PWM — must be early (same context as HRT/TCC0) */
	board_tcc1_early_init();

#ifdef CONFIG_PIC32CZCA90_EIC
	sam_eic_initialize();
#endif

#ifdef CONFIG_PIC32CZCA90_WDT
	sam_wdt_initialize();
	kthread_create("wdt_pet", SCHED_PRIORITY_MAX - 10, 1024,
		       wdt_pet_task, NULL);
#endif

	/* LED0 solid ON = NuttX booted; LED1 blinks at 1 Hz via heartbeat */
	led_on(0);

	g_led1_state = false;
	kthread_create("heartbeat", SCHED_PRIORITY_DEFAULT - 20, 1024,
		       heartbeat_task, NULL);

#ifdef CONFIG_PIC32CZCA90_SQI1
	{
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

#ifdef CONFIG_PIC32CZCA90_SDMMC0
	{
#if !SDMMC_TEST_USE_SDMMC1
		/* SDMMC0 EXT-connector pins — skip when redirected to SDMMC1 onboard
		 * socket; sam_sdmmc_initialize() muxes PC30/PG00-03 internally. */
		sam_portconfig(PORT_SDMMC0_CLK);
		sam_portconfig(PORT_SDMMC0_CMD);
		sam_portconfig(PORT_SDMMC0_DAT0);
#  ifdef CONFIG_PIC32CZCA90_SDMMC0_WIDTH_D1_D4
		sam_portconfig(PORT_SDMMC0_DAT1);
		sam_portconfig(PORT_SDMMC0_DAT2);
		sam_portconfig(PORT_SDMMC0_DAT3);
#  else
		/* 1-bit mode: drive PC12 (SDMMC0_DAT3 = SD CS/DAT3) HIGH as GPIO
		 * to prevent card entering SPI mode if DAT3 has an external
		 * pull-down (e.g. Waveshare adapter CS pin).
		 * DFP: PC12=SDMMC0_SDDAT3 (mux I), PC13=SDMMC0_SDCMD (mux I).
		 * CMD (PC13) is already muxed to SDMMC0; only DAT3 needs
		 * explicit GPIO-HIGH here since it's unused in 1-bit mode. */
		sam_portconfig(PORT_PORTC | PORT_PIN(12) |
			       PORT_FLAG_OUTPUT | PORT_FLAG_OUTVAL_HIGH);
#  endif
		sam_portconfig(PORT_SDMMC0_CD);
#endif /* !SDMMC_TEST_USE_SDMMC1 */

		int ret = sam_sdmmc_slotinitialize(0);
		if (ret < 0) {
			syslog(LOG_ERR, "[boot] sam_sdmmc_slotinitialize: %d\n", ret);
		}
	}
#endif /* CONFIG_PIC32CZCA90_SDMMC0 */

#ifdef CONFIG_PIC32CZCA90_SERCOM8_ISSPI
	{
		FAR struct spi_dev_s *spi8 = sam_spibus_initialize(8);
		if (!spi8) {
			syslog(LOG_ERR, "[boot] SPI8 init failed\n");
		} else {
			/* ICM-45686 SPI wake-up.
			 *
			 * The chip powers on in I2C mode (CS HIGH during POR — BootROM runs
			 * before board code runs).  A clocked CS-LOW transaction (T0) switches
			 * it from I2C to SPI mode, which triggers an internal chip reset.
			 * Send dummy T0 transactions every 50ms for 500ms to keep the device
			 * in SPI mode until the driver probes it (spi.cpp waits 200ms post-T0).
			 * devid=0 hits the board fallback which drives GPIO_SPI8_CS_IMU. */
			const uint8_t icm_wake_tx[2] = {0xAA, 0x55};
			uint8_t icm_wake_rx[2];

			/* Clock-free CS LOW for 2ms — forces ICM-45686 SPI mode (re)selection.
			 * The BootROM runs for several ms with CS floating HIGH (reset-state
			 * input), which can close the ICM POR window before sam_board_initialize
			 * drives CS LOW.  This pulse recovers the ICM from I2C mode at any
			 * point after boot, matching what spi_test does before every WHO_AM_I. */
			px4_arch_gpiowrite(GPIO_SPI8_CS_IMU, false);
			up_udelay(2000);   /* 2ms clock-free CS LOW — ICM SPI mode selection */
			px4_arch_gpiowrite(GPIO_SPI8_CS_IMU, true);
			up_udelay(100000); /* 100ms settling — ICM needs time to complete internal
			                    * SPI interface reset after mode-select pulse;
			                    * spi_test's printf calls provide this implicitly */

			SPI_LOCK(spi8, true);

			/* T0: mode-switch transaction (CS low pulse → device exits I2C mode) */
			SPI_SELECT(spi8, 0, true);
			SPI_EXCHANGE(spi8, icm_wake_tx, icm_wake_rx, 2);
			SPI_SELECT(spi8, 0, false);

			/* Keep-alive: T0 every 50ms for 500ms so ICM stays in SPI mode. */
			for (int i = 0; i < 10; i++) {
				up_udelay(50000);
				SPI_SELECT(spi8, 0, true);
				SPI_EXCHANGE(spi8, icm_wake_tx, icm_wake_rx, 2);
				SPI_SELECT(spi8, 0, false);
			}

			SPI_LOCK(spi8, false);
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
			}
		}
	}
#endif

	return OK;
}

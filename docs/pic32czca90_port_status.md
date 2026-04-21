# PIC32CZ CA90 — NuttX/PX4 Port Flight Roadmap

**Branch:** `pic32czca90-port`  
**Board:** PIC32CZ CA90 Curiosity Ultra (EV16W43A)  
**MCU:** PIC32CZ8110CA90208 — Cortex-M7 @ 300 MHz, 8 MB PFM, 832 KB SRAM  
**Last updated:** 2026-04-16

---

## Hardware Summary

| Item | Value |
|------|-------|
| MCU | PIC32CZ CA90 (Cortex-M7, 300 MHz, DP-FPU) |
| PFM (Program Flash) | 8 MB at 0x0C000000 |
| BFM (Boot Flash) | 128 KB at 0x08000000 — vector table lives here |
| SRAM | 832 KB at 0x20020000 |
| DMA nocache region | 64 KB at 0x200F0000 (linker-reserved) |
| CPU clock | DFLL48M → PLL0 → 300 MHz (GCLK0) |
| Peripheral clock | GCLK1 = 150 MHz (SERCOM, TCC, HRT) |
| Slow clock | GCLK3 = 32.768 kHz (SERCOM slow, WDT) |
| Console | SERCOM1 @ PC04/PC07, 115200 baud → J700 PKOB4 USB |
| CA80 vs CA90 | Identical peripherals; CA90 adds HSM only |

**Build size (current):** ~911 KB flash (10.9% of 8 MB), ~46 KB SRAM (5.5% of 832 KB)

---

## Connector / Sensor Bus Map (Hardware-Validated)

Cross-validated against DS70005522C user guide tables 2-3 to 2-5 and Harmony `plib_port.c` / `plib_sercom*.c` examples.

| Interface | Connector | SERCOM | MCU Pins | Func | Harmony example confirmed |
|-----------|-----------|--------|----------|------|--------------------------|
| **SPI MOSI** | EXT2 pin 16 / MikroBUS | SERCOM3 PAD0 | PC12 | D | `spi_eeprom_write_read` ✓ |
| **SPI MISO** | EXT2 pin 17 / MikroBUS | SERCOM3 PAD3 | PC15 | D | `spi_eeprom_write_read` ✓ |
| **SPI SCK** | EXT2 pin 18 / MikroBUS | SERCOM3 PAD1 | PC13 | D | `spi_eeprom_write_read` ✓ |
| **SPI CS** | EXT2 pin 15 / MikroBUS | GPIO output | PC14 | — | `spi_eeprom_write_read` ✓ |
| **I2C SDA** | EXT2 pin 11 / MikroBUS | SERCOM5 PAD0 | PC25 | D | `i2c_eeprom` ✓ |
| **I2C SCL** | EXT2 pin 12 / MikroBUS | SERCOM5 PAD1 | PC26 | D | `i2c_eeprom` ✓ |
| **IMU INT** | MikroBUS pin 15 | GPIO input | PA8 | — | — |
| Console UART | EXT1 pin 13/14 + J700 | SERCOM1 | PC04/PC07 | D | `usart_echo_blocking` ✓ |
| CAN3 | J701 (ATA6561) | CAN3 | PD13/PC29 | G | — |
| CAN4 | J702 (ATA6561) | CAN4 | PA31/PA30 | G | — |

> **Note:** EXT2 and MikroBUS share the same SPI (SERCOM3) and I2C (SERCOM5) buses.  
> EXT1 UART pins 13/14 = PC4/PC7 = shared with PKoB4 console — do not use for sensor UART.

---

## Code Reference Hierarchy

When implementing any driver, use references in this order:

| Priority | Source | Purpose |
|----------|--------|---------|
| 1 | **Harmony CSP examples** on `C:\Users\I74182\Content\csp_apps_pic32cz_ca8x_ca9x\` | Exact register sequences, pin mux, init order — ground truth |
| 2 | **CA90 DFP headers** (`PIC32CZ8110CA80208_DFP/component/`) | Register addresses, bit field names, peripheral channel IDs |
| 3 | **DS70005522C** (family datasheet) | Only where DFP/Harmony agree — has known errors (§21.6 MCLK map) |
| 4 | **SAMV71 NuttX port** (`arch/arm/src/samv7/`) | Architecture only — how drivers map to NuttX APIs. Never copy register values from SAMV7 directly. |

---

## Boot Console Sequence

Expected on `/dev/ttyACM0` at 115200 baud after flash:

```
ABDE
NuttShell (NSH) NuttX-12.x
nsh>
```

| Marker | Meaning | If missing |
|--------|---------|-----------|
| A | PLL0 + UART initialized | Clock or BFM vector issue |
| B | Early serial done | sam_lowsetup failure |
| D | GPIO / LEDs initialized | Board GPIO fault |
| E | Caches on, entering nx_start() | MPU or cache config fault |
| NSH | NuttX scheduler running | SysTick / IRQ table issue |

LED0 = steady ON after boot. LED1 = 1 Hz blink (LPWORK heartbeat). **LED1 frozen = scheduler stalled.**

---

## Known Hardware Errata

| Issue | Affected address | Status |
|-------|-----------------|--------|
| DS70005522C §21.6 MCLK CLKDIV1 offset wrong | 0x44052014 causes APB bus stall on read | **Do not access 0x44052014.** DFP stride-4 layout (0x10) is correct. |
| MCLK CLKDIV[0] PAC write-protected | 0x4405200C write causes BusFault | Never write CLKDIV[0]. CPU stays at reset default (÷1 = 300 MHz). `SAM_MCLK_CPUDIV` intentionally undefined. |
| PLL0CTRL not cleared before init | (Microchip forum t400049) | **Fixed** — `sam_pll0_init()` writes PLL0CTRL=0 before REFDIV. |

---

---

# COMPLETED TASKS ✓

All items below are build-verified and confirmed working on hardware.

## Foundation Layer

- [x] **Boot ROM → BFM vector table** — `.vectors` section placed in `boot_rom` region at 0x08000000; BootROM reads SP/PC from BFM[0]/BFM[1] and jumps to `__start` in PFM (`script.ld`, `linker.ld`)
- [x] **VTOR explicit set** — `__start()` writes `NVIC_VECTAB = _vectors` as first instruction; re-affirmed in `up_irqinitialize()` (`sam_start.c`, `sam_irq.c`)
- [x] **BSS/data init correct** — fixed `(uint32_t *)_sbss` → `&_sbss` pointer arithmetic (`sam_start.c`)
- [x] **I-cache + D-cache enabled** — `CONFIG_ARMV7M_ICACHE/DCACHE=y`, write-through D-cache (`defconfig`)
- [x] **MPU enabled** — `CONFIG_ARM_MPU=y`, 16 regions, early reset (`defconfig`)
- [x] **chip.h ITCM_SIZE** — corrected to 128 KB (was 64 KB) (`arch/arm/include/pic32czca90/chip.h`)
- [x] **chip.h peripheral counts** — NSERCOM=10, NTC=0 (no TC, only TCC), NTCC=10, NCAN=6 (`chip.h`)

## Clock Tree

- [x] **PLL0 → 300 MHz** — DFLL48M / REFDIV=12 × FBDIV=225 / POSTDIV=3; PLL0CTRL cleared before config (`sam_clockconfig.c`)
- [x] **GCLK0 → 300 MHz** — SRC=6 (PLL0_1), DIV=0 → CPU clock (`sam_gclk.c`, `board.h`)
- [x] **GCLK1 → 150 MHz** — SRC=6 (PLL0_1), DIVSEL=1, DIV=0 → SERCOM1, TCC0 (`sam_gclk.c`)
- [x] **GCLK3 → 32.768 kHz** — SRC=3 (OSCULP32K) → SERCOM slow clock, WDT (`sam_gclk.c`)
- [x] **MCLK CKRDY barrier** — writes CLKDIV[1]=2 at 0x44052010, polls INTFLAG.CKRDY before switching GCLK0 to PLL0; matches Harmony `GCLK0_Initialize` exactly (`sam_clockconfig.c`)
- [x] **BOARD_CPU_FREQUENCY = 300 MHz** — cross-test confirms <0.3% skew (`board.h`)
- [x] **GCLKs SRC values corrected for CA90** — OSCULP32K=3, DFLL48M=5, PLL0_1=6 (not SAMD5x values) (`sam_gclk.h`)

## Console UART

- [x] **SERCOM1 pin mux** — PC04 PAD0 TX / PC07 PAD3 RX, func D, TXPO=0, RXPO=3 — matches Harmony (`pic32czca90_pinmap.h`, `board.h`)
- [x] **BAUD register** — GCLK1=150 MHz → BAUD=64730 → 115200 baud (`board.h`)
- [x] **IBON bit set** — USART CTRLA IBON=1 for immediate buffer overflow notification — matches Harmony (`sam_lowputc.c`)
- [x] **NSH console alive** — `nsh>` prompt verified on hardware, full keyboard input working
- [x] **NSH stable** — 15+ minutes sustained operation, all shell commands tested

## Interrupt System

- [x] **arm_vectors.c** — all 16 Cortex-M7 exception vectors route to `exception_common`; `my_hardfault` silent loop removed (`arch/arm/src/armv7-m/arm_vectors.c`)
- [x] **pic32czca90_irq.h** — 222 peripheral IRQs from Harmony `device_vectors.h` (PIC32CZ8110CA80208 DFP); was wrong SAMD5x 137-IRQ table (`arch/arm/include/pic32czca90/pic32czca90_irq.h`)
- [x] **NVIC debug handlers** — NMI, BusFault, UsageFault, PendSV, DebugMon, Reserved all call `PANIC()` under `CONFIG_DEBUG_FEATURES` (`sam_irq.c`)
- [x] **NVIC_ICTR-based init** — `nintlines = (NVIC_ICTR & INTLINESNUM_MASK) + 1` for disable/priority loops (`sam_irq.c`)
- [x] **sam_timerisr wrapper** — proper `int sam_timerisr(int irq, uint32_t *regs, void *arg)` replaces invalid direct cast (`sam_timerisr.c`)

## LED / GPIO

- [x] **LED0/LED1 active-LOW** — PB21/PB22, initial state HIGH (LED off), Harmony OUTSET-then-DIRSET sequence (`pic32czca90_pinmap.h`, `sam_port.c`)
- [x] **LED0 steady ON** after boot; **LED1 blinks 1 Hz** via LPWORK heartbeat confirming scheduler alive (`init.c`)
- [x] **SW0/SW1** — PB24/PC23, active-LOW, pullup configured (`pic32czca90_pinmap.h`, `board.h`)

## PX4 Flight Stack

- [x] **HRT (hrt.c) rewritten** — TCC0 free-running 32-bit counter at GCLK1=150 MHz; CC[0] compare match ISR drives `hrt_call_invoke()`; READSYNC protocol for COUNT reads; 64-bit µs extension via `g_base_ticks`; `hrt_reschedule()` programs next CC[0] deadline (`platforms/nuttx/src/px4/microchip/pic32czca90/hrt/hrt.c`)
- [x] **All PX4 ScheduledWorkItems firing** — commander, sensors, EKF2, navigator all execute periodic work; `ps` shows tasks Running not stuck Waiting
- [x] **USB autostart suppressed** — `param set SYS_USB_AUTO -1` in `rc.board_defaults`; prevents `/dev/ttyACM0 does not exist` spam
- [x] **DSU reads stubbed** — `board_mcu_version.c` / `board_identity.c` return fixed values; avoids APB bus stall (DSU clock not yet enabled)

## Register Headers

- [x] **sam_oscctrl.h** — PLL0 registers, DFLLMUL offset 0x003C (DFP-verified), DFLLSYNC removed
- [x] **sam_mclk.h** — CLKDIV[0] at 0x0C (PAC-protected, never defined as writeable), CLKDIV[1] at 0x10 (DFP stride-4), MCLK IDs for TCC0-9 (41-50) and SERCOM0-9 (31-40)
- [x] **sam_gclk.h** — TCC GCLK channels 31-40 (individual, DFP-verified), CAN channels 46-51, GMAC=52
- [x] **sam_tcc.h** — TCC register definitions: CTRLA, CTRLBSET, SYNCBUSY, INTENCLR, INTENSET, INTFLAG, COUNT, WAVE, PER, CC[n]; TCC0 GCLK_ID=31, MCLK_ID_APB_TCC0=41 (DFP-verified)

---

---

# PENDING TASKS

Tasks are ordered highest → lowest priority. Each tier must be substantially complete before the next tier yields a functional system.

---

## TIER 0 — Sensor Bus Drivers *(EKF2 cannot run without these)*

### T0.1 — SPI Master (SERCOM3) → IMU

> **Unlocks:** IMU → EKF2 data → arming possible  
> **Harmony reference:** `csp_apps_pic32cz_ca8x_ca9x/apps/sercom/spi/master/spi_eeprom_write_read/firmware/src/config/pic32cz_ca80_curiosity_ultra/`  
> **SERCOM3:** DOPO=PAD0 (MOSI=PC12, SCK=PC13), DIPO=PAD3 (MISO=PC15), CS=PC14 GPIO, GCLK1=150 MHz

- [ ] **0.1.1** `hardware/sam_sercom_spi.h` — SERCOM SPI register definitions (CTRLA, CTRLB, BAUD, DATA, SYNCBUSY, INTFLAG, INTENCLR, INTENSET); from Harmony `plib_sercom3_spi_master.c`
- [ ] **0.1.2** `sam_spi.c` — SERCOM3 SPI master interrupt-driven driver; NuttX `spi_dev_s` interface; BAUD formula: `baud = (f_gclk / (2 × f_spi)) - 1`
- [ ] **0.1.3** `Kconfig` — add `PIC32CZCA90_SERCOM3`, `PIC32CZCA90_SERCOM3_ISSPI`, `PIC32CZCA90_HAVE_SPI`
- [ ] **0.1.4** `Make.defs` — add `sam_spi.c` to `CHIP_CSRCS`
- [ ] **0.1.5** `defconfig` — `CONFIG_PIC32CZCA90_SERCOM3=y`, `CONFIG_PIC32CZCA90_SERCOM3_ISSPI=y`, `CONFIG_SPI=y`, `CONFIG_SPI_EXCHANGE=y`
- [ ] **0.1.6** `pic32czca90_pinmap.h` — add `PORT_SERCOM3_PAD0` (PC12, func D), `PORT_SERCOM3_PAD1` (PC13), `PORT_SERCOM3_PAD3` (PC15); `PORT_SPI3_CS` (PC14, GPIO output HIGH)
- [ ] **0.1.7** `include/px4_arch/spi_hw_description.h` — full impl: one SPI bus on SERCOM3, ICM-42688-P CS=PC14, DRDY=PA8
- [ ] **0.1.8** `boards/microchip/czca90curiosity/src/spi.cpp` — `px4_spi_buses[]` table; `sam_spi3select()`, `sam_spi3status()` functions
- [ ] **0.1.9** `boards/microchip/czca90curiosity/src/board_config.h` — add `GPIO_SPI3_CS_ICM42688P`, `GPIO_SPI3_DRDY_ICM42688P`
- [ ] **0.1.10** `boards/microchip/czca90curiosity/src/CMakeLists.txt` — add `spi.cpp`
- [ ] **0.1.11** `default.px4board` — `CONFIG_DRIVERS_IMU_INVENSENSE_ICM42688P=y`
- [ ] **0.1.12** `init/rc.board_sensors` — `icm42688p start -s -R 0`
- [ ] **0.1.13** *(Hardware)* Wire ICM-42688-P breakout to EXT2: pin 15=CS(PC14), pin 16=MOSI(PC12), pin 17=MISO(PC15), pin 18=SCK(PC13); INT→PA8 (MikroBUS pin 15)

---

### T0.2 — I2C Master (SERCOM5) → Magnetometer + Barometer

> **Unlocks:** Heading (mag) + altitude (baro) → full EKF2 state  
> **Harmony reference:** `csp_apps_pic32cz_ca8x_ca9x/apps/sercom/i2c/master/i2c_eeprom/firmware/src/config/pic32cz_ca80_curiosity_ultra/`  
> **SERCOM5:** SDA=PC25 PAD0, SCL=PC26 PAD1, func D, 400 kHz

- [ ] **0.2.1** `hardware/sam_sercom_i2c.h` — SERCOM I2C register definitions (I2CM CTRLA, CTRLB, BAUD, ADDR, DATA, STATUS, SYNCBUSY, INTFLAG); from Harmony `plib_sercom5_i2c_master.c`
- [ ] **0.2.2** `sam_i2c_master.c` — SERCOM5 I2C master driver; NuttX `i2c_master_s` interface; SCLSM=0, SDAHOLD=75ns, 400 kHz; ACK on receive
- [ ] **0.2.3** `Kconfig` — add `PIC32CZCA90_SERCOM5`, `PIC32CZCA90_SERCOM5_ISI2C`, `PIC32CZCA90_HAVE_I2C_MASTER`
- [ ] **0.2.4** `Make.defs` — add `sam_i2c_master.c`
- [ ] **0.2.5** `defconfig` — `CONFIG_PIC32CZCA90_SERCOM5=y`, `CONFIG_PIC32CZCA90_SERCOM5_ISI2C=y`, `CONFIG_I2C=y`
- [ ] **0.2.6** `pic32czca90_pinmap.h` — add `PORT_SERCOM5_PAD0` (PC25, func D), `PORT_SERCOM5_PAD1` (PC26, func D)
- [ ] **0.2.7** `include/px4_arch/i2c_hw_description.h` — full impl: one I2C bus on SERCOM5 (`initI2CBusExternal(1)`)
- [ ] **0.2.8** `boards/microchip/czca90curiosity/src/i2c.cpp` — `px4_i2c_buses[]` table
- [ ] **0.2.9** `boards/microchip/czca90curiosity/src/init.c` — call `sam_i2cbus_initialize(5)` and `px4_i2cdev_initialize()`
- [ ] **0.2.10** `boards/microchip/czca90curiosity/src/CMakeLists.txt` — add `i2c.cpp`
- [ ] **0.2.11** `default.px4board` — `CONFIG_DRIVERS_MAGNETOMETER_IST8310=y`, `CONFIG_DRIVERS_BAROMETER_BOSCH_BMP388=y`
- [ ] **0.2.12** `init/rc.board_sensors` — `ist8310 start -X`, `bmp388 start -X`
- [ ] **0.2.13** *(Hardware)* Wire IST8310 + BMP388 breakouts to EXT2 pins 11=SDA(PC25), 12=SCL(PC26); add 4.7 kΩ pull-ups to 3.3V

---

## TIER 1 — Motor Output & RC Input *(Required for any flight)*

### T1.1 — PWM Output via TCC (4-channel for quadcopter)

> **Unlocks:** Motor spin → actual flight  
> **Harmony reference:** `csp_apps_pic32cz_ca8x_ca9x/apps/tcc/` examples  
> **Note:** `sam_tcc.h` exists with basic HRT registers. PWM needs CCBUF, PATTBUF, WO output adds.

- [ ] **1.1.1** `hardware/sam_tcc.h` — extend with CCBUF[n], PATTBUF, WAVE.WAVEGEN=NPWM, per-channel WO output enable; verify TCC1 (GCLK_ID=32, MCLK_ID=42) and TCC2 (33/43) channel IDs in `sam_gclk.h` and `sam_mclk.h`
- [ ] **1.1.2** `sam_tcc_pwm.c` — TCC PWM driver; configure TCC1/TCC2 as 400 Hz NPWM; GCLK1=150 MHz → prescaler → 50 Hz or 400 Hz ESC frame; update CCBUF for glitch-free period changes
- [ ] **1.1.3** `sam_oneshot.c` — one-shot timer abstraction (NuttX lower-half timer API); used by PX4 scheduler and oneshot capture
- [ ] **1.1.4** `sam_freerun.c` — free-running counter for tick measurement (NuttX timer API)
- [ ] **1.1.5** `sam_gclk.c` — enable GCLK1 → TCC1 (channel 32) and TCC2 (channel 33) via `sam_gclk_chan_enable()`
- [ ] **1.1.6** `pic32czca90_pinmap.h` — add TCC WO output pin macros for 4× motor outputs; identify available TCC WO pins on EXT headers (verify against user guide + DFP pin mux table)
- [ ] **1.1.7** `include/px4_arch/io_timer_hw_description.h` — define 4+ PWM channels mapped to TCC1/TCC2 WO outputs
- [ ] **1.1.8** `io_pins/io_timer.c` — PX4 `io_timer` implementation over TCC; `io_timer_init()`, `io_timer_set_rate()`, `io_timer_set_ccr()`
- [ ] **1.1.9** `boards/microchip/czca90curiosity/src/board_config.h` — `DIRECT_PWM_OUTPUT_CHANNELS=4`, `BOARD_NUM_IO_TIMERS=2`
- [ ] **1.1.10** `defconfig` — enable TCC1/TCC2 Kconfig options
- [ ] **1.1.11** `default.px4board` — `CONFIG_DRIVERS_PWM_OUT=y`
- [ ] **1.1.12** *(Hardware)* Wire 4× ESC signal wires to TCC WO output pins (identify exact pins from step 1.1.6)

### T1.2 — RC Input

> **Unlocks:** Manual stick control

- [ ] **1.2.1** Decide RC input method: SBUS (inverted UART), PPM (timer capture), or DSM2/DSMX (UART); SBUS on spare SERCOM UART is recommended
- [ ] **1.2.2** Configure spare SERCOM (e.g. SERCOM0 on PA04/PA05 or SERCOM4 on PC21/PC22) as inverted UART for SBUS in `defconfig`
- [ ] **1.2.3** `default.px4board` — `CONFIG_DRIVERS_RC_INPUT=y`, `CONFIG_BOARD_SERIAL_RC="/dev/ttySx"`
- [ ] **1.2.4** `init/rc.board_defaults` — configure `RC_INPUT_PROTO` for SBUS
- [ ] **1.2.5** *(Hardware)* Wire SBUS/PPM receiver signal output to chosen SERCOM RX pin

---

## TIER 2 — Ground Control & Arming Safety

### T2.1 — USB CDC-ACM (MAVLink to GCS)

> **Unlocks:** QGroundControl connection, parameter upload/download, in-flight telemetry

- [ ] **2.1.1** `hardware/sam_usb.h` — CA90 USB FS device controller register definitions (DFP-verified; CA90 is **full-speed FS**, not HS — different IP than SAMV7)
- [ ] **2.1.2** `sam_usb.c` — USB FS device driver; register `usbdev` with NuttX; handle EP0 control, bulk IN/OUT endpoints for CDC-ACM
- [ ] **2.1.3** `defconfig` — `CONFIG_PIC32CZCA90_USB=y`; remove `# CONFIG_PIC32CZCA90_USB is not set`
- [ ] **2.1.4** `boards/microchip/czca90curiosity/src/init.c` — remove `-ENODEV` USB stubs; call real `usbdev_register()`
- [ ] **2.1.5** `init/rc.board_defaults` — restore `param set SYS_USB_AUTO 0`

### T2.2 — ADC / Battery Monitoring

> **Unlocks:** Battery voltage check → arming safety

- [ ] **2.2.1** `hardware/sam_adc.h` — ADC0/ADC1 register definitions (CTRLA, CTRLB, REFCTRL, INPUTCTRL, RESULT, SEQCTRL, SYNCBUSY, INTFLAG); from Harmony `plib_adc*.c`
- [ ] **2.2.2** `sam_adc.c` — ADC driver; 12-bit SAR; channel scan mode; GCLK source; NuttX ADC lower-half interface
- [ ] **2.2.3** `boards/microchip/czca90curiosity/src/board_config.h` — `BOARD_NUMBER_BRICKS=1`, add ADC channel macro for voltage divider pin; remove `BOARD_NUMBER_DIGITAL_BRICKS` stub
- [ ] **2.2.4** `defconfig` — enable ADC Kconfig
- [ ] **2.2.5** `default.px4board` — `CONFIG_DRIVERS_ADC_BOARD_ADC=y`
- [ ] **2.2.6** *(Hardware)* Wire battery voltage divider to ADC input pin

### T2.3 — Safety Button & Debug Commands

> **Unlocks:** Arming safety interlock; field debugging

- [ ] **2.3.1** `boards/microchip/czca90curiosity/src/board_config.h` — define `BOARD_SAFETY_BUTTON` pointing to SW0 (PB24)
- [ ] **2.3.2** `default.px4board` — add `CONFIG_DRIVERS_SAFETY_BUTTON=y`, `CONFIG_SYSTEMCMDS_I2CDETECT=y`, `CONFIG_SYSTEMCMDS_ACTUATOR_TEST=y`, `CONFIG_SYSTEMCMDS_TUNE_CONTROL=y`, `CONFIG_SYSTEMCMDS_LED_CONTROL=y`, `CONFIG_SYSTEMCMDS_BSONDUMP=y`, `CONFIG_SYSTEMCMDS_MFT=y`, `CONFIG_SYSTEMCMDS_TESTS=y`, `CONFIG_MODULES_MC_AUTOTUNE_ATTITUDE_CONTROL=y`
- [ ] **2.3.3** Tone alarm: configure one TCC WO output as PWM buzzer; implement `tone_alarm` driver hookup

---

## TIER 3 — Navigation Quality

### T3.1 — GPS (UART)

> **Unlocks:** Position hold, Return-to-home, waypoint missions

- [ ] **3.1.1** Select spare SERCOM UART for GPS TX/RX (SERCOM0 PA04/PA05 or SERCOM4 PC21/PC22 from EXT2 UART pins)
- [ ] **3.1.2** `defconfig` — configure chosen SERCOM as second USART (GPS port)
- [ ] **3.1.3** `default.px4board` — `CONFIG_DRIVERS_GPS=y`, `CONFIG_BOARD_SERIAL_GPS1="/dev/ttySx"`
- [ ] **3.1.4** *(Hardware)* Wire UBlox M8N/M9N or equivalent GNSS module to chosen SERCOM UART pins

### T3.2 — EIC (Sensor Data-Ready Interrupts)

> **Unlocks:** IMU DRDY-triggered reads instead of polling → lower latency, lower CPU load

- [ ] **3.2.1** `hardware/sam_eic.h` — EIC register definitions (CTRLA, EVCTRL, INTENCLR, INTENSET, INTFLAG, CONFIG, DEBOUNCEN, DPRESCALER, SYNCBUSY); from CA90 DFP `eic.h`
- [ ] **3.2.2** `sam_eic.c` — EIC driver; `sam_gpiosetevent()` implementation; edge-triggered DRDY interrupt on PA8 (MikroBUS INT)
- [ ] **3.2.3** `include/px4_arch/spi_hw_description.h` — set correct `drdy_gpio = GPIO_SPI3_DRDY_ICM42688P` in bus device table
- [ ] **3.2.4** `boards/microchip/czca90curiosity/src/sam_gpiosetevent.c` — board-level EIC event enable (like SAMV71 `sam_gpiosetevent.c`)

---

## TIER 4 — Performance & Persistent Storage

### T4.1 — DMA Controller (32-channel)

> **Unlocks:** High-throughput I2C/SPI transfers; prerequisite for USB HS and SDHC

- [ ] **4.1.1** `hardware/sam_dmac.h` — DMAC register definitions (BASEADDR, WRBADDR, CTRL, CHID, CHCTRLA/B, BTCTRL, BTCNT, SRCADDR, DSTADDR, DESCADDR, INTSTATUS); from CA90 DFP `dmac.h` and Harmony `plib_dmac*.c`
- [ ] **4.1.2** `sam_dmac.c` — 32-channel DMA driver; descriptor ring in nocache region (0x200F0000); NuttX DMA API; linked-list transfers
- [ ] **4.1.3** MPU nocache region — configure MPU region 0 for DMA descriptor area at 0x200F0000 (64 KB, already linker-reserved)
- [ ] **4.1.4** Update `sam_spi.c` to use DMA for transfers > 4 bytes
- [ ] **4.1.5** Update `sam_i2c_master.c` to use DMA for transfers > 4 bytes
- [ ] **4.1.6** `defconfig` — enable DMA Kconfig

### T4.2 — Parameter Storage (SQI NOR Flash)

> **Unlocks:** Parameters persist across reboots; PX4 calibration survives power cycle  
> **Hardware:** SST26VF032BAT-104I/SM (on-board 32 Mbit SQI NOR flash, Table 2-2)

- [ ] **4.2.1** `hardware/sam_sqi.h` — SQI register definitions (CA90 uses SQI, not QSPI); from CA90 DFP `sqi.h`
- [ ] **4.2.2** `sam_sqi.c` — SQI NOR flash driver; MTD layer; read/write/erase for SST26VF032BAT
- [ ] **4.2.3** `boards/microchip/czca90curiosity/nuttx-config/include/board_dma_map.h` — DMA channel allocation (like SAMV71)
- [ ] **4.2.4** `boards/microchip/czca90curiosity/src/init.c` — register MTD driver; create `/fs/mtd_params` partition
- [ ] **4.2.5** `default.px4board` — `CONFIG_BOARD_PARAM_FILE="/fs/mtd_params"`, `CONFIG_SYSTEMCMDS_MTD=y`
- [ ] **4.2.6** `defconfig` — enable MTD, NOR flash Kconfig

### T4.3 — SD Card (SDHC) for Flight Logging

> **Unlocks:** Flight logs (`ulog`), parameter backup, crash dump

- [ ] **4.3.1** `hardware/sam_sdhc.h` — SDHC register definitions; from CA90 DFP `sdhc.h`
- [ ] **4.3.2** `sam_hsmci.c` (or `sam_sdhc.c`) — CA90 SDHC driver; SDIO interface; mount `/fs/microsd`; port from SAMV71 `sam_hsmci.c` with register updates
- [ ] **4.3.3** `boards/microchip/czca90curiosity/src/init.c` — mount SD card at `/fs/microsd`
- [ ] **4.3.4** `default.px4board` — `CONFIG_BOARD_ROOT_PATH="/fs/microsd"`, `CONFIG_BOARD_PARAM_BACKUP_FILE="/fs/microsd/parameters_backup.bson"`
- [ ] **4.3.5** `defconfig` — enable SDHC, FAT filesystem Kconfig
- [ ] **4.3.6** `boards/microchip/czca90curiosity/init/rc.logging` — configure ulog destination

---

## TIER 5 — Safety, CAN-FD, Advanced

### T5.1 — Watchdog Timer

> **Unlocks:** Hardware safety — system reset if software hangs

- [ ] **5.1.1** `hardware/sam_wdt.h` — WDT register definitions; from CA90 DFP `wdt.h`
- [ ] **5.1.2** `sam_wdt.c` — WDT driver; NuttX watchdog lower-half API; kick from idle task; configurable timeout
- [ ] **5.1.3** `defconfig` — `CONFIG_WATCHDOG=y`

### T5.2 — CAN-FD (CYPHAL / UAVCAN ESC Telemetry)

> **Hardware:** CAN3 (J701) and CAN4 (J702) are already on the board with ATA6561 transceivers — just wire to CYPHAL ESC nodes

- [ ] **5.2.1** `hardware/sam_mcan.h` — MCAN register definitions; CA90 DFP `mcan.h`; MCAN IP is same as SAMV7 — near-direct port possible
- [ ] **5.2.2** `sam_mcan.c` — CAN-FD driver; port from `samv7/sam_mcan.c`; update base addresses for CA90 CAN3/CAN4
- [ ] **5.2.3** `defconfig` — enable CAN/MCAN Kconfig
- [ ] **5.2.4** `default.px4board` — enable CYPHAL/UAVCAN modules

### T5.3 — DSU Clock Enable (MCU Version / Identity)

> **Unlocks:** `ver mcu` and `ver px4guid` return real hardware IDs instead of stubs

- [ ] **5.3.1** Determine MCLK APB ID for DSU on CA90 (missing from `sam_mclk.h`); check DFP `mclk.h` for `MCLK_APBAMASK_DSU_Msk`
- [ ] **5.3.2** `sam_mclk.h` — add `MCLK_ID_APB_DSU` once confirmed
- [ ] **5.3.3** `boards/microchip/czca90curiosity/src/version/board_mcu_version.c` — enable real DSU DID read (currently stubbed)
- [ ] **5.3.4** `boards/microchip/czca90curiosity/src/version/board_identity.c` — enable real DSU serial read

---

## TIER 6 — Flight Validation & Drone Testing

All tests to be run in sequence. Do not skip ahead — each step validates the previous tier's work.

### Bench Validation (pre-flight)

- [ ] **6.1** Build + flash with T0 complete; run `icm42688p status` on NSH → expect `INFO [icm42688p] Found ICM-42688-P`
- [ ] **6.2** `listener sensor_accel` + `listener sensor_gyro` → expect data at ~1 kHz
- [ ] **6.3** `i2cdetect 1` → expect addresses: IST8310 = 0x0E, BMP388 = 0x76 or 0x77
- [ ] **6.4** `listener sensor_mag` + `listener sensor_baro` → expect valid data
- [ ] **6.5** `commander check` → zero preflight failures (accel, gyro, mag, baro all green)
- [ ] **6.6** `listener estimator_status` → EKF2 healthy flag set, innovations near zero
- [ ] **6.7** USB connected → MAVLink heartbeat visible in QGroundControl
- [ ] **6.8** Battery voltage reading in QGC shows correct value
- [ ] **6.9** RC stick input → `listener manual_control_setpoint` reflects stick movements
- [ ] **6.10** Safety button SW0 → `listener safety` shows safety state toggle
- [ ] **6.11** Full sensor calibration: accelerometer (6-axis), gyroscope, magnetometer, level
- [ ] **6.12** ESC calibration: PWM range (1000–2000 µs)
- [ ] **6.13** Motor direction check: `actuator_test -a 0 -v 0.1` per motor; verify correct rotation direction

### First Flight Tests

- [ ] **6.14** **Tethered hover** — stabilized manual mode, 10 cm AGL; verify vehicle holds attitude
- [ ] **6.15** **Free hover** — stabilized manual, 1 m AGL, <30 seconds; land immediately if oscillations seen
- [ ] **6.16** PID tune (manual or `mc_autotune_attitude_control start`); verify `top` shows CPU load < 30% at hover
- [ ] **6.17** **Altitude hold** (requires baro T0.2 + EKF2); throttle stick mid = hold height
- [ ] **6.18** **Position hold** (requires GPS T3.1); release sticks = vehicle holds position
- [ ] **6.19** **Return-to-home** test; verify RTL engages, climbs, navigates home, lands
- [ ] **6.20** Log review post-flight: check IMU noise floor, EKF innovation sequence, vibration levels, actuator saturation

---

## Summary

| Tier | Focus | Key unlocks |
|------|-------|-------------|
| **T0** | SPI + I2C sensor buses | IMU + mag + baro → EKF2 → can arm |
| **T1** | PWM output + RC input | Motor spin + manual control |
| **T2** | USB + ADC + safety button | GCS link + battery safety + arming interlock |
| **T3** | GPS + EIC | Position hold + RTH + reliable sensor DRDY IRQs |
| **T4** | DMA + SQI flash + SDHC | Persistent params + flight logging |
| **T5** | WDT + CAN-FD + DSU | Hardware safety + ESC telemetry + real MCU ID |
| **T6** | Flight testing | Validated flying system |

**Total pending tasks: ~66**  
**Minimum for first controlled hover: T0 + T1 + T2 = ~32 tasks**

---

## File Locations Quick Reference

| What | Where |
|------|-------|
| Board PX4 config | `boards/microchip/czca90curiosity/default.px4board` |
| NuttX kernel config | `boards/microchip/czca90curiosity/nuttx-config/nsh/defconfig` |
| Board clock/SERCOM config | `boards/microchip/czca90curiosity/nuttx-config/include/board.h` |
| Linker script | `boards/microchip/czca90curiosity/nuttx-config/scripts/script.ld` |
| Board init (PX4) | `boards/microchip/czca90curiosity/src/init.c` |
| Board config macros | `boards/microchip/czca90curiosity/src/board_config.h` |
| NSH startup scripts | `boards/microchip/czca90curiosity/init/rc.board_*` |
| NuttX chip drivers | `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/` |
| NuttX chip register headers | `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/hardware/` |
| NuttX pin assignments | `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/hardware/pic32czca90_pinmap.h` |
| PX4 platform layer | `platforms/nuttx/src/px4/microchip/pic32czca90/` |
| PX4 HRT | `platforms/nuttx/src/px4/microchip/pic32czca90/hrt/hrt.c` |
| SPI/I2C hw descriptions | `platforms/nuttx/src/px4/microchip/pic32czca90/include/px4_arch/` |
| SAMV71 reference board | `boards/microchip/samv71-xult-clickboards/` |
| NuttX SAMV71 chip drivers | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/` |
| Harmony examples (local) | `C:\Users\I74182\Content\csp_apps_pic32cz_ca8x_ca9x\apps\` |
| User guide (local) | `C:\Users\I74182\Downloads\PIC32CZ-CA80-CA90-Curiosity-Ultra-User-Guide-DS70005522.pdf` |
| Family datasheet (local) | `C:\Users\I74182\Downloads\PIC32CZ-CA80-CA90-Family-Data-Sheet-DS60001749.pdf` |

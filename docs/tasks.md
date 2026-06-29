# PIC32CZ CA90 — Flight Port Task Board

**Board:** PIC32CZ CA90 Curiosity Ultra (EV16W43A)
**MCU:** PIC32CZ8110CA90208 — Cortex-M7 @ 300 MHz, 8 MB Flash, 832 KB SRAM
**Status as of 2026-06-08:** SD card logging working (SDMMC1 ADMA2); SQI param storage production-ready; SPI master (SERCOM3) and I2C master (SERCOM5) drivers complete; sensor hardware testing pending.
**Branch:** `pic32czca90-port`

---

## How to Use This List

Tasks are ordered by **real dependency**, not feature category. A task cannot be started until all
tasks above it (within the same stage) are complete. Stage N+1 should not be started until Stage N
is substantially complete — with the exception that Stage 3 SPI and I2C work can overlap.

---

## ✅ COMPLETED

> Hardware-verified and running on the physical board.

### Foundation
- [x] Boot ROM → BFM vector table placement (vectors at 0x08000000, code at 0x0C000000)
- [x] VTOR explicitly set at the first instruction of `__start()`
- [x] BSS zero-fill and `.data` copy loops correct
- [x] I-cache and D-cache enabled (write-through D-cache)
- [x] MPU enabled (16 regions)
- [x] Cortex-M7 ITCM size corrected to 128 KB in chip configuration

### Clock Tree
- [x] PLL0 configured: DFLL48M ÷12 × 225 ÷3 = 300 MHz
- [x] GCLK0 → 300 MHz → CPU
- [x] GCLK1 → 150 MHz → SERCOM peripherals and HRT timer
- [x] GCLK3 → 32.768 kHz → SERCOM slow clock / watchdog
- [x] MCLK CKRDY barrier implemented correctly (matches Harmony sequence)
- [x] CPU frequency confirmed at 300 MHz via cross-timing test (<0.3% skew)
- [x] CA90-specific GCLK source values verified (differ from SAMD5x)

### Console UART
- [x] SERCOM1 configured: PC04 TX / PC07 RX, function D, 115200 baud at 150 MHz clock
- [x] NSH shell prompt (`nsh>`) verified on hardware — full keyboard input working
- [x] NSH stable under sustained operation (3+ days, all shell commands tested)

### Interrupt System
- [x] 222 peripheral IRQ table from CA90 DFP (replaces incorrect SAMD5x 137-IRQ table)
- [x] All 16 Cortex-M7 exception vectors route to panic handler
- [x] NVIC debug handlers (NMI, BusFault, UsageFault, PendSV, DebugMon) produce diagnostic output
- [x] NVIC initialisation uses hardware ICTR register for interrupt line count

### GPIO / LEDs
- [x] LED0 (PB21, active-LOW) steady ON after boot
- [x] LED1 (PB22, active-LOW) blinks at 1 Hz via low-priority work queue — confirms scheduler alive
- [x] Buttons SW0 (PB24) and SW1 (PC23) configured as inputs with pull-up

### PX4 Flight Stack Integration
- [x] HRT (High-Resolution Timer) rewritten: TCC0 free-running at 150 MHz, 64-bit microsecond counter, compare-match ISR for callbacks
- [x] All PX4 periodic tasks firing: commander, sensors module, EKF2, navigator
- [x] USB auto-start suppressed (USB driver not yet implemented)
- [x] DSU peripheral reads stubbed to prevent APB bus stall

### Register Headers (verified vs CA90 DFP)
- [x] Oscillator controller (OSCCTRL) — PLL0, DFLL registers
- [x] Main clock (MCLK) — clock dividers, peripheral APB enable IDs
- [x] Generic clock (GCLK) — generator/channel defines, CA90 peripheral channel IDs corrected from DFP
- [x] TCC timer — control, sync, interrupt, count, waveform, compare registers
- [x] SQI — BD-DMA, XIP, clock/MCLK IDs
- [x] SDMMC — ADMA2 descriptors, host controller registers
- [x] SERCOM USART — used by console driver
- [x] PORT (GPIO) — pin mux, direction, output, pull configuration
- [x] EIC — External Interrupt Controller (register definitions in NuttX submodule)

### Storage
- [x] SQI1 flash parameter storage — SST26VF032BAT, BD-DMA writes, XIP reads, D-cache safe, read-back verify
- [x] SDMMC1 SD card logging — ADMA2 mode, 4-bit bus, 0 dropouts, multi-block DMA up to 2.5 MB
- [x] Pin-mux arbitration code added (SQI1 mux=7 ↔ SDMMC1 mux=8) — dormant until coexistence enabled

---

## Stage 1 — Storage: SD Card and SQI Flash

> **Quick wins — no prerequisites beyond the completed foundation.**
> SD card (1.1) and SQI flash (1.2) are independent of each other and of the system DMA controller.
> SDMMC1 uses ADMA2 (descriptor-based DMA built into the SDMMC peripheral); SQI1 has an integrated
> BD-DMA engine. Neither requires the system DMA controller.
> Both deliver immediate value for every subsequent test session.
>
> Each sub-stage introduces exactly one new hardware abstraction so issues stay isolated.
> **1.1 SD card (ADMA2):** SDMMC1 peripheral with built-in ADMA2 descriptor engine — no system DMA required.
> **1.2 SQI flash:** SQI1 integrated BD-DMA is built into the peripheral — no system DMA required.
>
> **Runtime call order in `board_app_initialize()`:** SQI init first (params available on boot),
> SD card second.

### 1.1 — SD Card Logging (SDMMC1 ADMA2) ✅

> Flight logs and params (while SQI1 disabled due to shared-pin conflict).
> SDMMC1 ADMA2 mode — multi-block DMA transfers up to 2.5 MB confirmed.
> SQI1 temporarily disabled; params stored at `/fs/microsd/params` until
> pin-mux coexistence dance is implemented.

**IP:** CA90 **SDMMC1** (not SDMMC0). SDMMC1: `GCLK_ID=60`, `GCLK_ID_SLOW=61`, `MCLK_ID_AHB=71`, `MCLK_ID_APB=72`.
**Pins:** PC30/CLK, PG03/CMD, PC31/DAT0, PG00/DAT1, PG01/DAT2, PG02/DAT3, PC28/CD (mux=8).

- [x] **1.1.1** SDMMC register header — in NuttX submodule (`sam_sdmmc.h`)
- [x] **1.1.2** SDMMC driver — ADMA2 mode (not PIO); NuttX SDIO lower-half; GCLK4=100 MHz main, GCLK5=12 MHz slow
- [x] **1.1.3** Board init — card detect on PC28 (GPIO, active LOW); `mmcsd_slotinitialize()`; mount at `/fs/microsd`
- [x] **1.1.4** PX4 board config — `CONFIG_LOGGER=y`; `BOARD_PARAM_FILE=/fs/microsd/params`
- [x] **1.1.5** Smoke test — `ls /fs/microsd` succeeds; logger writes with 0 dropouts; multi-block DMA verified

### 1.2 — SQI Flash Parameter Storage (params + caldata + dataman)

> SQI1 has an integrated BD-DMA engine (BDCTRL/BDNXT are at offset 0x00 in the peripheral) —
> does **not** require the system DMA controller from Stage 2.1. Independent of Stage 1.1.
> See `docs/sqi_filesystem.md` for the full NuttX MTD stack walkthrough and implementation guide.

**Flash:** SST26VF032BAT-104I/SM (U602) — 4 MB, JEDEC BF 26 42, on **SQI1**.
**SQI1 (DFP instance/sqi1.h):** base `0x4F009000`, `GCLK_ID=57`, `MCLK_ID_AHB=67`. No APB clock.
**Pins:** PC31=IO0, PG0=IO1, PG1=IO2, PG2=IO3, PC30=CLK, PG3=CS0.
**BD descriptors:** must be placed in nocache MPU region (linker 0x200F0000 already reserved).

- [x] **1.2.1** SQI register header — `hardware/sam_sqi.h` (DFP-verified, all register offsets + BD descriptor struct)
- [x] **1.2.2** SQI driver — `sam_sqi.c`; BD-DMA mode; GCLK2=100 MHz, MCLK AHB ID 67; ULBPR+WBPR unlock; BD descriptors in .nocache section
- [x] **1.2.3** Custom MTD — NuttX sst26.c SPI ops incompatible with SQI BD-DMA (CS deasserts between BDs); custom `qspi_mtd_bwrite`/`qspi_mtd_erase`/`qspi_xip_bread` bypass SPI abstraction
- [x] **1.2.4** Board file `qspi.c` — JEDEC probe, WBPR/ULBPR unlock, custom MTD device, D-cache invalidation before reads, mutex concurrency protection, read-back verify with 3 retries; partition table:

  | # | Mount point          | Erase-sect offset | Sectors | Size   |
  |---|----------------------|-------------------|---------|--------|
  | 0 | `/fs/mtd_params`     | 0                 | 32      | 128 KB |
  | 1 | `/fs/mtd_caldata`    | 32                | 16      | 64 KB  |
  | 2 | `/fs/mtd_waypoints`  | 48                | 128     | 512 KB |

- [x] **1.2.5** Partition stack — `mtd_partition()` → `ftl_initialize()` → `bchdev_register()` → `px4_mtd_register_instance()`
- [x] **1.2.6** Kconfig / Make.defs — `CONFIG_PIC32CZCA90_SQI1=y`; `sam_sqi.c` in Make.defs
- [x] **1.2.7** Smoke test — `param set CBRK_SUPPLY_CHK 894281`; reboot; `param show` returns 894281 ✓

---

## Stage 2 — Bus Infrastructure: DMA and EIC

> Each sub-stage introduces exactly one new hardware abstraction so issues stay isolated.
> **2.1 System DMA:** general-purpose DMA engine — needed for DShot burst (Stage 7.1); optional SDMMC DMA upgrade.
> **2.2 EIC:** sensor DRDY interrupts — required before Stage 3 sensor drivers.
>
> **DFP peripheral name: DMA** (not "DMAC"). SQI flash (Stage 1.2) uses its own integrated BD-DMA
> and does **not** use the system DMA.

### 2.1 — System DMA

> **Needed for:** DShot burst generation (Stage 7.1).
> SQI flash (Stage 1.2) has its own integrated BD-DMA — does **not** use the system DMA.
> SDMMC1 (Stage 1.1) uses its own ADMA2 engine — does **not** use the system DMA.
> **DFP peripheral name: DMA** (not "DMAC"). Instance: `DMA_MCLK_ID_AXI=24`, `DMA_MCLK_ID_APB=25`.
> BD descriptor layout from DFP `component/dma.h`: BDNXT (next pointer), BDCFG (config),
> BDCTRLB (control B), BDEVCTRL (event control), BDSSA (source addr), BDDSA (dest addr), BDXSIZ (transfer size).

- [ ] **2.1.1** DMA register header — `hardware/sam_dma.h` derived from DFP `component/dma.h` + `instance/dma.h`; channel registers: CHCTRLA, CHCTRLB, CHEVCTRL, CHINTENCLR/SET, CHINTF, CHSSA, CHDSA; enable both MCLK AXI (ID=24) and APB (ID=25) clocks at init
- [ ] **2.1.2** DMA driver — `sam_dma.c`; channel allocation/free, BD-based transfer submit, transfer-complete ISR; NuttX DMA API (`sam_dmachannel`, `sam_dmasetup`, `sam_dmastart`, `sam_dmastop`)
- [ ] **2.1.3** MPU nocache region — configure 64 KB at `0x200F0000` (already reserved in linker) as write-through/no-cache for DMA BDs and bounce buffers; configure before first DMA transfer
- [ ] **2.1.4** Kconfig / Make.defs — compile `sam_dma.c` when `CONFIG_PIC32CZCA90_DMA=y`; wire DMA channel count from chip.h (`SAM_NDMACHAN=32`)
- [ ] **2.1.5** defconfig — enable `CONFIG_PIC32CZCA90_DMA=y`
- [ ] **2.1.6** Smoke test — DMA memory-to-memory transfer; verify correct data at destination
- [ ] **2.1.7** *(Optional)* SDMMC DMA upgrade — add DMA-backed data path to Stage 1.1 SDMMC driver; TX: `up_clean_dcache()` + DMB before transfer; RX: `up_invalidate_dcache()` after; confirm same functionality with improved throughput

### 2.2 — External Interrupt Controller (EIC)

> Sensor DRDY interrupts (IMU PA8, mag, baro) route through EIC. Without EIC the SPI/I2C
> drivers poll — workable but wastes cycles and increases latency.

- [ ] **2.2.1** EIC register header — EIC base address, CTRLA, CONFIG[0]/CONFIG[1] (interrupt sense per channel), INTFLAG, INTENSET, INTENCLR; edge sense field values
- [ ] **2.2.2** EIC driver — `sam_eic_config(pin, sense)`, `sam_eic_isr()`; configure PA8 as rising-edge for IMU DRDY; wire to NVIC
- [ ] **2.2.3** GCLK enable — EIC uses `GCLK_CHAN_EIC=5`; route GCLK3 (32 kHz) to channel 5 at startup
- [ ] **2.2.4** Kconfig / Make.defs — compile `sam_eic.c` when `CONFIG_PIC32CZCA90_EIC=y`
- [ ] **2.2.5** defconfig — enable `CONFIG_PIC32CZCA90_EIC=y`

---

## Stage 3 — Sensor Stack ✅ (hardware verified, I2C ISR optimization pending)

> SPI (IMU) and I2C (mag/baro) drivers verified on hardware with live sensor data.
> I2C runs in polled mode (ISR mode has a bug causing system freeze from work queue context).
> Polled mode limits BMI088 accel to ~11 Hz; ISR fix needed for production 200 Hz rate.
> See `docs/i2c_isr_bug_investigation.md` for full analysis and fix plan.
> EKF2 cannot produce attitude estimates until all three sensor types are delivering data.

### 3.1 — SPI Master Driver → IMU (SERCOM3) ✅

**Pins:** MOSI=PC12, MISO=PC15, SCK=PC13, CS=PC14 (GPIO), INT=PA8
**Clock:** GCLK2 = 100 MHz; polled mode (no system DMA dependency)
**Sensor:** ICM-42688-P (SPI mode 3, WHO_AM_I=0x47)

- [x] **3.1.1** SERCOM SPI register header — `hardware/sam_sercom_spi.h` with CTRLA, CTRLB, BAUD, DATA, SYNCBUSY, INTFLAG bit definitions
- [x] **3.1.2** SPI master driver — `sam_spi.c`; NuttX `spi_dev_s` interface; polled mode; SERCOM3 instance; chip-select via GPIO
- [x] **3.1.3** Kconfig — `SERCOM3_ISSPI`, `HAVE_SPI` options wired
- [x] **3.1.4** Make.defs — `sam_spi.c` included when `HAVE_SPI=y`
- [x] **3.1.5** defconfig — `CONFIG_PIC32CZCA90_SERCOM3=y`, `CONFIG_PIC32CZCA90_SERCOM3_ISSPI=y`, `CONFIG_SPI=y`, `CONFIG_SPI_EXCHANGE=y`
- [x] **3.1.6** Pin assignments — SERCOM3 PAD0(MISO)=PC15, PAD1(SCK)=PC13, PAD3(MOSI)=PC12 function C; PC14 GPIO chip-select
- [x] **3.1.7** PX4 bus table — `px4_spi_buses[]` entry for SERCOM3
- [x] **3.1.8** Board SPI wiring — `spi.cpp` with `sam_spi3select()`, `sam_spi3status()`; in board CMake
- [x] **3.1.9** Board config macros — `GPIO_SPI3_CS_ICM42688P` defined
- [x] **3.1.10** PX4 driver enable — `CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y` in default.px4board
- [x] **3.1.11** Sensor startup script — `icm20689 -s -b 3 start` in `rc.board_sensors`
- [ ] **3.1.12** Smoke test — connect sensor; `icm20689 info` shows WHO_AM_I; `listener sensor_gyro` shows data *(hardware testing pending)*
- [ ] **3.1.13** *(Hardware)* Wire IMU breakout to EXT2 header; confirm SPI bus activity on scope

### 3.2 — I2C Master Driver → Magnetometer + Barometer (SERCOM5) ✅

**Pins:** SDA=PC25 (PAD0), SCL=PC26 (PAD1), function D; 4.7 kΩ pull-ups to 3.3 V
**Clock:** GCLK2 = 100 MHz; interrupt-driven (MB/SB/ERROR ISR state machine)
**Sensors:** BMM150 magnetometer (I2C addr 0x10), BMP388 barometer (addr 0x76 or 0x77)

- [x] **3.2.1** SERCOM I2C register header — `hardware/sam_sercom_i2c.h` with I2CM CTRLA, CTRLB, BAUD, ADDR, DATA, STATUS, SYNCBUSY, INTFLAG bit definitions
- [x] **3.2.2** I2C master driver — `sam_i2c_master.c`; NuttX `i2c_master_s` interface; interrupt-driven; NACK/timeout recovery; SERCOM5 instance
- [x] **3.2.3** Kconfig — `SERCOM5_ISI2C`, `HAVE_I2C_MASTER` wired
- [x] **3.2.4** Make.defs — `sam_i2c_master.c` included when `HAVE_I2C_MASTER=y`
- [x] **3.2.5** defconfig — `CONFIG_PIC32CZCA90_SERCOM5=y`, `CONFIG_PIC32CZCA90_SERCOM5_ISI2C=y`, `CONFIG_I2C=y`
- [x] **3.2.6** Pin assignments — SERCOM5 PAD0=PC25 (SDA), PAD1=PC26 (SCL), function D; in pinmap header
- [x] **3.2.7** PX4 bus table — `px4_i2c_buses[]` entry for SERCOM5; board init calls `sam_i2cbus_initialize(5)`
- [x] **3.2.8** Board CMake — `i2c.cpp` in build
- [x] **3.2.9** PX4 driver enable — `CONFIG_DRIVERS_IMU_BOSCH_BMI088_I2C=y`, `CONFIG_DRIVERS_BAROMETER_BMP388=y`, `CONFIG_DRIVERS_MAGNETOMETER_BOSCH_BMM150=y`
- [x] **3.2.10** Sensor startup script — `bmi088_i2c`, `bmm150` start commands in `rc.board_sensors`
- [ ] **3.2.11** Smoke test — connect sensors; `i2cdetect 5` shows ACKs; `listener sensor_mag` / `listener sensor_baro` show data *(hardware testing pending)*
- [ ] **3.2.12** *(Hardware)* Wire mag and baro breakouts to EXT2 pins 11=SDA(PC25), 12=SCL(PC26); 4.7 kΩ pull-ups to 3.3 V

---

## Stage 4 — Vehicle Control

> All of Stage 3 must be done first — EKF2 must be running before attempting motor spin.
> WDT (4.4) must be enabled before any motor is powered.

### 4.1 — PWM Output via TCC (4 channels → ESCs)

> TCC register header exists (TCC0 used by HRT). PWM uses TCC1/TCC2 on separate channels.

- [ ] **4.1.1** Extend TCC register header — add CCBUF (buffered compare), PATTBUF (pattern buffer), WAVE.WAVEGEN=NPWM mode bits; verify TCC1 (GCLK_ID=32, MCLK_ID_APB=42) and TCC2 (GCLK_ID=33, MCLK_ID_APB=43)
- [ ] **4.1.2** TCC PWM driver — configure TCC1/TCC2 for NPWM at 400 Hz (analog ESCs: 50 Hz); GCLK1=150 MHz; CCBUF for glitch-free duty cycle updates
- [ ] **4.1.3** PWM pin assignments — identify 4× TCC WO output pins on EXT headers from CA90 user guide pin mux table; add to pinmap header
- [ ] **4.1.4** GCLK enable for TCC1/TCC2 — route GCLK1 to channels 32 and 33 in board clock init
- [ ] **4.1.5** IO timer hardware description — PX4 `io_timer` channel table mapping 4 PWM channels to TCC1/TCC2 wave outputs
- [ ] **4.1.6** IO timer driver — `io_timer_init()`, `io_timer_set_rate()`, `io_timer_set_ccr()`
- [ ] **4.1.7** Board config — `DIRECT_PWM_OUTPUT_CHANNELS=4`, `BOARD_NUM_IO_TIMERS=2`
- [ ] **4.1.8** PX4 board config — `CONFIG_DRIVERS_PWM_OUT=y`
- [ ] **4.1.9** Smoke test — `pwm test -c 1234 -p 1100`; oscilloscope shows 1.1 ms pulse on all 4 outputs
- [ ] **4.1.10** *(Hardware)* Wire 4× ESC signal wires to TCC WO output pins identified in 4.1.3

### 4.2 — RC Input (SBUS)

- [ ] **4.2.1** Configure spare SERCOM as inverted UART for SBUS — SERCOM0 (PA04/PA05) or SERCOM4 (PC21/PC22); 100000 baud, 2 stop bits, even parity, signal inverted
- [ ] **4.2.2** Pin assignments — add chosen SERCOM RX pin to pinmap header
- [ ] **4.2.3** PX4 board config — `CONFIG_DRIVERS_RC_INPUT=y`; set `BOARD_SERIAL_RC` to chosen SERCOM device node
- [ ] **4.2.4** Board defaults — set `RC_INPUT_PROTO=1` (SBUS) in `rc.board_defaults`
- [ ] **4.2.5** Smoke test — `rc_input status` shows stick movement; all channels in range 1000–2000 µs
- [ ] **4.2.6** *(Hardware)* Wire SBUS receiver output to chosen SERCOM RX pin; confirm signal polarity (SBUS is active-LOW = inverted)

### 4.3 — ADC / Battery Monitoring

**IP:** CA90 has **4 ADC instances (ADC0–ADC3)**, each with 4 SARCOREs.
ADC channel counts: ADC0=16+7+7+7=37 channels, ADC1-3: 7+7+7+7 channels each.
All 4 instances share `GCLK_ID=41` (`GCLK_CHAN_ADC`) and `MCLK_ID_APB=51`.

- [ ] **4.3.1** ADC register header — ADC0 base address, CTRLA, CTRLB, REFCTRL, AVGCTRL, SAMPCTRL, INPUTCTRL, INTFLAG, RESULT registers; SARCORE sub-block offsets; DFP `component/adc.h` is reference
- [ ] **4.3.2** ADC driver — NuttX ADC lower-half driver over ADC0 SARCORE0; single-ended 12-bit conversion; DMA-backed burst sampling
- [ ] **4.3.3** GCLK enable for ADC — route GCLK1 (or GCLK3) to `GCLK_CHAN_ADC=41` in board clock init
- [ ] **4.3.4** Board config — wire battery voltage divider input channel to ADC0; set `BOARD_ADC_*` macros and voltage divider ratio
- [ ] **4.3.5** PX4 board config — enable battery monitoring module; set `BOARD_BATT_V_LIST` to ADC channel
- [ ] **4.3.6** Smoke test — `adc read`; voltmeter on battery rail matches reported value within ±2%

### 4.4 — Watchdog Timer

> Must be enabled before any motor is commanded. WDT ensures the system cannot be left in a
> runaway state if a driver deadlocks or the scheduler stalls.

- [ ] **4.4.1** WDT register header — WDT base address, CTRLA (ENABLE, WEN, ALWAYSON), CONFIG (PER, WINDOW), EWCTRL (EWOFFSET), INTFLAG, CLEAR registers; DFP `component/wdt.h` is reference
- [ ] **4.4.2** WDT driver — enable with 4-second timeout; EWINT early-warning interrupt at 2 seconds; kick from lowest-priority task; disable only during flash erase (FCW)
- [ ] **4.4.3** Kconfig / Make.defs — compile `sam_wdt.c` when `CONFIG_PIC32CZCA90_WDT=y`
- [ ] **4.4.4** defconfig — `CONFIG_PIC32CZCA90_WDT=y`
- [ ] **4.4.5** PX4 integration — `CONFIG_SYSTEMCMDS_WATCHDOG=y`; kick from `commander` task loop

### 4.5 — Safety Button

- [ ] **4.5.1** Wire SW0 (PB24) to PX4 safety button input in board init (`board_get_safety_button()`)
- [ ] **4.5.2** Board config — `BOARD_SAFETY_BUTTON=GPIO_BTN_SW0`; safety logic: active-LOW, debounce 50 ms
- [ ] **4.5.3** Smoke test — `commander status` shows safety state toggling when SW0 is pressed/released

---

## Stage 5 — Connectivity and Dev Tools

> J700 (PKOB4 UART) provides the NSH console throughout development — stages 1–4 can be
> completed without USB. USBHS (J200) is needed before Stage 6 bench validation: QGroundControl
> calibration (6.2.8) and MAVLink arming require it.
>
> PROGMEM crash log is most valuable during Stage 3–4 driver development to catch panics, but
> has no hardware dependency and can be done in parallel with any stage.

### 5.1 — USBHS Device Driver (MAVLink + GCS over J200)

**IP:** CA90 USBHS (USB High-Speed). DFP `component/usbhs.h` is the register reference.
**Clock:** USBHS does not use a GCLK — it runs from a dedicated USB clock source via OSCCTRL.
**DFP (instance/usbhs0.h):** `MCLK_ID_AHB=73` (USBHS0), `MCLK_ID_AHB=74` (USBHS1).

- [ ] **5.1.1** USBHS register header — `hardware/sam_usbhs.h` derived from DFP `component/usbhs.h`; add USBHS0/USBHS1 base addresses and `MCLK_ID_AHB` defines from `instance/usbhs0.h` / `instance/usbhs1.h`
- [ ] **5.1.2** USBHS device driver — `sam_usbhs.c`; implement NuttX `usbdev_s` lower-half interface; enable MCLK AHB clock (`MCLK_ID_AHB=73` for USBHS0); configure USB clock source via OSCCTRL
- [ ] **5.1.3** Board init — remove `-ENODEV` stubs in `init.c`; call `arm_usbinitialize()`; register CDC-ACM device
- [ ] **5.1.4** defconfig — `CONFIG_USBDEV=y`, `CONFIG_CDCACM=y` (Kconfig already present; confirm Make.defs linkage to new driver)
- [ ] **5.1.5** Board defaults — `rc.board_defaults`: change `param set SYS_USB_AUTO -1` → `param set SYS_USB_AUTO 0`
- [ ] **5.1.6** Smoke test — connect J200 USB; `/dev/ttyACM1` appears on host; QGroundControl receives MAVLink heartbeat

### 5.2 — Crash Log via PROGMEM

> PROGMEM exposes the last N pages of internal PFM as an MTD device via the NuttX `up_progmem_*`
> API. `board_crashdump.c` detects `HAS_PROGMEM` at compile time and routes crash output to
> `progmem_dump.c`, which writes the register dump and RAMLOG snapshot to that MTD region.
> On the next boot, `board_hardfault_init()` detects the crash record and prints it on the console.
> Crash data survives power cycling because it is in non-volatile flash.
>
> `hardware/sam_fcw.h` already exists (DFP-verified; page/erase unit = 4096 bytes).
> `CONFIG_PIC32CZCA90_PROGMEM_NSECTORS=2` → last 2×4 KB = 8 KB of PFM reserved at runtime.
> No linker reservation required — the driver calculates the address from flash top at runtime.
> `CONFIG_SYSTEMCMDS_HARDFAULT_LOG` is **not** enabled (requires additional infrastructure not yet implemented).

- [ ] **5.2.1** `sam_progmem.c` — implement NuttX `up_progmem_*` API for CA90 using `hardware/sam_fcw.h`; key functions: `up_progmem_neraseblocks()`, `up_progmem_eraseblock()`, `up_progmem_write()`, `up_progmem_read()`, `up_progmem_ispageerased()`; reserve last `CONFIG_PIC32CZCA90_PROGMEM_NSECTORS` pages from end of PFM (0x0C000000 + 8MB − N×4096)
- [ ] **5.2.2** Make.defs — compile `sam_progmem.c` when `CONFIG_PIC32CZCA90_PROGMEM=y`
- [ ] **5.2.3** defconfig — `CONFIG_PIC32CZCA90_PROGMEM=y`, `CONFIG_PIC32CZCA90_PROGMEM_NSECTORS=2`, `CONFIG_MTD_PROGMEM=y`, `CONFIG_BOARD_CRASHDUMP=y`, `CONFIG_RAMLOG=y`, `CONFIG_RAMLOG_SYSLOG=y`, `CONFIG_RAMLOG_BUFSIZE=32768`, `CONFIG_DEBUG_HARDFAULT_ALERT=y`
- [ ] **5.2.4** Board config — define `HAS_PROGMEM` in `boards/microchip/czca90curiosity/src/board_config.h` (or `include/px4_arch/micro_hal.h`); set `PROGMEM_PATH`, `PROGMEM_FILE_COUNT`, and `PROGMEM_FILE_SIZES` constants
- [ ] **5.2.5** Board init — `board_hardfault_init(2, true)` in `board_app_initialize()` in `init.c`
- [ ] **5.2.6** Smoke test — trigger deliberate `PANIC()` from NSH; reboot; boot prompt shows crash detected; register dump visible on console

---

## Stage 6 — First Flight

> All of Stage 4 and Stage 5 must be complete. WDT running. Proceed in order — do not skip steps.

### 6.1 — GPS

- [ ] **6.1.1** Configure spare SERCOM as UART for GNSS module — SERCOM2 (PA08/PA09) recommended; 9600 baud initial, auto-baud to 115200
- [ ] **6.1.2** Pin assignments — add SERCOM2 PAD0/PAD1 to pinmap header
- [ ] **6.1.3** PX4 board config — `BOARD_SERIAL_GPS1` set to SERCOM2 device node; `CONFIG_DRIVERS_GNSS=y`
- [ ] **6.1.4** Smoke test — `gps status` shows fix type ≥3D, ≥8 satellites, PDOP <2.0
- [ ] **6.1.5** *(Hardware)* Wire u-blox M9N (or equivalent) TX/RX to SERCOM2 pins; supply 3.3 V

### 6.2 — Bench Validation (no props, no motors powered)

- [ ] **6.2.1** `listener sensor_gyro` — gyro data at 2 kHz, noise floor normal
- [ ] **6.2.2** `listener sensor_accel` — accel data present; board-level shakes register
- [ ] **6.2.3** `listener sensor_mag` — magnetometer data at ≥50 Hz; rotate board, heading changes
- [ ] **6.2.4** `listener sensor_baro` — barometer data at ≥25 Hz; breathe near board, altitude changes ±0.5 m
- [ ] **6.2.5** `ekf2 status` — EKF2 initialised; attitude estimate converged; heading within ±5° of known reference
- [ ] **6.2.6** `rc_input status` — all channels correct direction and range; switch positions match
- [ ] **6.2.7** Battery monitoring — `battery_status` topic shows correct voltage and current
- [ ] **6.2.8** USB GCS — QGroundControl connected via J200; MAVLink stream visible; all sensor values displayed
- [ ] **6.2.9** Parameter persistence — `param set SYS_AUTOSTART 4001`; reboot; confirm param survived

### 6.3 — Motor Spin Test (no props)

- [ ] **6.3.1** ESC calibration — min/max throttle calibration sequence
- [ ] **6.3.2** `motor_test test -m 1 -t 1 -v 5` — each motor spins at 5%, no smoke, correct rotation direction
- [ ] **6.3.3** `motor_test test -m 1 -t 1 -v 5` for motors 2, 3, 4 — all spin, correct direction per mixer
- [ ] **6.3.4** Reverse any wrong-direction motors via ESC programming or `MOT_*_DIR` params
- [ ] **6.3.5** Confirm arming interlock — motors do not spin without safety button press + arm sequence

### 6.4 — First Hover (fit props last)

- [ ] **6.4.1** Compass calibration completed in QGroundControl
- [ ] **6.4.2** Accelerometer calibration completed
- [ ] **6.4.3** Level calibration completed
- [ ] **6.4.4** `commander check` — all green: gyro, accel, mag, baro, RC, GPS
- [ ] **6.4.5** Flight modes — Stabilized on CH5, Position Hold on CH6
- [ ] **6.4.6** Props fitted, safety off, arm in open area — stable 30-second hover at 1 m; no oscillation
- [ ] **6.4.7** Land; disarm; inspect ESCs for heat; inspect all connections

---

## Stage 7 — Flight Operations

> Refine propulsion protocol, add telemetry radio, and validate all failure modes before
> repeated field operations. Storage (SQI params + SD logging) is complete from Stage 1.

### 7.1 — DShot ESC Protocol (replaces PWM for production)

> PWM (Stage 4.1) is sufficient for initial flight. All modern ESCs prefer DShot300/DShot600:
> no calibration needed, digital, supports bidirectional RPM telemetry. Required for production.

- [ ] **7.1.1** DShot output driver — TCC-generated DShot300 (or DShot600) frames using CC/CCBUF at 150 MHz bit timing; one channel per motor
- [ ] **7.1.2** DMA-backed DShot burst — DMA descriptor chain fills TCC CCBUF for glitch-free bit generation without CPU intervention per bit (requires Stage 2.1 system DMA)
- [ ] **7.1.3** Bidirectional DShot telemetry — GPIO + spare SERCOM UART capture RPM/temperature feedback from ESCs after each command frame
- [ ] **7.1.4** Wire DShot into PX4 `io_timer` abstraction alongside PWM; per-channel protocol selection
- [ ] **7.1.5** PX4 board config — `CONFIG_DRIVERS_DSHOT=y`; set default output protocol to DShot300
- [ ] **7.1.6** ESC arming sequence — DShot requires specific init frames; implement and test with actual ESCs
- [ ] **7.1.7** Smoke test — `dshot esc_info -m 1`; ESC returns firmware version and serial number; RPM telemetry visible in `esc_status` topic

### 7.2 — Telemetry Radio (MAVLink over serial)

> USB MAVLink works only at bench. Production needs 433/915 MHz radio for GCS connection during flight.

- [ ] **7.2.1** Configure spare SERCOM as MAVLink UART — SERCOM0 (PA04/PA05) or SERCOM2 at 57600 baud
- [ ] **7.2.2** PX4 board config — `MAV_0_CONFIG` set to chosen SERCOM device; `SER_TEL1_BAUD=57600`; `MAV_0_MODE=0` (normal MAVLink)
- [ ] **7.2.3** Smoke test — SiK radio or RFD900 connected; `mavlink status` shows link; QGroundControl connects at 100 m range

### 7.3 — Failsafe Validation

> Nominal flight (Stage 6.4) tests the green path only. Every failure mode must be tested
> before repeated field operations. Do not skip any item.

- [ ] **7.3.1** RC loss — power off transmitter mid-hover; vehicle switches to RTL or land within 1 s; `COM_RC_LOSS_T` set conservatively
- [ ] **7.3.2** Battery critical — reduce simulated battery voltage below `BAT_CRIT_THR`; confirm forced land; `BAT_CRIT_THR` documented in `rc.board_defaults`
- [ ] **7.3.3** GPS loss — disable GPS mid-flight in Position Hold; confirm graceful degradation to Altitude Hold; heading maintained
- [ ] **7.3.4** EKF2 divergence — vibrate IMU severely mid-hover; confirm EKF health flag triggers failsafe; no uncontrolled flight
- [ ] **7.3.5** All critical failsafe params documented with safe conservative values in `rc.board_defaults`

---

## Stage 8 — Field Operations

### 8.1 — PX4 Bootloader (field firmware updates over USB, no programmer needed)

> Currently firmware requires MPLAB IPE + PKOB4. Production needs field-updatable firmware.
> Bootloader lives in BFM (0x08000000, ≤128 KB); application lives in PFM (0x0C000000).

- [ ] **8.1.1** Port PX4 bootloader to CA90 — BFM region (0x08000000); update clock init, UART, USB for CA90
- [ ] **8.1.2** USB DFU — bootloader accepts `.px4` image over USB CDC via PX4 upload protocol
- [ ] **8.1.3** Bootloader watchdog — if application fails to start within 5 s, bootloader re-enters DFU mode
- [ ] **8.1.4** Linker script update — ensure application `.vectors` remain at BFM origin (already correct); application entry at PFM `0x0C000000` with VTOR updated to point there
- [ ] **8.1.5** Smoke test — `make upload` flashes correctly over USB; force DFU by holding button on boot; new image accepted and verified

### 8.2 — Endurance and Soak Testing

- [ ] **8.2.1** 24-hour continuous power-on soak — board powered, armed/disarmed every 30 minutes; monitor memory via `free` trend; confirm no leak
- [ ] **8.2.2** Thermal soak — confirm stable operation across operating temperature range (−20°C to +70°C); check for timing drift and UART framing errors at extremes
- [ ] **8.2.3** SD card endurance — 2-hour flight-rate log write; verify no SDMMC bottleneck, no log gaps, clean unmount on power-off
- [ ] **8.2.4** WDT verification — starve the WDT kick (suspend commander task); confirm system resets within configured period; crash log captures event

---

## Stage 9 — Production

### 9.1 — CAN-FD (MCAN — 6 instances)

> J701 (CAN3, ATA6561 transceiver) and J702 (CAN4, ATA6561 transceiver) are board-accessible.
> CAN3 GCLK channel=48, CAN4 GCLK channel=49. DFP `component/can.h` is the register reference.

- [ ] **9.1.1** MCAN register header — `hardware/sam_mcan.h` from DFP `component/can.h`; CAN-FD message RAM layout, CCCR, NBTP, TDCR, TXBC, RXBC, IE, IR; CA90 base addresses and MCLK IDs from `instance/can3.h` / `instance/can4.h`
- [ ] **9.1.2** MCAN driver — `sam_mcan.c`; NuttX CAN lower-half interface; CAN-FD message RAM allocation; MCLK APB/AHB clock enable; enable CAN3 and CAN4
- [ ] **9.1.3** GCLK enable — route GCLK1 to CAN3 (channel 48) and CAN4 (channel 49) in board clock init
- [ ] **9.1.4** defconfig — `CONFIG_CAN=y`; enable CAN3, CAN4 instances
- [ ] **9.1.5** Smoke test — `candump can3`; send CAN frame from USB-CAN adapter; frame received correctly

### 9.2 — DroneCAN Peripheral Support

> Requires Stage 9.1 (MCAN driver). Enables CAN-bus GPS, ESCs, power modules.

- [ ] **9.2.1** Enable DroneCAN/UAVCAN stack — `CONFIG_DRONECAN=y` (or `CONFIG_UAVCAN=y` per PX4 version); CAN3/CAN4 via J701/J702
- [ ] **9.2.2** DroneCAN GPS — connect ARK GPS or u-blox via CAN; verify GPS topic from DroneCAN node; disable serial GPS if replaced
- [ ] **9.2.3** DroneCAN ESCs — connect CAN ESCs (e.g. Zubax Myxa); verify motor commands over CAN; disable PWM/DShot for those channels
- [ ] **9.2.4** Node ID assignment — all CAN nodes get unique IDs; hot-plug/unplug does not crash bus; node IDs documented in `rc.board_defaults`

### 9.3 — DSU Peripheral (real MCU ID in logs)

> `ver mcu` / `ver all` currently return stubbed values. Needed for serial number tracking.
> **DFP confirmed:** `DSU_MCLK_ID_AHB=0`, `DSU_MCLK_ID_APB=1` (CA90 DFP `instance/dsu.h`).

- [ ] **9.3.1** Add `MCLK_ID_AHB_DSU=0` and `MCLK_ID_APB_DSU=1` to `sam_mclk.h`; enable both DSU clocks in board init
- [ ] **9.3.2** Un-stub `board_mcu_version()` and `board_get_uuid32()` — read from `CA90_DSU_DID` (0x44000120) and `CA90_DSU_UID` registers
- [ ] **9.3.3** Smoke test — `ver all` shows real chip family/revision and unique serial number

### 9.4 — Reliability Sign-off

- [ ] **9.4.1** 50-flight log review — no EKF2 divergences, no unexpected reboots, WDT never triggered, all SD logs complete
- [ ] **9.4.2** All CLAUDE.md bug history items confirmed fixed and regression-tested
- [ ] **9.4.3** Final defconfig freeze — remove all `CONFIG_DEBUG_*` overrides not needed in production; confirm flash/SRAM budget within limits
- [ ] **9.4.4** Git tag `v1.0.0-ca90` on release commit; branch merged to main

---

## Summary

| Stage | Description | Prerequisite | Key outcome |
|-------|-------------|-------------|-------------|
| ✅ Done | Boot, clocks, console, HRT, PX4 scheduler | — | NSH live, all tasks firing |
| 1.1 ✅ | SD card logging (ADMA2) | Done | Flight logs → /fs/microsd; 0 dropouts |
| 1.2 ✅ | SQI param storage | Done | Persistent params across reboot (custom MTD, BD-DMA) |
| 2 | System DMA + EIC | Stage 1 | DMA engine; sensor DRDY interrupts |
| 3 ✅ | SPI (IMU) + I2C (mag/baro) | Stage 2 | Drivers complete; hardware sensor testing pending |
| 4 | PWM + RC + ADC + WDT + safety button | Stage 3 | Motor outputs, pilot input, battery monitoring, safety interlock |
| 5 | USBHS + PROGMEM crash log | Stage 2 | GCS over J200; crash data survives reboot |
| 6 | GPS + bench validation + motor spin + hover | Stage 4 + 5 | First controlled flight |
| 7 | DShot + telemetry + failsafe validation | Stage 6 + 2.1 | Production ESC protocol, radio telemetry, failure coverage |
| 8 | Bootloader + soak testing | Stage 7 | Field firmware updates, proven stability |
| 9 | CAN-FD + DroneCAN + DSU + sign-off | Stage 8 | Production-grade, shippable |

> **Storage:** SD card (Stage 1.1, ADMA2) for flight logs; SQI1 SST26VF032BAT 4 MB (Stage 1.2)
> for params/caldata/dataman. Currently SQI1 disabled due to shared-pin conflict with SDMMC1 —
> params stored at `/fs/microsd/params` until pin-mux coexistence is implemented.
> SQI uses its own integrated BD-DMA — does not require system DMA. System DMA (Stage 2.1) is
> needed for DShot burst generation (Stage 7.1).

**Minimum path to first hover: Stages 1–6.**
**Minimum path to production: all stages.**

---

## Post-Verification Update Checklist

> **When any hardware test passes or a driver finding changes, update ALL files below.**
> This prevents documentation drift. Walk this list after every flash + test session.
>
> **Full paths of every file referenced below** (from repo root):
>
> | Short name | Full path |
> |-----------|-----------|
> | `tasks.md` | `docs/tasks.md` |
> | `CLAUDE.md` | `CLAUDE.md` (repo root) |
> | `README.md` | `README.md` (repo root) |
> | `board.h` (PX4) | `boards/microchip/czca90curiosity/nuttx-config/include/board.h` |
> | `board.h` (NuttX) | `platforms/nuttx/NuttX/nuttx/boards/arm/pic32czca90/pic32czca90-curiosity/include/board.h` |
> | `pinmap.h` | `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/hardware/pic32czca90_pinmap.h` |
> | `default.px4board` | `boards/microchip/czca90curiosity/default.px4board` |
> | `rc.board_defaults` | `boards/microchip/czca90curiosity/init/rc.board_defaults` |
> | `rc.board_sensors` | `boards/microchip/czca90curiosity/init/rc.board_sensors` |
> | `board_config.h` | `boards/microchip/czca90curiosity/src/board_config.h` |
> | `init.c` | `boards/microchip/czca90curiosity/src/init.c` |
> | `qspi.c` | `boards/microchip/czca90curiosity/src/qspi.c` |
> | `spi.cpp` | `boards/microchip/czca90curiosity/src/spi.cpp` |
> | `i2c.cpp` | `boards/microchip/czca90curiosity/src/i2c.cpp` |
> | `CMakeLists.txt` | `boards/microchip/czca90curiosity/src/CMakeLists.txt` |
> | `sqi_hardware_behavior.md` | `docs/sqi_hardware_behavior.md` |
> | `MEMORY.md` | `~/.claude/projects/.../memory/MEMORY.md` (auto-memory, not in repo) |
> | `project_status.md` | `~/.claude/projects/.../memory/project_status.md` (auto-memory) |

### After ANY driver test passes or fails on hardware:

| What changed | Files to update | What to write |
|--------------|----------------|---------------|
| SPI sensor responds (WHO_AM_I OK) | `docs/tasks.md` §3.1.12, `CLAUDE.md` §P5 Win line, `README.md` status table row "SPI / IMU" | Mark `[x]`, change "pending" → "verified on hardware", add sensor model + WHO_AM_I value |
| I2C device ACKs on bus scan | `docs/tasks.md` §3.2.11, `CLAUDE.md` §P6 Win line, `README.md` status table row "I2C / mag / baro" | Mark `[x]`, add I2C address + device name confirmed |
| EKF2 converges with real sensors | `docs/tasks.md` §6.2.5, `CLAUDE.md` "Done" bullet list (after GCLK2 line) | Add "EKF2 converges with real sensor data ✓" to Done list |
| New silicon quirk discovered | `docs/sqi_hardware_behavior.md` (or new `docs/<peripheral>_hardware_behavior.md`), `CLAUDE.md` Bug History table | Document: symptom, register, root cause, fix, verification |
| A "hardware bug" claim proven wrong | grep the claim → fix ALL hits in source comments, `.h` headers, doc files, test file headers, `CLAUDE.md` bug table | Remove or correct everywhere; make test file headers neutral |
| New GCLK/MCLK ID used | `CLAUDE.md` GCLK table + Clock Tree section, `board.h` (PX4), `board.h` (NuttX) — keep identical | Add generator row with source, DIV, output, peripheral |
| Pin mux changed | `CLAUDE.md` pin references, `pinmap.h`, `board.h` (PX4), `board.h` (NuttX) | Sync all four; check for conflicts with SDMMC/SQI shared pins |
| Build size changes significantly (>5%) | `CLAUDE.md` "Current build size" line, `docs/tasks.md` header block | Update KB and % values |
| CONFIG option added/removed in defconfig | `default.px4board`, `CLAUDE.md` if major peripheral | Document what the option enables and any dependencies |
| Param default changed | `rc.board_defaults`, `CLAUDE.md` if the param is referenced there | Keep both in sync; note reason for change |
| Sensor driver changed (different part#) | `rc.board_sensors`, `default.px4board`, `board_config.h`, `docs/tasks.md` §3.x | Update start command, CONFIG driver, bus table |

### After a driver is marked DONE:

1. `docs/tasks.md` — mark ALL sub-items `[x]`; add `✅` to section header
2. `CLAUDE.md` — move from "Pending" P*n* section to "Done" bullet list; remove from "Missing headers" table if header now exists
3. `README.md` — update status column (change `🔧 In progress` or `🔲 Pending` → `✅ Done` or `✅ Driver done`)
4. `MEMORY.md` + `project_status.md` — update description line and status section
5. Run the grep commands below — fix any remaining stale references

### After a claim is invalidated (e.g., "PIO doesn't work" → "PIO works"):

1. Grep the repo for the exact claim text AND synonyms (see commands below)
2. Fix every hit: source `.c`/`.h` comments, doc `.md` files, test file headers, `CLAUDE.md` bug table
3. If a test file's header asserts the old conclusion, rewrite it as neutral (describe what it *tests*, not what it *proves*)
4. If any `.h` header has "NEVER do X" or "NOT for <silicon>" warnings tied to the old claim, remove them
5. If `docs/tasks.md` references the old claim in a completed item description, update it

### Quick grep commands (run from repo root after any verification session):

```bash
# Find stale "not functional" / "broken" / "does not work" / "NEVER" claims
grep -rn "not function\|broken\|does not work\|NEVER.*write\|NOT for" \
  platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/ \
  boards/microchip/czca90curiosity/ \
  docs/tasks.md docs/sqi_hardware_behavior.md docs/sqi_filesystem.md \
  CLAUDE.md README.md

# Find stale status markers
grep -rn "In progress\|in progress\|🔧\|not yet implemented\|not yet done" \
  docs/tasks.md README.md CLAUDE.md

# Find "pending" in our port docs (filter out the checklist itself)
grep -n "pending" docs/tasks.md CLAUDE.md README.md | grep -v "Post-Verification\|checklist\|Quick grep\|testing pending"
```

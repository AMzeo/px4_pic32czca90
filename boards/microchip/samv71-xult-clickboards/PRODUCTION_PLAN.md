# SAMV71 PX4 Port: Production Implementation Plan

**Rev 3 — 2026-02-10** (Phase 0, Phase 1, Phase 4C completed)

## Context

The SAMV71-XULT PX4 port is currently a working prototype with 4-channel PWMC PWM, IMU, baro, mag, GPS, battery ADC, safety, SD card, USB, RC serial input, and HRT. The goal is to bring this to production quality as a general-purpose dev platform, with a custom production PCB planned. The primary gaps are: io_timer allocation API (foundation for everything else), DShot output, input capture for RC, and board-level production hardening.

**Branch:** `samv7-custom`
**User priorities:** IO timer API first, then DShot, bidirectional DShot deferred (architecture must allow it later)
**Supersedes:** `PRODUCTION_READINESS.md` (stale — incorrectly states ADC missing, 3 TC channels)

---

## Known Risks & Corrections (from Rev 1 review)

| # | Severity | Finding | Correction in this rev |
|---|----------|---------|----------------------|
| 1 | Critical | `board_on_reset()` is a no-op (init.c:189) — safety hole exists NOW | Promoted to Phase 0 as production blocker |
| 2 | Critical | API convergence with STM32 io_timer underscoped — consumers (pwm_servo, dshot, RPMCapture, PPSCapture) expect full typed enum, allocation tables, channel mapping, callback signatures | Phase 1 reframed as "API convergence + safety closure + migration" with explicit consumer compatibility matrix |
| 3 | High | `PRODUCTION_READINESS.md` stale (says no ADC, 3 TC channels) | Marked superseded; Phase 0 adds deprecation header |
| 4 | High | Phase 3 input capture not integrated with current serial RC strategy (`rc.board_defaults` uses UART4) | Phase 3 now includes param/startup integration and coexistence plan |
| 5 | High | Phase 5 `PWMCPeripheral::C` missing from enum (only A/B); `MAX_IO_TIMERS` hardcoded to 4 not board-derived | Phase 1 fixes MAX_IO_TIMERS derivation; Phase 5 adds Periph C/D |
| 6 | Medium | Debug instrumentation (absolute `getreg32`, `PX4_INFO` spam) is production blocker, not cleanup nice-to-have | Phase 0 reframed as production blocker |
| 7 | Medium | Dead legacy files `io_timer_tc.c`, `io_timer_stub.c` still on disk (not in CMakeLists but confusing) | Phase 0 marks deprecated with header comments |
| 8 | Medium | `initIOTimerChannel()` Timer5 → index 4, but `MAX_IO_TIMERS=4` (0-3 valid) — latent bounds issue | Phase 1 fixes MAX_IO_TIMERS to be board-derived via `BOARD_NUM_IO_TIMERS` |

---

## Phase 0: Safety Closure & Cleanup ~~(Production Blocker)~~ DONE
**Complexity: S | Dependency: None | Dev Board**
**Status: COMPLETE** — Committed `9a35d09e23` (2026-02-08)

All sub-phases completed:
- **0A** `board_on_reset()` — Disables all PWM0 channels and drives motor GPIOs low on any reset
- **0B** Debug instrumentation removed from `io_timer_pwmc.c`
- **0C** Legacy files (`io_timer_tc.c`, `io_timer_stub.c`) marked deprecated
- **0D** Stale docs (`PRODUCTION_READINESS.md`, `QSPI_FLASH_IMPLEMENTATION_PLAN.md`) marked superseded
- **0E** Uncommitted files staged and committed

Verified: build clean, 4-ch PWM works, PWM outputs go low on reboot

---

## Phase 1: IO Timer API Convergence & Allocation — DONE
**Complexity: L | Dependency: Phase 0 | Dev Board**
**Status: COMPLETE** — Committed `2a14f110d8` (2026-02-08)

Foundation phase. Full API convergence with STM32 io_timer — all consumer-facing signatures match.

### What Was Implemented

- **1A** `io_timer.h` converged with STM32: proper `io_timer_channel_mode_t` enum (13 modes including Dshot/OneShot/PPS/RPM), `dshot_conf_t` with XDMAC fields, extended `io_timers_t` (clock_freq, dshot), extended `timer_io_channels_t` (masks, ccr_offset), STM32-compatible `channel_handler_t` signature, `io_timers_channel_mapping_element_t` struct
- **1B** `io_timer_hw_description.h` updated: `initIOPWMTimer()` populates clock_freq/vectorno/dshot, `initIOPWMChannel()` populates masks/ccr_offset, proper channel mapping struct
- **1C** `io_timer_pwmc.c` allocation logic: `channel_allocations[]` / `timer_allocations[]` state tracking, `io_timer_allocate_timer/channel()` + `unallocate` functions with irqsave critical sections, IRQ handler dispatch (`io_timer_handler0/1`), `io_timer_channel_get_gpio_output()`, OneShot support in `io_timer_trigger()` / `io_timer_set_enable()`
- **1D** Board files updated: `BOARD_NUM_IO_TIMERS=1`, `pwm_servo.c` updated for mode param, channel mapping uses new struct

Verified: build clean, 4-ch PWM works, allocation conflict returns -EBUSY, gpio_output returns valid config

<details>
<summary>Consumer Compatibility Matrix (reference)</summary>

| Consumer | Functions Called | Status |
|----------|----------------|--------|
| `pwm_servo.c` (SAMV7) | `io_timer_channel_init(ch, PWMOut, NULL, NULL)`, `io_timer_set_ccr()`, `io_timer_set_enable()` | ✅ Works |
| `src/drivers/dshot/dshot.c` | `io_timer_allocate_timer()`, `io_timer_set_dshot_burst_mode()`, `io_timer_update_dma_req()`, `io_timer_unallocate_timer/channel()` | ✅ API ready (DShot driver is Phase 2) |
| `src/drivers/pwm_out/PWMOut.cpp` | `io_timer_channel_init()`, `io_timer_set_pwm_rate()`, `io_timer_set_ccr()`, `io_timer_get_group()` | ✅ Works |
| `src/drivers/rpm/RPMCapture.cpp` | `io_timer_allocate_channel(ch, RPM)`, `io_timer_channel_get_gpio_output()`, `io_timer_unallocate_channel()` | ✅ API ready |
| `src/drivers/pps_capture/PPSCapture.cpp` | `io_timer_allocate_channel(ch, PPS)`, `io_timer_channel_get_as_pwm_input()`, `io_timer_unallocate_channel()` | ✅ API ready |
| `input_capture.c` (Phase 3) | `io_timer_channel_init(ch, Capture, handler, ctx)` with full callback signature | ✅ API ready |
| `board_on_reset()` (50+ boards) | `io_timer_channel_get_gpio_output(i)` in loop | ✅ Implemented |

</details>

---

## Phase 2: DShot Output (Unidirectional)
**Complexity: XL | Dependency: Phase 1 | Dev Board**

### Architecture Decision: PWMC Sync Mode + XDMAC

SAMV7 has no DMAR burst equivalent. Instead, use **PWMC Synchronous Channel Mode (SCM)**:
- Mark channels as synchronized (SCM.SYNCx bits)
- Set UPDM=2 (DMA update mode via DMAR register)
- XDMAC writes 24-bit duty values to DMAR; hardware distributes to channels sequentially
- Each period: 4 DMAR writes (one per sync channel) = one DShot bit across all motors

**DShot timing (MCK=150MHz):**
| Protocol | Bit Period | CPRE | Clock | CPRD | Bit-1 (75%) | Bit-0 (37.5%) |
|----------|-----------|------|-------|------|-------------|---------------|
| DShot150 | 6.67 us | 1 (MCK/2) | 75 MHz | 500 | 375 | 187 |
| DShot300 | 3.33 us | 1 (MCK/2) | 75 MHz | 250 | 187 | 93 |
| DShot600 | 1.67 us | 1 (MCK/2) | 75 MHz | 125 | 93 | 46 |

**DMA buffer layout:** 17 periods x 4 channels = 68 words per XDMAC transfer (16 data bits + 1 zero pad)

### New Files
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/dshot.h` — SAMV7-specific DShot defines
- `platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c` — Full DShot driver
- `platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt`

### Key Functions to Implement
```c
int up_dshot_init(uint32_t channel_mask, unsigned dshot_pwm_freq, bool enable_bidirectional);
void up_dshot_trigger(void);     // Flush dcache, start XDMAC to DMAR
int up_dshot_arm(bool armed);
void dshot_motor_data_set(unsigned channel, uint16_t data, bool telemetry);
// Bidir stubs: up_bdshot_get_erpm() returns -ENOSYS, etc.
```

### `io_timer_pwmc.c` Additions
- `io_timer_set_dshot_mode()`: Reconfigure PWMC for DShot timing (CPRE=1, CPRD per protocol)
- `io_timer_update_dma_req()`: Enable/disable WRDY interrupt in IER2 for DMA triggering
- `io_timer_channel_init()`: Handle `IOTimerChanMode_Dshot` (same GPIO setup as PWMOut, timing set by dshot.c)

### API Compatibility Note
STM32 consumers call `io_timer_set_dshot_burst_mode(timer, freq, dma_burst_length)` (3 params).
SAMV7 uses `io_timer_set_dshot_mode(timer, freq)` (2 params) because PWMC Sync Mode handles distribution automatically — no burst length needed.
The platform-specific DShot driver (`dshot.c`) is the only caller, so this signature difference is safe (each platform has its own dshot.c).

### Build System
- `platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt`: Add `add_subdirectory(dshot)`
- `boards/.../default.px4board`: Enable `CONFIG_DRIVERS_DSHOT=y`

### Bidir Extension Point
Architecture preserves clean path to bidirectional DShot:
- `dshot_conf_t` has `xdmac_ch_rx[]` and `tc_capture_base` fields (unused now)
- `IOTimerChanMode_DshotInverted` and `IOTimerChanMode_CaptureDMA` are in the enum
- Custom PCB must route motor pins to pads with both PWMC and TC alternate functions

### Verify
- Build succeeds with DShot driver linked
- Scope: DShot150 bit pattern — Bit-1 = 75% duty @ 6.67us, Bit-0 = 37.5% @ 6.67us
- BLHeli32/AM32 ESC: motor spins with `dshot` driver
- DShot commands: beacon, direction change
- No regression: PWM mode still works when DShot parameter not set

---

## Phase 3: Input Capture (TC-based)
**Complexity: M | Dependency: Phase 1 | Dev Board (TC5/PC29)**

Input capture uses TC (Timer/Counter), not PWMC. TC5 on PC29/TIOA5 is already reserved for RC.

### Integration with Current RC Strategy

**Current state:** Board defaults use serial RC on UART4/ttyS3/PD18 (`rc.board_defaults` line 43: `RC_PORT_CONFIG 300`).
**Pin situation:** Serial RC (UART4/PD18) and PPM capture (TC5/PC29) are on **different pins** — no
hardware conflict. Both could be configured simultaneously, but PX4 typically uses only one RC input
method at a time.

**Note:** PD18 does have a separate conflict (SD Card Detect vs UART4 RX), documented in
[SAMV71_PIN_MAP.md](SAMV71_PIN_MAP.md). That conflict is unrelated to TC5 capture.

**Coexistence plan:**
- Serial RC (SBUS/CRSF on UART4/PD18) is the default and most common for modern receivers
- PPM capture (TC5/PC29) is an alternative for legacy PPM receivers
- Use existing `RC_PORT_CONFIG` param to select:
  - `RC_PORT_CONFIG = 300` → UART4 serial RC (current default, SBUS/CRSF)
  - `RC_PORT_CONFIG = 0` + PPM module started → TC5 capture mode (PPM RC)
- `rc.board_defaults` keeps serial RC as default
- Startup script (`rc.board_sensors` or equivalent) must check param and conditionally start PPM capture module

### New File
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/input_capture.c`

### TC Capture Mode Configuration
- TC_CMR: WAVE=0 (capture mode), TCCLKS=MCK/2 (75MHz), LDRA=rising, LDRB=falling
- TC_RA: captured counter on rising edge
- TC_RB: captured counter on falling edge
- TC_SR: LDRAS/LDRBS flags → interrupt → callback

### API (matches STM32 `input_capture.c`)
```c
int up_input_capture_set(unsigned channel, input_capture_edge edge,
    capture_filter_t filter, capture_callback_t callback, void *context);
int up_input_capture_get_filter(unsigned channel, capture_filter_t *filter);
int up_input_capture_set_filter(unsigned channel, capture_filter_t filter);
int up_input_capture_get_trigger(unsigned channel, input_capture_edge *edge);
int up_input_capture_set_trigger(unsigned channel, input_capture_edge edge);
```

### Build
- Add `input_capture.c` to `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`

### Verify
- Serial RC (default): SBUS receiver on UART4 still works, TC5 not initialized
- PPM RC (param switch): PPM receiver on TC5/PC29, `rc_input` module receives valid channels
- Pulse widths match scope measurements (timing accuracy < 1us)
- Switching between serial and PPM RC via param change + reboot works correctly

---

## Phase 4: Production Hardening
**Complexity: M | Dependency: Phase 1 | Dev Board**

### 4A: PWMC Fault Protection
**File:** `io_timer_pwmc.c`
- Configure FMR/FPE/FPV registers: outputs go LOW on fault
- Connect to software fault for `board_on_reset()` safety
- Note: Phase 0A already provides basic reset safety via direct register writes; this phase adds hardware-level fault protection that works even if software is hung

### 4B: PWM Watchdog
- HRT callback checks timestamp per channel
- If `io_timer_set_ccr()` not called within timeout → force duty to zero

### Verify
- PWMC outputs go low on fault trigger
- Motors stop if update loop stalls (watchdog test)

---

## Phase 4C: QSPI Flash Storage — DONE
**Complexity: M | Dependency: None (independent of io_timer) | Dev Board**
**Status: COMPLETE** — 2026-02-10 (LittleFS mounted, persistence verified)
**Full details:** See [QSPI_FILESYSTEM.md](QSPI_FILESYSTEM.md)

### What Was Implemented

The SAMV71-XULT has an onboard **S25FL116K** (Spansion, JEDEC 01 40 15, **2 MB**) — not
SST26VF064B as documented in the Atmel user guide. The flash is fully operational with LittleFS.

**Architecture:**
```
sam_qspi_spi_initialize(0)   → struct spi_dev_s*     (QSPI SPI compat mode)
w25_initialize(spi)           → struct mtd_dev_s*     (W25/S25FL1xx MTD driver)
register_mtddriver("/dev/mtdqspi")                     (NuttX VFS)
mount("/dev/mtdqspi", "/mnt/qspi", "littlefs", 0, "autoformat")
```

**defconfig:**
`CONFIG_SAMV7_QSPI=y`, `CONFIG_SAMV7_QSPI_SPI_MODE=y`, `CONFIG_MTD_W25=y`,
`CONFIG_W25_SPIFREQUENCY=1000000`, `CONFIG_FS_LITTLEFS=y`

**Files created/modified:**
- `src/qspi.c` — Board-level QSPI init with JEDEC probe, W25 MTD, `/dev/mtdqspi` registration
- `src/init.c` — Calls `board_qspi_flash_init()`, mounts LittleFS at `/mnt/qspi` with autoformat
- `src/board_config.h` — `BOARD_HAS_QSPI_FLASH`
- `src/CMakeLists.txt` — Added `qspi.c`

**NuttX driver fixes (4 bugs in `sam_qspi_spi.c`):**
1. Swapped `qspi_putreg()` arguments at WPCR (caused AHB bus hang)
2. CSMODE left at NRELOAD (CS pulsed between every byte)
3. Missing LASTXFER logic in exchange/select (CS never released between transactions)
4. CS deassertion: dummy byte corrupts WREN, CSS polarity inverted; fix = QSPI disable/re-enable

**Verified:** File I/O (create, read, write, mkdir, nested files), large file (32 KB dd),
persistence across reboot, LittleFS autoformat on first boot.

**Pin conflict with Phase 5:** PA12/PA14 shared with PWM1 — custom PCB uses PA30/PA31 instead.

### Future (Phase 4C-2: MTD Partitions — deferred)

Partition the 2 MB flash into dedicated MTD regions for PX4 parameter/dataman storage:

```
/dev/mtdqspi (2 MB)
+-- Partition 0: /fs/mtd_params     (128 KB)  - System parameters
+-- Partition 1: /fs/mtd_caldata    ( 64 KB)  - Factory calibration
+-- Partition 2: /fs/mtd_waypoints  (512 KB)  - Mission/geofence/rally
+-- Partition 3: /fs/mtd_dataman    (  1 MB)  - Dataman general storage
+-- Reserved                        (~320 KB)  - Future use
```

Requires `board_get_manifest()` and `FLASH_BASED_PARAMS` / `FLASH_BASED_DATAMAN` in board_config.h.
Flight logs stay on SD card (continuous high-throughput writes, GB capacity needed).

---

## Phase 5: 8-Channel PWM
**Complexity: M | Dependency: Phase 1 | Custom PCB Required**

### Prerequisites from Phase 1

Phase 1 must have already fixed:
- `MAX_IO_TIMERS` derived from `BOARD_NUM_IO_TIMERS` (not hardcoded to 4)
- `io_timers_channel_mapping_element_t` with proper struct (not bitmask)

### Add `PWMCPeripheral::C` and `::D`

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`

Current enum only has A/B. PWM1 pins use Peripheral C (and some use A), so the enum needs extension:
```c
enum class PWMCPeripheral {
    A = 0,  /* GPIO_PERIPHA */
    B = 1,  /* GPIO_PERIPHB */
    C = 2,  /* GPIO_PERIPHC */
    D = 3,  /* GPIO_PERIPHD */
};
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

Update `initIOPWMChannel()` GPIO mode encoding to handle C/D:
```c
uint32_t gpio_mode;
switch (periph) {
    case PWMCPeripheral::A: gpio_mode = (3 << 21); break;  // GPIO_PERIPHA
    case PWMCPeripheral::B: gpio_mode = (4 << 21); break;  // GPIO_PERIPHB
    case PWMCPeripheral::C: gpio_mode = (5 << 21); break;  // GPIO_PERIPHC
    case PWMCPeripheral::D: gpio_mode = (6 << 21); break;  // GPIO_PERIPHD
}
```

### Add PWM1 Module
**Files:** `timer_config.cpp`, `board_config.h`

- Update `BOARD_NUM_IO_TIMERS = 2` in board_config.h
- Expand `io_timers[]` to include `initIOPWMTimer(PWM::PWM1)`
- Add 4 more channels to `timer_io_channels[]` (PWM1 CH0-CH3)
- Update `DIRECT_PWM_OUTPUT_CHANNELS = 8`
- Enable `CONFIG_SAMV7_PWM1*` in defconfig

PWM1 pin options (custom PCB selects):
- PWM1_H0: ~~PA12 (Periph C)~~ **CONFLICT: QSPI IO1** → use PA30 (Periph A)
- PWM1_H1: ~~PA14 (Periph C)~~ **CONFLICT: QSPI SCK** → use PA31 (Periph A)
- PWM1_H2: PA1 (Periph A) or PD0 (Periph C)
- PWM1_H3: PA8 (Periph A) or PD2 (Periph C)

### Verify
- All 8 channels produce PWM
- PWM0 and PWM1 can run at different rates
- DShot works on both modules

---

## Phase 6: Extended ADC & Power Monitoring
**Complexity: M | Dependency: Custom PCB**

### Expand ADC
**Files:** `board_config.h`, `adc.cpp`
- Add channels: 2nd brick V/I, 5V rail, 3.3V rail, RSSI
- Extend `adc.cpp` AFEC initialization for new channels

### Power Rail Control
**File:** `board_config.h`
```c
#define GPIO_VDD_3V3_SENSORS_EN  (GPIO_OUTPUT | ...)
#define GPIO_VDD_5V_PERIPH_EN    (GPIO_OUTPUT | ...)
#define VDD_5V_PERIPH_EN(on)     px4_arch_gpiowrite(GPIO_VDD_5V_PERIPH_EN, !(on))
```

**File:** `init.c`
- Implement `board_peripheral_reset(ms)`: power-cycle peripheral rails

---

## Phase 7: HW Versioning & Final Production
**Complexity: M | Dependency: Phases 5-6, Custom PCB**

- ADC resistor-ladder for HW version/revision detection
- EEPROM/FRAM MTD driver for parameter redundancy (dual storage)
- USB VBUS detection GPIO
- Peripheral reset circuit integration
- Full `rc.board_sensors` with hw-version-based sensor tree selection

---

## Dependency Graph

```
Phase 0 (Safety + Cleanup)  ✅ DONE (2026-02-08)
    │              │
    v              v
Phase 1            P4C              ✅ DONE (2026-02-08, 2026-02-10)
(API Convergence)  QSPI Flash
   ╱    │    ╲
  v     v     v
P2     P3    P4                    ← NEXT: these are now unblocked
DShot  Capture  Harden
                                   ─── Dev Board boundary ───
Phase 5 (8-ch PWM)      ← Custom PCB (note: PA12/PA14 conflict with QSPI)
    │
Phase 6 (ADC/Power)     ← Custom PCB
    │
Phase 7 (Versioning)    ← Custom PCB
```

Phases 0, 1, and 4C are complete. Phases 2 (DShot), 3 (Input Capture), and 4A/4B (Hardening) are
now unblocked and can proceed in parallel.

## Effort Summary

| Phase | Scope | Effort | Status |
|-------|-------|--------|--------|
| 0 | Safety closure + cleanup + deprecation | S | ✅ Done (2026-02-08) |
| 1 | API convergence + allocation + migration | L-XL | ✅ Done (2026-02-08) |
| 2 | DShot Output (PWMC+XDMAC) | XL | Pending (unblocked) |
| 3 | Input Capture (TC-based) + RC integration | M | Pending (unblocked) |
| 4 | Production Hardening (fault + watchdog) | M | Pending (unblocked) |
| 4C | QSPI Flash + LittleFS filesystem | M | ✅ Done (2026-02-10) |
| 5 | 8-Channel PWM (PCB) | M | Pending (custom PCB) |
| 6 | ADC/Power (PCB) | M | Pending (custom PCB) |
| 7 | HW Versioning (PCB) | M | Pending (custom PCB) |

### Remaining Dev Board Work (Phases 2-4)

| Phase | Estimated Calendar |
|-------|-------------------|
| 2 — DShot Output | 3-4 weeks |
| 3 — Input Capture | 1-2 weeks |
| 4A/4B — Hardening | 1 week |
| **Remaining dev board** | **~5-7 weeks** |
| **+ Custom PCB (P5-P7)** | **+ 3-4 weeks + fab** |

Note: QSPI implementation took ~2 days but required fixing 4 upstream NuttX bugs in
`sam_qspi_spi.c` (the SPI-compatibility mode driver had never been tested on real hardware).
The actual flash is S25FL116K (2 MB), not SST26VF064B (8 MB) as originally documented.

## Key Reference Files

| Purpose | File |
|---------|------|
| SAMV7 io_timer API | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h` |
| SAMV7 PWMC driver | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c` |
| SAMV7 HW description | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h` |
| SAMV7 GPIO/PWM enums | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h` |
| STM32 io_timer (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c` |
| STM32 io_timer.h (reference) | `platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/io_timer.h` |
| STM32 DShot (reference) | `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c` |
| STM32 input_capture (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/input_capture.c` |
| Board config | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| Timer config | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| Board init | `boards/microchip/samv71-xult-clickboards/src/init.c` |
| Board defaults | `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` |
| NuttX XDMAC API | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.h` |
| NuttX PWMC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_pwm.h` |
| NuttX TC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_tc.h` |
| NuttX QSPI SPI mode | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_qspi_spi.c` (4 bugs fixed) |
| NuttX W25 MTD driver | `platforms/nuttx/NuttX/nuttx/drivers/mtd/w25.c` |
| Board QSPI init | `boards/microchip/samv71-xult-clickboards/src/qspi.c` |
| QSPI filesystem doc | `boards/microchip/samv71-xult-clickboards/QSPI_FILESYSTEM.md` |
| Legacy (deprecated) | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c` |
| Legacy (deprecated) | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_stub.c` |
| Stale (superseded) | `boards/microchip/samv71-xult-clickboards/PRODUCTION_READINESS.md` |

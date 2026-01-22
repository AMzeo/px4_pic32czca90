# SAMV71-XULT Production Gap Analysis

**Created:** January 2026
**Revised:** January 2026 (Combined evidence-based master document)
**Baseline:** PX4 FMU-V6X (Pixhawk 6X)
**Target:** SAMV71-XULT Clickboards

---

## Executive Summary

This document provides an evidence-based gap analysis comparing the SAMV71-XULT PX4 implementation against the production-grade FMU-V6X baseline. All findings reference specific files in the repository.

### Readiness Tiers

| Tier | Definition | Status |
|------|------------|--------|
| **1. Bench Flight-Ready (MVP)** | Safe indoor hover/bench testing | Blocked |
| **2. Field Flight-Ready** | Safe outdoor flight operations | Not Started |
| **3. V6X Feature Parity** | Pixhawk-class feature set | Optional |

### Current Blockers (MVP)

| Blocker | Evidence | Impact |
|---------|----------|--------|
| ADC/Battery Monitoring | `CONFIG_DRIVERS_ADC_BOARD_ADC` disabled in `default.px4board` | Cannot monitor battery |
| Watchdog Timer | No `CONFIG_WATCHDOG` in `defconfig` | No crash recovery |
| 4-Channel PWM | Only 3 TC channels in `timer_config.cpp` | Cannot fly quadcopter |

### Recently Resolved

| Issue | Resolution | Evidence |
|-------|------------|----------|
| SD Card TX DMA | **FIXED** (Fix #46) | PA26 pin conflict resolved; `#undef HSCMI_NOTXDMA` in `sam_hsmci.c:132` |

---

## Part 1: Evidence Snapshot

### 1.1 Source Files Analyzed

**FMU-V6X (Baseline):**
```
boards/px4/fmu-v6x/
├── default.px4board              # PX4 module config
├── nuttx-config/nsh/defconfig    # NuttX config
├── src/board_config.h            # Hardware definitions
├── src/timer_config.cpp          # PWM timer mapping
├── init/rc.board_sensors         # Sensor startup
└── bootloader.px4board           # Bootloader config
```

**SAMV71-XULT (Target):**
```
boards/microchip/samv71-xult-clickboards/
├── default.px4board              # PX4 module config
├── nuttx-config/nsh/defconfig    # NuttX config
├── src/board_config.h            # Hardware definitions
├── src/timer_config.cpp          # PWM timer mapping (TC-based, 3ch)
├── init/rc.board_sensors         # Sensor startup
└── [NO bootloader.px4board]      # Missing
```

### 1.2 Current SAMV71 Configuration State

| Subsystem | Config Status | Evidence |
|-----------|---------------|----------|
| **PWM Output** | 3 channels (TC-based) | `timer_config.cpp`: Timer1,3,4 on PA15,PC23,PC26 |
| **ADC** | Disabled | `default.px4board`: `# CONFIG_DRIVERS_ADC_BOARD_ADC is not set` |
| **Battery Module** | Enabled (placeholder ADC) | `default.px4board`: `CONFIG_MODULES_BATTERY_STATUS=y`; `board_config.h`: placeholder channels 0/1 |
| **Watchdog** | Disabled | `defconfig`: No `CONFIG_WATCHDOG` |
| **SD Card** | ✅ Working | `defconfig`: `CONFIG_SAMV7_HSMCI0=y`, `CONFIG_SAMV7_HSMCI_DMA=y`; TX DMA enabled (Fix #46) |
| **Logger** | Enabled | `default.px4board`: `CONFIG_MODULES_LOGGER=y` |
| **CAN (MCAN)** | Disabled | `defconfig`: No `CONFIG_SAMV7_MCAN0/1` |
| **DShot** | Disabled | `default.px4board`: `# CONFIG_DRIVERS_DSHOT is not set` |
| **Safety Button** | Disabled | `default.px4board`: Not present |
| **Tone Alarm** | Disabled | `default.px4board`: Not present |

---

## Part 2: Detailed Feature Comparison

### 2.1 Motor Output & Control

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| PWM Channels | 8+ (FMU + IO) | 3 | `timer_config.cpp` | **CRITICAL** |
| DShot | Yes | No | `default.px4board` | HIGH |
| PWM_OUT driver | Yes | Yes | Both have `CONFIG_DRIVERS_PWM_OUT=y` | Done |
| PX4IO | Yes | No | V6X has dedicated IO processor | N/A |

**Gap:** Need 4 channels minimum for quadcopter. Current TC-based implementation limited to 3 due to PA26/SD card conflict.

**Plan:** Migrate to PWMC (see `PWMC_DSHOT_IMPLEMENTATION_PLAN.md`)
- Phase 1: 4-channel PWMC PWM
- Phase 2: DShot with XDMAC

### 2.2 Power Management & ADC

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| Board ADC | Yes | **No** | V6X: `rc.board_sensors` starts it | **CRITICAL** |
| Battery Status | Yes | Enabled but no data | Both have module enabled | **CRITICAL** |
| INA226/228/238 | Yes | No | V6X `default.px4board` | HIGH |
| Voltage Scaling | Configured | Placeholder | `board_config.h` ADC defines | **CRITICAL** |

**Gap:** `battery_status` topic has no real data. ADC driver not enabled, channels not mapped.

**Evidence from `boards/microchip/samv71-xult-clickboards/src/board_config.h` (lines 112-115):**
```c
// Current state - placeholder channel numbers, no ADC driver enabled
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define BOARD_NUMBER_BRICKS          1
#define BOARD_ADC_BRICK_VALID        1  /* Placeholder - all bricks considered valid */
```

**Note:** Channels are mapped but `CONFIG_DRIVERS_ADC_BOARD_ADC` is disabled in `default.px4board`, so no actual ADC reads occur.

**Plan Options:**
1. **AFEC ADC Path:** Enable NuttX `CONFIG_SAMV7_AFEC0`, map VBAT/IBAT pins, implement `board_adc.c`
2. **Digital Monitor Path:** Add INA226 on I2C if hardware supports it

**Acceptance Criteria:**
- `battery_status` publishes voltage/current at idle
- Arming blocked on undervoltage

### 2.3 Safety & Watchdog

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| Watchdog | Yes | **No** | V6X: `CONFIG_WATCHDOG=y` in defconfig | **CRITICAL** |
| Safety Button | Yes | No | V6X: `CONFIG_DRIVERS_SAFETY_BUTTON=y` | HIGH |
| Tone Alarm | Yes | No | V6X: `CONFIG_DRIVERS_TONE_ALARM=y` | MEDIUM |
| Hardfault Log | Yes | Partial | V6X: `CONFIG_SYSTEMCMDS_HARDFAULT_LOG=y` | MEDIUM |
| GPIO Control | Yes | No | V6X: `CONFIG_SYSTEMCMDS_GPIO=y` | LOW |

**Gap:** No automatic recovery from software hangs.

**Plan:**
```diff
# Add to nuttx-config/nsh/defconfig
+CONFIG_WATCHDOG=y
+CONFIG_SAMV7_WDT=y          # Confirm exact symbol via: make menuconfig (search WDT)
```

**Note:** `CONFIG_SAMV7_WDT` symbol name needs verification via NuttX menuconfig or `arch/arm/src/samv7/Kconfig`. V6X only confirms `CONFIG_WATCHDOG=y` at NuttX level.

**Acceptance Criteria:**
- Induced scheduler deadlock causes reset within bounded time (e.g., 5 seconds)

### 2.4 Storage & Logging

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| SD Card | SDMMC | HSMCI | Both have SD support | Done |
| SD Write | Reliable | ✅ **Fixed** | TX DMA enabled after Fix #46 | Done |
| Logger | Yes | Yes | Both enabled | Done |
| FRAM | Yes | No | V6X uses for params | LOW |
| MTD/PROGMEM | Yes | Partial | `CONFIG_SAMV7_PROGMEM=y` | MEDIUM |

**Status:** ✅ SD card writes now working with TX DMA enabled.

**Root Cause (Fix #46):** PA26 pin conflict between PWM Timer2 and HSMCI DA2 (SD card data line 2). Resolved by removing Timer2 from PWM config (now 3 TC channels).

**Evidence from `sam_hsmci.c` line 132:**
```c
#undef  HSCMI_NOTXDMA              /* TX DMA enabled - testing with FIX #46 (PA26 pin conflict resolved) */
```

**Evidence from `nuttx-config/nsh/defconfig` (lines 129, 195-198):**
```
CONFIG_SDIO_DMA=y
CONFIG_SAMV7_HSMCI0=y
CONFIG_SAMV7_HSMCI_DMA=y
CONFIG_SAMV7_HSMCI_RDPROOF=y
CONFIG_SAMV7_HSMCI_WRPROOF=y
```

**Verification:** All writes now verify successfully (see `SAMV7_HSMCI_DMA_FIX.md` section "Verification Results").

### 2.5 Communication Interfaces

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| USB CDC/ACM | Yes | Yes | Both working | Done |
| MAVLink | Yes | Yes | Both working | Done |
| CAN/DroneCAN | Yes | **No** | V6X: `CONFIG_DRIVERS_UAVCAN=y` | HIGH |
| GPS UART | 2 ports | 1 port | `board_config.h` | MEDIUM |
| Telemetry | 3 ports | 1 port | `board_config.h` | MEDIUM |
| RC Input | Multiple | UART only | `CONFIG_DRIVERS_RC_INPUT=y` | Done |

**Gap:** No CAN bus support despite SAMV71 having MCAN peripheral.

**Plan:**
```diff
# Add to nuttx-config/nsh/defconfig
+CONFIG_SAMV7_MCAN0=y
+CONFIG_CAN=y

# Add to default.px4board
+CONFIG_DRIVERS_UAVCAN=y
```

### 2.6 Sensors

| Feature | FMU-V6X | SAMV71 | Status |
|---------|---------|--------|--------|
| ICM-20689 | Yes | Yes | Working |
| ICM-45686 | Yes | Driver only | Needs GPIO config |
| BMI088 | Yes | Yes | Working |
| BMP388 | Yes | Yes | Working |
| DPS310 | Yes | Yes | Working |
| AK09916 | Yes | Yes | Working |
| BMM150 | Yes | Yes | Working |

**Status:** Core sensors working. ICM-45686 needs interrupt GPIO configuration.

### 2.7 Bootloader & Updates

| Feature | FMU-V6X | SAMV71 | Evidence | Priority |
|---------|---------|--------|----------|----------|
| Bootloader | Yes | **No** | V6X: `bootloader.px4board` exists | HIGH |
| USB DFU | Yes | No | - | HIGH |
| OTA Update | Yes | No | - | MEDIUM |

**Gap:** No field firmware update capability. Currently JTAG-only.

**Plan:** Implement USB DFU bootloader or accept JTAG-only for prototype phase.

---

## Part 3: Configuration Delta

### 3.1 Enabled on V6X but NOT on SAMV71

**Critical for Flight:**
```
CONFIG_DRIVERS_ADC_BOARD_ADC=y      # Battery monitoring
CONFIG_DRIVERS_DSHOT=y              # Digital ESC protocol
CONFIG_WATCHDOG=y                   # Crash recovery (NuttX level)
```

**Safety & UI:**
```
CONFIG_DRIVERS_SAFETY_BUTTON=y
CONFIG_DRIVERS_TONE_ALARM=y
CONFIG_SYSTEMCMDS_GPIO=y
CONFIG_SYSTEMCMDS_HARDFAULT_LOG=y
```

**Communication:**
```
CONFIG_DRIVERS_UAVCAN=y
CONFIG_DRIVERS_PX4IO=y              # N/A - requires IO processor
```

**Advanced:**
```
CONFIG_DRIVERS_PWM_INPUT=y
CONFIG_DRIVERS_CAMERA_TRIGGER=y
CONFIG_DRIVERS_HEATER=y
CONFIG_MODULES_TEMPERATURE_COMPENSATION=y
```

### 3.2 Enabled on SAMV71 but NOT on V6X

**Board-Specific:**
```
CONFIG_DRIVERS_RC_INPUT=y           # V6X uses PX4IO for RC
CONFIG_MODULES_SIMULATION_PWM_OUT_SIM=y  # HITL support
CONFIG_BOARD_PCK6_TEST=y            # Timer test command
```

---

## Part 4: Phased Implementation Plan

### Phase 0: Decision Points (1-2 days)

Before implementation, decide:

1. **Battery Monitoring Path:**
   - [ ] Option A: AFEC ADC (requires board ADC driver)
   - [ ] Option B: INA226 on I2C (if hardware available)

2. **Storage Strategy:**
   - [x] ~~Option A: Fix HSMCI DMA TX~~ ✅ **DONE** (Fix #46 - PA26 pin conflict resolved)
   - [ ] Option B: RAM logging + periodic flush (not needed)
   - [ ] Option C: QSPI flash (if available) (optional enhancement)
   - [ ] ~~Option D: Accept limited logging for MVP~~ (not needed)

3. **Update Strategy:**
   - [ ] Option A: JTAG-only (prototype phase)
   - [ ] Option B: Implement USB DFU bootloader

4. **Safety Hardware:**
   - [ ] Identify GPIO pins for safety button
   - [ ] Identify PWM pin for buzzer
   - [ ] Confirm MCAN transceiver availability

### Phase 1: Bench Flight-Ready MVP (2-3 weeks)

| Task | Files to Modify | Acceptance Criteria |
|------|-----------------|---------------------|
| **ADC Battery Monitor** | `defconfig`, `board_config.h`, new `board_adc.c` | `battery_status` shows real voltage |
| **Watchdog Timer** | `defconfig`, `init.c` | Deadlock causes reset in <5s |
| **4-Channel PWMC** | `defconfig`, `timer_config.cpp`, new `io_timer_pwmc.c` | `pwm_out status` shows 4 channels |
| ~~**Storage Validation**~~ | ~~Test existing~~ | ✅ **DONE** - TX DMA working (Fix #46) |

**Milestone:** Safe to attempt indoor hover test with battery monitoring.

### Phase 2: Field Flight-Ready (2-4 weeks)

| Task | Files to Modify | Acceptance Criteria |
|------|-----------------|---------------------|
| **Safety Button** | `default.px4board`, GPIO config | Button toggles armed state |
| **Tone Alarm** | `default.px4board`, PWM config | Arming beep plays |
| **CAN/DroneCAN** | `defconfig`, `default.px4board` | CAN ESC responds |
| **Bootloader** | New `bootloader/` directory | USB DFU update works |

**Milestone:** Safe for outdoor flight with full safety features.

### Phase 3: DShot & Extras (Ongoing)

| Task | Dependency | Notes |
|------|------------|-------|
| **DShot Protocol** | Phase 1 PWMC complete | See `PWMC_DSHOT_IMPLEMENTATION_PLAN.md` |
| **Bidirectional DShot** | DShot complete | ESC telemetry (RPM) |
| **Additional Sensors** | As needed | ICM-45686 GPIO, external mag |
| **Fixed-Wing Support** | Core complete | Enable FW modules |

---

## Part 5: Implementation Details

### 5.1 ADC Implementation Path

**NuttX Configuration:**
```diff
# nuttx-config/nsh/defconfig
+CONFIG_SAMV7_AFEC0=y
+CONFIG_SAMV7_AFEC0_CHANNEL0=y    # VBAT
+CONFIG_SAMV7_AFEC0_CHANNEL1=y    # IBAT
+CONFIG_ADC=y
```

**Board Configuration:**
```c
// board_config.h
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define ADC_BATTERY_VOLTAGE_SCALE    (3.3f * 11.0f / 4096.0f)  // Adjust for divider
#define ADC_BATTERY_CURRENT_SCALE    (3.3f / 4096.0f / 0.01f)  // Adjust for shunt
```

**PX4 Configuration:**
```diff
# default.px4board
+CONFIG_DRIVERS_ADC_BOARD_ADC=y
```

### 5.2 Watchdog Implementation Path

**NuttX Configuration:**
```diff
# nuttx-config/nsh/defconfig
+CONFIG_WATCHDOG=y
+CONFIG_SAMV7_WDT=y              # Verify via: make menuconfig → Device Drivers → Watchdog
# Timeout symbol TBD - may be CONFIG_SAMV7_WDT_TIMEOUT or configured at runtime
```

**Note:** Exact SAMV7 watchdog Kconfig symbols need verification. Use `make menuconfig` and search for "WDT" or "watchdog" to find the correct driver symbol for this NuttX tree.

**Board Init:**
```c
// init.c - add to board initialization
#ifdef CONFIG_WATCHDOG
    int fd = open("/dev/watchdog0", O_RDONLY);
    if (fd >= 0) {
        ioctl(fd, WDIOC_START, 0);
        close(fd);
    }
#endif
```

### 5.3 4-Channel PWMC Path

See `PWMC_DSHOT_IMPLEMENTATION_PLAN.md` for complete implementation.

**Summary (Phase 1 - Standard PWM):**
- Motors 1-3: PWM0 (CH3, CH1, CH2) on PA7, PA2, PC19
- Motor 4: PWM1 (CH1) on PA14
- Uses existing `timer_io_channels_t` struct fields
- Standard 400 Hz PWM, no DMA required for basic operation

**Note:** DMA is only required for Phase 3 (DShot protocol). See `PWMC_DSHOT_IMPLEMENTATION_PLAN.md` for DMA details.

### 5.4 CAN Implementation Path

**NuttX Configuration:**
```diff
# nuttx-config/nsh/defconfig
+CONFIG_SAMV7_MCAN0=y
+CONFIG_SAMV7_MCAN0_BITRATE=1000000
+CONFIG_CAN=y
```

**Board Configuration (EXAMPLE ONLY - verify before use):**
```c
// board_config.h
// WARNING: PB2/PB3 are example pins only. Before implementing:
// 1. Verify MCAN0 pin routing on SAMV71-XULT schematic
// 2. Confirm peripheral function in: platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/samv71_pinmap.h
// 3. Check for conflicts with other SAMV71-XULT peripherals
#define GPIO_CAN0_TX  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOB | GPIO_PIN2)  /* VERIFY */
#define GPIO_CAN0_RX  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOB | GPIO_PIN3)  /* VERIFY */
```

**PX4 Configuration:**
```diff
# default.px4board
+CONFIG_DRIVERS_UAVCAN=y
```

---

## Part 6: Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| ~~HSMCI DMA TX unfixable~~ | ~~Medium~~ | ~~High~~ | ✅ **RESOLVED** - Fix #46 |
| ADC calibration issues | Low | Medium | Use digital power monitor |
| PWMC timing problems | Low | Medium | Keep TC fallback, use scope |
| CAN signal integrity | Low | Medium | Proper termination |
| Memory constraints | Low | Low | 36% flash headroom |

---

## Part 7: Quick Reference

### Working Now
- USB MAVLink communication
- Core flight control (MC)
- EKF2 state estimation
- SPI sensors (ICM20689, BMP388, BMI088)
- I2C sensors (AK09916, BMM150, DPS310)
- 3-channel PWM output
- GPS via UART
- Parameter storage
- RC input (UART)
- **SD card writes** (TX DMA enabled - Fix #46)
- **Logger** (can be enabled: `SDLOG_MODE=0`)

### MVP Blockers
- **ADC battery monitoring** - Driver disabled (`default.px4board`)
- **Watchdog timer** - Not enabled (`defconfig`)
- **4th PWM channel** - Only 3 TC channels (`timer_config.cpp`)

### Recently Fixed
- ✅ **SD write reliability** - TX DMA enabled (Fix #46 resolved PA26 pin conflict)

### Post-MVP Gaps
- DShot protocol
- CAN/DroneCAN
- Safety button/LED
- Bootloader
- Tone alarm

---

## Appendix A: File Reference

| Purpose | FMU-V6X File | SAMV71 File |
|---------|--------------|-------------|
| PX4 Modules | `default.px4board` | `default.px4board` |
| NuttX Config | `nuttx-config/nsh/defconfig` | `nuttx-config/nsh/defconfig` |
| Hardware Defs | `src/board_config.h` | `src/board_config.h` |
| Timer Config | `src/timer_config.cpp` | `src/timer_config.cpp` |
| Sensor Startup | `init/rc.board_sensors` | `init/rc.board_sensors` |
| Bootloader | `bootloader.px4board` | *Missing* |

## Appendix B: Related Documents

- `PWMC_DSHOT_IMPLEMENTATION_PLAN.md` - 4-channel PWM and DShot implementation
- `PWMC_IMPLEMENTATION_PLAN.md` - Original PWMC pin analysis
- `PWM_VERIFICATION_TEST.md` - Oscilloscope test procedures
- `SPI_SENSORS.md` - SPI sensor configuration

---

**Document Status:** Master (Combined V1 + V2)
**Next Action:** Complete Phase 0 decision points, then begin Phase 1 MVP

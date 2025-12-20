# SAMV71 FMUv6 Parity – Execution Plan & Acceptance Criteria

**Companion document:** `docs/SAMV71_FMUV6_PARITY_ROADMAP.md`

---

## Overview

This document tracks concrete acceptance criteria for each workstream. Each item has a clear pass/fail test.

---

## Phase 1: Foundation

### WS-1: FMUv6 Parity Matrix
**Owner:** TBD
**Status:** ⬜ Not Started

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | Complete feature matrix document exists | Document review | ⬜ |
| 2 | All FMUv6 features listed with SAMV71 status | Document review | ⬜ |
| 3 | Priority assigned to each gap | Document review | ⬜ |

---

### WS-2: Hardware Design
**Owner:** TBD
**Status:** 🔄 In Progress (using dev kit)

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | Pinmux document with no conflicts | Schematic review | ⬜ |
| 2 | All sensor CS lines on dedicated pins | Schematic review | ⬜ |
| 3 | All DRDY lines on IRQ-capable GPIOs | Schematic review | ⬜ |
| 4 | HSMCI pins free of TC conflicts | Schematic review | ✅ (devkit) |
| 5 | CAN transceiver connections verified | Schematic review | ⬜ |

---

### WS-3: GPIO Interrupt Semantics (BLOCKING)
**Owner:** TBD
**Status:** ⬜ Not Started
**Priority:** 🔴 CRITICAL

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | `px4_arch_gpiosetevent()` attaches callback internally | Code review | ⬜ |
| 2 | Handler receives `arg` parameter correctly | On-target selftest: toggle PD28 (EXT1 DRDY), verify with scope/LA | ⬜ |
| 3 | ICM20689 runs at 8kHz with DRDY interrupt | `uorb top` or `icm20689 status` | ⬜ |
| 4 | Multiple DRDY interrupts work simultaneously | Multi-sensor test | ⬜ |
| 5 | No polling required for sensor reads | Code review + timing | ⬜ |
| 6 | Card detect interrupt works for SD | Hot-plug test | ⬜ |

**Files:**
- `boards/microchip/samv71-xult-clickboards/src/sam_gpiosetevent.c`
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/micro_hal.h`

---

## Phase 2: Core Flight

### WS-4: Cache/DMA Coherency
**Owner:** TBD
**Status:** 🔄 Partial

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | `board_dma_alloc()` used for all DMA buffers | Code audit | ⬜ |
| 2 | Alignment requirements documented | Doc review | ⬜ |
| 3 | SPI + SD + USB concurrent test passes | Stress test | ⬜ |
| 4 | No DMA-related hard faults in 1-hour test | Stress test | ⬜ |
| 5 | Cache operations documented per peripheral | Doc review | ⬜ |

**Stress Test Procedure:**
```bash
# Run concurrently:
# 1. High-rate sensor reads (icm20689)
# 2. SD logging at flight rates
# 3. MAVLink stream over USB
# Duration: 1 hour minimum
```

---

### WS-5: IO Timer Feature Complete
**Owner:** TBD
**Status:** 🔄 PWM-out only

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | PWM output on 3 channels (devkit) / 4+ (production) | Oscilloscope | ⬜ |
| 2 | PWM frequency configurable 50-400Hz | Oscilloscope | ⬜ |
| 3 | Mixer outputs drive motors correctly | Motor spin test | ⬜ |
| 4 | Disarm sets outputs to safe state | Oscilloscope | ⬜ |
| 5 | Failsafe outputs configured | Failsafe trigger test | ⬜ |
| 6 | Input capture works for RC | RC receiver test | ⬜ |
| 7 | OneShot125 protocol working | Oscilloscope | ⬜ |

**Timer Allocation:**
| Timer | Channel | Function | Pin |
|-------|---------|----------|-----|
| TC0 CH0 | HRT | Reserved | - |
| TC0 CH1 | PWM1 | Motor 1 | PA15 |
| TC1 CH0 | PWM2 | Motor 2 | PC23 |
| TC1 CH1 | PWM3 | Motor 3 | PC26 |
| TC1 CH2 | RC Input | Capture | PC29 |

---

### WS-6: Motor Protocols
**Owner:** TBD
**Status:** 🔄 PWM Only

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | PWM 50Hz output verified | Oscilloscope | ⬜ |
| 2 | PWM 400Hz output verified | Oscilloscope | ⬜ |
| 3 | OneShot125 waveform correct | Oscilloscope | ⬜ |
| 4 | Motor spins with arming sequence | Motor test | ⬜ |
| 5 | Motor stops on disarm | Motor test | ⬜ |
| 6 | DShot150 working (stretch goal) | Oscilloscope | ⬜ |

---

## Phase 3: Production Quality

### WS-7: Storage & Logging
**Owner:** TBD
**Status:** 🔄 Working (manual mount)

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | SD mounts automatically on boot | Boot test | ⬜ |
| 2 | Card detect interrupt triggers remount | Hot-plug test | ⬜ |
| 3 | ULog files written correctly | Log analysis | ✅ |
| 4 | 500Hz IMU logging sustained | Long-duration test | ⬜ |
| 5 | No buffer overruns in 1-hour test | Log analysis | ⬜ |
| 6 | Clean unmount on disarm | File integrity check | ⬜ |
| 7 | 4-bit HSMCI mode working | Performance test | ✅ |

---

### WS-8: USB Production Ready
**Owner:** TBD
**Status:** ⬜ Stubbed

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | VBUS detection GPIO configured | Code review | ⬜ |
| 2 | VBUS state reflects actual connection | GPIO test | ⬜ |
| 3 | CDCACM starts on USB connect | Connection test | ⬜ |
| 4 | MAVLink stream stable | QGC test | ⬜ |
| 5 | Hot-plug doesn't crash system | Rapid plug test | ⬜ |
| 6 | USB + SD concurrent stable | Stress test | ⬜ |

**Hot-Plug Test:**
```
1. Connect USB while logging
2. Disconnect during MAVLink stream
3. Rapid connect/disconnect (10 cycles)
4. No crashes or hangs
```

---

### WS-9: Sensor Stack Parity
**Owner:** TBD
**Status:** 🔄 ICM20689 working

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | ICM20689 publishing accel/gyro | `listener sensor_accel` | ✅ |
| 2 | ICM20689 at target rate (8kHz) | Rate measurement | ⬜ |
| 3 | BMP388 publishing pressure | `listener sensor_baro` | ⬜ |
| 4 | AK09915 publishing mag data | `listener sensor_mag` | ⬜ |
| 5 | All sensors concurrent operation | Multi-sensor test | ⬜ |
| 6 | SPI bus locking correct | Concurrent access test | ⬜ |
| 7 | Bus reset recovers from errors | Error injection test | ⬜ |

**Current Sensor Status:**
| Sensor | Interface | Status | Notes |
|--------|-----------|--------|-------|
| ICM20689 | SPI | ✅ Working | EXT1, -s command |
| BMP388 | SPI | ⬜ Not Working | Driver investigation needed |
| AK09915 | I2C | ⬜ Configured | Not tested |

---

### WS-10: CAN/UAVCAN
**Owner:** TBD
**Status:** ⬜ Not Started

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | MCAN peripheral initialized | Register check | ⬜ |
| 2 | CAN frames transmitted | Bus analyzer | ⬜ |
| 3 | CAN frames received | Bus analyzer | ⬜ |
| 4 | UAVCAN node allocation works | UAVCAN GUI | ⬜ |
| 5 | Parameter read/write over CAN | UAVCAN GUI | ⬜ |
| 6 | GPS data received over UAVCAN | `listener sensor_gps` | ⬜ |

---

## Phase 4: Robustness

### WS-11: System Robustness
**Owner:** TBD
**Status:** ⬜ Mostly Not Started

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | Watchdog resets on hang | Infinite loop test | ⬜ |
| 2 | Watchdog timeout configurable | Config test | ⬜ |
| 3 | True silicon UUID read | `ver all` output | ⬜ |
| 4 | Reset-to-bootloader survives power cycle | Power cycle test | ⬜ |
| 5 | Hardfault dumps to SD | Crash test | ⬜ |
| 6 | Hardfault dumps to console | Crash test | ⬜ |
| 7 | GPBR registers used for reset mode | Code review | ⬜ |

---

### WS-12: Production Boot/Update
**Owner:** TBD
**Status:** ⬜ Not Started

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | Bootloader strategy documented | Doc review | ⬜ |
| 2 | Memory layout defined | Doc review | ⬜ |
| 3 | Firmware update via USB works | Update test | ⬜ |
| 4 | Failed update doesn't brick | Power-loss test | ⬜ |
| 5 | Boot time under 3 seconds | Timing test | ⬜ |

---

## Phase 5: Validation

### WS-13: Test Strategy
**Owner:** TBD
**Status:** ⬜ Not Started

| # | Acceptance Criteria | Test Method | Status |
|---|---------------------|-------------|--------|
| 1 | HIL test passes | SITL/HIL | ⬜ |
| 2 | Motor spin test passes | Bench test | ⬜ |
| 3 | Tethered hover stable | Indoor flight | ⬜ |
| 4 | 30-minute flight test | Outdoor flight | ⬜ |
| 5 | 4-hour logging stress test | Bench test | ⬜ |
| 6 | Thermal stress test passes | Chamber test | ⬜ |
| 7 | Power brownout recovery | Power test | ⬜ |

---

## Quick Reference: Current Status

### Working ✅
- [x] PX4 boots on SAMV71-XULT
- [x] NSH shell operational
- [x] SD card logging (4-bit HSMCI)
- [x] ICM20689 IMU publishing data
- [x] SPI0 bus operational
- [x] I2C (TWIHS0) bus operational
- [x] HRT (high-resolution timer)
- [x] PWM output (3 channels)

### Partially Working 🔄
- [ ] BMP388 barometer (configured, not responding)
- [ ] AK09915 magnetometer (configured, not tested)
- [ ] USB (stubbed VBUS, basic function)
- [ ] GPIO interrupts (mode only, no callback)

### Not Started ⬜
- [ ] DShot motor protocol
- [ ] RC input capture
- [ ] CAN/UAVCAN
- [ ] Watchdog
- [ ] ADC
- [ ] True UUID
- [ ] GPBR reset storage
- [ ] Bootloader

---

## Test Commands Reference

```bash
# Sensor tests
icm20689 start -s              # Start IMU (uses board SPI config)
bmp388 start -s -b 1           # Start barometer on SPI bus 1
ak09916 start -I -b 1          # Start magnetometer on internal I2C bus 1 (TWIHS0)

# Verify sensors publishing
listener sensor_accel          # Check IMU accel data
listener sensor_gyro           # Check IMU gyro data
listener sensor_baro           # Check barometer data
listener sensor_mag            # Check magnetometer data
uorb top                       # Check topic rates (preferred for rate verification)

# PWM test
pwm_out start                  # Start PWM driver
pwm test                       # Test PWM outputs

# System info
ver all                        # Version/UUID info
top                           # CPU usage
free                          # Memory usage
```

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ⬜ | Not Started |
| 🔄 | In Progress |
| ✅ | Complete |
| 🔴 | Critical Priority |

---

*Document Version: 1.2*
*Created: December 2024*
*Updated: December 2024 - Fixed test methods and bus specs per Codex review*

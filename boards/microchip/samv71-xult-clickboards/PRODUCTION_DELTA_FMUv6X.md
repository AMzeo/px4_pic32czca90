# Production Delta: SAMV71-XULT vs STM32 FMUv6X

## Master Feature List

### ✅ Implemented (Working)

| Feature | Status | Notes |
|---------|--------|-------|
| **Core Flight Stack** | ✅ | Commander, EKF2, Navigator, MC control |
| **PWM Output (4 channels)** | ✅ | PWMC-based, 50-400 Hz (dynamic prescaler) |
| **IMU (ICM20689)** | ✅ | SPI on EXT1 header |
| **Barometer (BMP388)** | ✅ | SPI on EXT2 header |
| **Magnetometer (AK09915)** | ✅ | I2C on mikroBUS |
| **GPS** | ✅ | UART2 (USART2) |
| **Battery Monitoring** | ✅ | AFEC0 CH0 (voltage), CH7 (current) |
| **Safety Button** | ✅ | PA9 (SW0), active-low |
| **Safety LED** | ✅ | PC9 (LED1) |
| **nARMED Output** | ✅ | PA20 |
| **SD Card** | ✅ | HSMCI0 with card detect (PD18) |
| **USB CDC/ACM** | ✅ | MAVLink console |
| **Logger** | ✅ | SD card logging |
| **Parameters** | ✅ | SD card storage |
| **MAVLink** | ✅ | USB + UART telemetry |
| **RC Input (Serial)** | ✅ | UART3 (SBUS/CRSF) |
| **High-Resolution Timer** | ✅ | TC0 CH0 |
| **I2C Bus (TWIHS0)** | ✅ | Single bus, all sensors |
| **SPI Bus (SPI0)** | ✅ | Single bus, IMU + Baro |
| **Status LED** | ✅ | PA23 (Blue) |
| **PA7/XIN32 Conflict** | ✅ | FIXED - Motor 1 on PC13 |

### ❌ Not Implemented (Pending)

| Feature | Priority | Hours | Blocker? |
|---------|----------|-------|----------|
| **DShot Output** | Must-Have | 60-80 | Yes |
| **PWM 50 Hz (Servos)** | ✅ Done | 0 | No |
| **PWM Capture (RC PPM)** | Must-Have | 16-24 | Yes |
| **IO Timer Allocation API** | Must-Have | 20-30 | Yes |
| **OneShot125/42** | Must-Have | Incl. in DShot | Yes |
| **PWM Channels 5-8** | Should-Have | 16-24 | No |
| **EEPROM/FRAM Params** | Should-Have | 16-24 | No |
| **Power Rail Control** | Should-Have | 8-12 | No |
| **USB Valid Detect** | Should-Have | 4-8 | No |
| **5V Peripheral Enable** | Should-Have | 4-8 | No |
| **ADC Rail Monitoring** | Should-Have | 8-12 | No |
| **Second IMU** | Should-Have | 16-24 | No |
| **Second Barometer** | Should-Have | 8-12 | No |
| **Heater Control** | Should-Have | 4-8 | No |
| **USB Bootloader (PX4-style)** | Should-Have | 40-80 | No |
| **Camera Trigger** | Nice-to-Have | 4-8 | No |
| **Buzzer/Tone Alarm** | Nice-to-Have | 4-8 | No |
| **LED Strip** | Nice-to-Have | 8-12 | No |
| **CAN Bus Validation** | Nice-to-Have | 8-16 | No |
| **Ethernet** | N/A | - | HW missing |
| **PX4IO Co-processor** | N/A | - | HW missing |

### ⚠️ Partial / Needs Work

| Feature | Status | Issue |
|---------|--------|-------|
| **RC Input** | ⚠️ Partial | Serial only, no PPM/PWM capture |
| **SD Card Detect** | ⚠️ Conflict | PD18 conflicts with TC5 (RC capture) |
| **CAN Bus** | ⚠️ Untested | Hardware present, driver not validated |
| **QSPI Flash** | ⚠️ Planned | 8MB on board, not implemented |
| **Safety Button** | ⚠️ Conflict | PA9 conflicts with UART0_RXD |
| **Motor 4 (PB0)** | ⚠️ Conflict | Conflicts with UART0_TXD |
| **USB Bootloader** | ⚠️ SAM-BA only | ROM bootloader works, PX4-style not implemented |

---

## Executive Summary

This document provides a comprehensive comparison between the SAMV71-XULT-Clickboards development board implementation and the production-ready STM32 FMUv6X platform. The SAMV71 implementation is suitable for **evaluation and prototyping** but requires significant work for production deployment.

| Aspect | FMUv6X | SAMV71-XULT | Delta |
|--------|--------|-------------|-------|
| PWM Outputs | 8 channels | 4 channels | -4 |
| PWM Capture | 1 channel (TIM1_CH2) | Reserved only | -1 |
| IO Co-processor | PX4IO (dedicated MCU) | None | Missing |
| DShot Support | Yes (DMA burst) | No | Missing |
| Power Management | Full (rails, bricks, USB) | None | Missing |
| Sensor Redundancy | 3x IMU, 2x Baro | 1x each | None |
| Storage Redundancy | EEPROM + SD | SD only | Partial |
| Communications | CAN FD + Ethernet + USB OTG | CAN (untested) + USB device | Limited |

---

## 1. Feature Delta Summary

### 1.1 IO Co-processor & Safety Architecture

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| IO Co-processor | PX4IO (dedicated MCU) | None | Weaker isolation |
| Hard safety failsafe | Yes (IO-side) | No | Reduced failsafe robustness |
| IO-side PWM generation | Yes | No | Single point of failure |
| Safety switch | Dedicated (PF5) | Shared GPIO (PA9/SW0) | Conflicts with UART0 |
| nARMED output | Dedicated (PE6) | PA20 | OK |

**Impact:** No hardware isolation between flight controller and actuators. Single MCU failure = loss of control.

### 1.2 Power Management & Rail Control

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| Power brick validation | HW (LTC44XX, 2 bricks) | None | No redundancy detection |
| USB valid GPIO | Yes (PA9) | No | Cannot detect USB power |
| 5V peripheral enable | Yes (PG4) | No | No peripheral isolation |
| 5V HiPower enable | Yes (PG10) | No | No high-power control |
| SD card power control | Yes (PC13) | No (always on) | No power gating |
| Sensor rail power | Yes (3V3_SENSORS4) | No | No sensor isolation |
| Spektrum power control | Yes (PH2) | No | No RC power control |

**Impact:** No power redundancy, no controlled sequencing, no ability to power-cycle peripherals.

### 1.3 ADC & Board Identity

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| ADC channels | 7 + 2 HW rev | 2 (bat V/I only) | Limited telemetry |
| Rail voltage monitoring | Yes (3V3 sensors x4, 5V) | No | No health monitoring |
| Hardware version detect | Yes (PH3, PH4 + driver) | No | No board identification |
| Hardware revision GPIO | Yes (PG0) | No | Cannot distinguish revisions |

**Impact:** Limited health telemetry, no automatic board identification for firmware variants.

### 1.4 Storage & Parameter Robustness

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| Primary param storage | I2C EEPROM/FRAM | SD card | Higher failure risk |
| Backup storage | SD card | None | No fallback |
| MTD EEPROM devices | 2 (base + IMU cal) | 0 | No wear-leveled storage |
| QSPI flash | No | Planned (8MB) | Not implemented |

**Impact:** If SD card fails or is removed, parameters are lost. No non-volatile backup.

### 1.5 Sensor Redundancy & Thermal Management

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| IMU count | 3x (redundant) | 1x (ICM20689) | No sensor voting |
| Barometer count | 2x | 1x (BMP388) | No redundancy |
| Magnetometer | Internal + external | External only | Partial |
| Heater support | Yes (PB10, PWM) | No | No thermal stabilization |
| Temperature compensation | Yes (multi-IMU) | Limited | Reduced accuracy |

**Impact:** No sensor voting/failover, no thermal stabilization for IMU drift.

### 1.6 Buses & Peripheral Density

| Interface | FMUv6X | SAMV71-XULT | Gap |
|-----------|--------|-------------|-----|
| SPI buses | 6 | 1 | -5 |
| I2C buses | 4 | 1 | -3 |
| UART/Serial | 8 | 4 | -4 |
| CAN buses | 2 (CAN FD) | 2 (untested) | Validation needed |

**Impact:** Limited expansion for multi-sensor topologies, payloads, or redundant comms.

### 1.7 Communications

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| CAN FD | 2 buses, validated | 2 buses, untested | Needs validation |
| Ethernet | Yes (100Mbps) | No | No high-bandwidth link |
| USB | OTG High-Speed | Device only | No host mode |
| UAVCAN | Enabled by default | Not configured | Limited CAN ecosystem |

**Impact:** Reduced networking options for companion computers or ground links.

### 1.8 Driver/Feature Set (Default Config)

| Feature | FMUv6X Default | SAMV71-XULT Default |
|---------|---------------|---------------------|
| DShot driver | Enabled | Not available |
| PWM capture | Enabled | Not implemented |
| Camera trigger | Enabled | Not configured |
| Heater control | Enabled | Not available |
| Tone alarm | Enabled | Not configured |
| UAVCAN | Enabled | Not configured |
| Multiple GNSS | Supported | Single GPS |
| System tools | Full set | Reduced set |

### 1.9 Timing & Clocking

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| Timer prescaler | Dynamic (any freq) | Dynamic MCK/8 or MCK/64 | ✅ Full range |
| 50 Hz servo PWM | Yes | ✅ Yes (dynamic prescaler) | Compatible |
| PWM capture | Yes (TIM1_CH2) | Reserved only | No input capture |
| OneShot modes | Yes | No | Racing quad incompatible |
| DShot DMA burst | Yes | No | No DShot support |

**Impact:** Cannot drive standard 50 Hz servos without prescaler modification.

### 1.10 Mechanical & Production Readiness

| Aspect | FMUv6X | SAMV71-XULT |
|--------|--------|-------------|
| Form factor | Production PCB | Dev board |
| Connectors | JST-GH (MIL-spec compatible) | Standard 0.1" headers |
| EMI shielding | Yes | No |
| Vibration isolation | Designed in | None |
| Conformal coating | Option | No |
| Temperature range | -40 to +85°C | 0 to +70°C |
| ESD protection | Yes | Basic |

---

## 2. Production-Parity Implementation Checklist

### 2.1 Must-Have (Production Blockers)

#### 1. DShot Output (DMA Burst)

| Aspect | Details |
|--------|---------|
| **Goal** | DShot150-1200 output with DMA burst mode |
| **New files** | `platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c` |
| | `platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt` |
| | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/dshot.h` |
| **Implementation** | DMA burst to PWMC using XDMAC + PWM DMAR |
| **Tests** | DShot driver starts, `pwm_out` DShot modes work, scope timing verified |
| **Effort** | 60-80 hours |

#### 2. IO Timer Allocation + Capture Framework

| Aspect | Details |
|--------|---------|
| **Goal** | Match STM32 io_timer API (allocations, IRQ handlers, capture) |
| **Modified files** | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h` |
| | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c` |
| **New functions** | `io_timer_allocate_timer()`, `io_timer_allocate_channel()` |
| | `io_timer_unallocate_*()`, IRQ handlers, capture mode |
| **Struct changes** | Add `clock_freq`, `dshot_conf_t` to `io_timers_t` |
| | Add `masks`, `ccr_offset` to `timer_io_channels_t` |
| **Tests** | `input_capture` functional, `pwm_input` driver works |
| **Effort** | 20-30 hours |

#### 3. PWM Rate Flexibility (50-400 Hz)

| Aspect | Details |
|--------|---------|
| **Goal** | Support standard 50 Hz servo PWM |
| **Problem** | Fixed MCK/8 prescaler → min ~286 Hz (CPRD overflow) |
| **Solution** | Dynamic prescaler selection or alternate clock source |
| **Modified files** | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c` |
| **Changes** | Rate calculation with prescaler selection logic |
| **Tests** | 50 Hz output verified on scope, servo responds |
| **Effort** | 8-12 hours |

#### 4. RC Input Path

| Aspect | Details |
|--------|---------|
| **Goal** | Working RC input (PPM/PWM capture OR serial-only) |
| **Option A** | TC capture path for PPM/PWM (requires TC driver + input_capture) |
| **Option B** | Serial RC only with dedicated UART pins (no pin conflicts) |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` |
| | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| **Tests** | `rc_input` driver with actual receiver, failsafe behavior |
| **Effort** | 16-24 hours |

#### 5. SD Card Detect Policy

| Aspect | Details |
|--------|---------|
| **Goal** | Resolve PD18 conflict (RC capture vs CD) or document no-CD policy |
| **Option A** | Reassign pins (move RC or CD) |
| **Option B** | No-CD policy with hardened mount behavior (always assume present) |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| | `platforms/nuttx/NuttX/nuttx/.../sam_hsmci.c` (if no-CD) |
| **Tests** | Mount/unmount behavior, hot-plug handling |
| **Effort** | 4-8 hours |

#### 6. PA7/XIN32 Conflict Resolution

| Aspect | Details |
|--------|---------|
| **Status** | ✅ **DONE** - Motor 1 moved to PC13 |
| **Verification** | Confirm no slow-clock pins (PA7, PA8) used for PWM |
| **Files verified** | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| **Lock** | Freeze PC13 mapping, document in board_config.h |

---

### 2.2 Should-Have (Strong Parity)

#### 7. Increase PWM Outputs to 8+

| Aspect | Details |
|--------|---------|
| **Goal** | 8 PWM outputs (match FMUv6X) |
| **Option A** | Add PWM1 module channels (CH0-CH3) → 8 total |
| **Option B** | Add IO co-processor (PX4IO-like) |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| **Changes** | Channel mapping, output groups, params |
| **Effort** | 16-24 hours |

#### 8. Parameter Storage Redundancy

| Aspect | Details |
|--------|---------|
| **Goal** | EEPROM/FRAM backend for parameters (survives SD failure) |
| **New files** | `boards/microchip/samv71-xult-clickboards/src/mtd.cpp` |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| | NuttX defconfig (enable MTD, I2C EEPROM) |
| **Hardware** | Add I2C EEPROM/FRAM to board |
| **Effort** | 16-24 hours |

#### 9. Power Rail Control + Validation

| Aspect | Details |
|--------|---------|
| **Goal** | USB valid, brick valid, SD power, sensor rail GPIOs |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| | `boards/microchip/samv71-xult-clickboards/src/init.c` |
| **New GPIOs** | Power enable pins, validation inputs |
| **Tests** | Power sequencing, brownout behavior |
| **Hardware** | Requires custom PCB or adapter board |
| **Effort** | 8-12 hours (software only) |

#### 10. ADC Expansion

| Aspect | Details |
|--------|---------|
| **Goal** | Rail measurement channels (3V3, 5V, etc.) |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| | `platforms/nuttx/src/px4/microchip/samv7/adc/adc.cpp` |
| **Changes** | AFEC channel configuration, scaling factors |
| **Hardware** | Voltage dividers for rail monitoring |
| **Effort** | 8-12 hours |

#### 11. Sensor Redundancy

| Aspect | Details |
|--------|---------|
| **Goal** | Second IMU + second barometer |
| **Modified files** | `boards/microchip/samv71-xult-clickboards/src/spi.cpp` |
| | `boards/microchip/samv71-xult-clickboards/default.px4board` |
| | Startup scripts |
| **Hardware** | Add second IMU (SPI) + second baro (I2C/SPI) |
| **Effort** | 16-24 hours |

#### 12. USB Bootloader

| Aspect | Details |
|--------|---------|
| **Goal** | PX4-compatible firmware update via USB (like FMUv6X) |
| **Current state** | SAM-BA ROM bootloader available (ERASE+RESET → bossac) |
| **Option A** | **PX4-style USB bootloader** (production-grade) |
| | New files: `boards/microchip/samv71-xult-clickboards/bootloader.px4board` |
| | `boards/microchip/samv71-xult-clickboards/src/bootloader_main.c` |
| | `boards/microchip/samv71-xult-clickboards/src/hw_config.h` |
| | USB DFU or CDC-based PX4 bootloader protocol |
| | Flash layout: reserve first sector(s) for bootloader |
| | Update linker scripts (bootloader + app) |
| | Pros: Works with PX4 uploader, versioning, safety checks |
| | Cons: 40-80 hours development, ongoing maintenance |
| **Option B** | **SAM-BA ROM bootloader** (evaluation/prototype) |
| | Document ERASE+RESET procedure in README |
| | Provide bossac/SAM-BA CLI command examples |
| | Keep app linker at standard flash start |
| | Pros: Zero development, proven ROM code |
| | Cons: Not PX4 uploader compatible, manual steps |
| **Recommendation** | Use SAM-BA for prototyping, implement PX4 bootloader for production |
| **Effort** | Option A: 40-80 hours, Option B: 2-4 hours (docs only) |

---

### 2.3 Nice-to-Have

#### 13. Camera Trigger/Capture

| Aspect | Details |
|--------|---------|
| **Goal** | GPIO trigger + timer capture for camera sync |
| **Effort** | 4-8 hours |

#### 14. Tone Alarm/Buzzer

| Aspect | Details |
|--------|---------|
| **Goal** | PWM or GPIO toggle for audio feedback |
| **Effort** | 4-8 hours |

#### 15. LED Strip Support

| Aspect | Details |
|--------|---------|
| **Goal** | SPI or timer-based LED strip driver |
| **Effort** | 8-12 hours |

#### 16. Ethernet

| Aspect | Details |
|--------|---------|
| **Status** | Not available on SAMV71-XULT hardware |
| **Alternative** | Custom PCB with SAMV71 + PHY |

---

## 3. Effort Summary

| Category | Items | Hours |
|----------|-------|-------|
| **Must-Have** | 1-6 | 108-154 |
| **Should-Have** | 7-12 | 104-176 |
| **Nice-to-Have** | 13-15 | 16-28 |
| **Total** | | **228-358** |

### Must-Have Breakdown

| Item | Hours |
|------|-------|
| 1. DShot output (DMA burst) | 60-80 |
| 2. IO timer allocation + capture | 20-30 |
| 3. PWM rate flexibility (50 Hz) | ✅ Done |
| 4. RC input path | 16-24 |
| 5. SD card detect policy | 4-8 |
| 6. PA7/XIN32 resolution | ✅ Done |

### Should-Have Breakdown

| Item | Hours |
|------|-------|
| 7. PWM outputs to 8+ | 16-24 |
| 8. Parameter storage (EEPROM) | 16-24 |
| 9. Power rail control | 8-12 |
| 10. ADC expansion | 8-12 |
| 11. Sensor redundancy | 16-24 |
| 12. USB bootloader (PX4-style) | 40-80 |

---

## 4. PWM/Timer Technical Details

### 4.1 Struct-Level Incompatibilities

**STM32 `io_timers_t` (target):**
```c
typedef struct io_timers_t {
    uint32_t    base;
    uint32_t    clock_register;
    uint32_t    clock_bit;
    uint32_t    clock_freq;      // ❌ SAMV7 MISSING
    uint32_t    vectorno;
    dshot_conf_t dshot;          // ❌ SAMV7 MISSING
} io_timers_t;
```

**SAMV7 `io_timers_t` (current):**
```c
typedef struct io_timers_t {
    uint32_t  base;
    uint32_t  clock_register;
    uint32_t  clock_bit;
    uint32_t  vectorno;
    // Missing: clock_freq, dshot_conf_t
} io_timers_t;
```

**STM32 `timer_io_channels_t` (target):**
```c
typedef struct timer_io_channels_t {
    uint32_t    gpio_out;
    uint32_t    gpio_in;
    uint8_t     timer_index;
    uint8_t     timer_channel;
    uint16_t    masks;           // ❌ SAMV7 MISSING
    uint8_t     ccr_offset;      // ❌ SAMV7 MISSING
} timer_io_channels_t;
```

### 4.2 Function Implementation Gap

| Function | STM32 | SAMV7 | Needed For |
|----------|-------|-------|------------|
| `io_timer_allocate_timer()` | ✅ | ❌ | Safe mode switching |
| `io_timer_allocate_channel()` | ✅ | ❌ | Conflict detection |
| `io_timer_handler0-7()` | ✅ (8) | ❌ | Capture callbacks |
| `io_timer_set_dshot_burst_mode()` | ✅ | ❌ | DShot output |
| `io_timer_set_dshot_capture_mode()` | ✅ | ❌ | Bidirectional DShot |
| `io_timer_update_dma_req()` | ✅ | ❌ | DMA transfers |
| `io_timer_trigger()` | ✅ (active) | ⚠️ (no-op) | OneShot |
| `io_timer_set_oneshot_mode()` | ✅ | ❌ | OneShot125/42 |
| Capture mode | ✅ | ❌ | RC input, RPM |

### 4.3 PWM Rate Limitation

```
SAMV7 PWMC Current Configuration:
├── MCK = 150 MHz
├── Prescaler = MCK/8 (fixed) = 18.75 MHz
├── CPRD register = 16-bit (max 65535)
└── Minimum frequency = 18.75 MHz / 65535 ≈ 286 Hz

Problem: Standard servo PWM requires 50 Hz
Solution: Dynamic prescaler selection (MCK/1024 for low rates)
```

### 4.4 Pin Conflicts

| Pin | Current Use | Conflicts With | Status |
|-----|-------------|----------------|--------|
| PA7 | (was Motor 1) | XIN32 crystal | ✅ **FIXED** → PC13 |
| PA9 | Safety Button | UART0_RXD | ⚠️ **Active** (UART0 enabled) |
| PB0 | Motor 4 | UART0_TXD | ⚠️ **Active** |
| PC13 | Motor 1 (new) | LCD RST | OK if LCD unused |
| PD18 | SD Card Detect | TC5 TIOA (RC) | ⚠️ **Review needed** |

---

## 5. Reference Files

### STM32 FMUv6X (Reference Implementation):
```
boards/px4/fmu-v6x/src/board_config.h
boards/px4/fmu-v6x/src/timer_config.cpp
boards/px4/fmu-v6x/default.px4board
platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c
platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c
platforms/nuttx/src/px4/stm/stm32_common/io_pins/input_capture.c
platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/io_timer.h
platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/dshot.h
```

### SAMV71-XULT (Current Implementation):
```
boards/microchip/samv71-xult-clickboards/src/board_config.h
boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
boards/microchip/samv71-xult-clickboards/default.px4board
platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c
platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h
```

---

## 6. Recommendations

### Current State (Evaluation/Prototyping)

The SAMV71-XULT is **suitable** for:
- PX4 porting feasibility assessment
- Basic flight testing with PWM ESCs (≥286 Hz)
- Sensor integration development
- Ground vehicle/rover applications
- Educational purposes

**Not suitable** for:
- Production aircraft
- DShot ESCs
- Standard 50 Hz servos (without modification)
- Applications requiring sensor redundancy
- Safety-critical deployments

### Path to Production

1. **Custom PCB Design** - XULT dev board has fundamental limitations
2. **Implement Must-Have items** (108-154 hours)
3. **Implement Should-Have items** for full parity (64-96 hours)
4. **Flight Testing** - Extensive validation required

### Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| Continue SAMV7 dev | Lower chip cost, learning | High software effort |
| Use FMUv6X for production | Proven, full-featured | Higher unit cost |
| Custom STM32 design | Proven software stack | Hardware development |
| Add PX4IO to SAMV7 | Hardware safety isolation | Two MCUs to manage |

---

*Document Version: 2.3*
*Date: 2025*
*Format: Master feature list + Production-parity implementation checklist*

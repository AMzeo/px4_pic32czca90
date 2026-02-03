# Production Delta: SAMV71-XULT vs STM32 FMUv6X

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
| Timer prescaler | Dynamic (any freq) | Fixed MCK/8 | Min ~286 Hz |
| 50 Hz servo PWM | Yes | No (needs rework) | Servo incompatible |
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

## 2. PWM/Timer Deep Dive

### 2.1 Struct-Level Incompatibilities (Production Blocker)

**STM32 `io_timers_t` (full featured):**
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

**SAMV7 `io_timers_t` (minimal):**
```c
typedef struct io_timers_t {
    uint32_t  base;
    uint32_t  clock_register;
    uint32_t  clock_bit;
    uint32_t  vectorno;
    // Missing: clock_freq, dshot_conf_t
} io_timers_t;
```

**STM32 `timer_io_channels_t`:**
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

### 2.2 Function Implementation Gap

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

### 2.3 PWM Rate Limitation

```
SAMV7 PWMC Configuration:
- MCK = 150 MHz
- Prescaler = MCK/8 (fixed) = 18.75 MHz
- CPRD register = 16-bit (max 65535)
- Minimum frequency = 18.75 MHz / 65535 ≈ 286 Hz

Problem: Standard servo PWM requires 50 Hz
Solution needed: Dynamic prescaler selection
```

### 2.4 Pin Conflicts

| Pin | Current Use | Conflicts With | Status |
|-----|-------------|----------------|--------|
| PA7 | (was Motor 1) | XIN32 crystal | **FIXED** → PC13 |
| PA9 | Safety Button | UART0_RXD | **Active** (UART0 enabled) |
| PB0 | Motor 4 | UART0_TXD | **Active** |
| PC13 | Motor 1 (new) | LCD RST | OK if LCD unused |
| PD18 | SD Card Detect | TC5 TIOA (RC) | **Review needed** |

---

## 3. Production Delta Matrix

### 3.1 Must-Have (Production Blockers)

| Item | STM32 Reference | SAMV7 Gap | Priority |
|------|-----------------|-----------|----------|
| **DShot output + DMA burst** | `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c` | `io_timer_pwmc.c` has no DMA | P0 |
| **IO-timer allocation framework** | `io_timer.c` (allocation, IRQ, capture) | No allocation tracking | P0 |
| **PWM frequency flexibility (50-400 Hz)** | Dynamic prescaler in `timer_set_rate()` | Fixed MCK/8, min ~286 Hz | P0 |
| **RC input capture** | `input_capture.c` (PPM/PWM) | Reserved only, not implemented | P0 |
| **SD card detect conflict** | N/A | PD18 vs RC capture | P0 |
| **PA7/XIN32 resolved** | N/A | **FIXED** (now PC13) | Done |
| **Bidirectional DShot (optional)** | `dshot.c` capture mode | Not implemented | P1 |

### 3.2 Should-Have (Strong Parity)

| Item | FMUv6X Implementation | SAMV7 Gap |
|------|----------------------|-----------|
| **Expand to 8+ outputs** | 8 PWM + capture | Only 4 (PWM0); add PWM1 or IO co-processor |
| **Parameter storage redundancy** | I2C EEPROM/FRAM | SD only |
| **Power rail control** | GPIO enables for 5V, HiPower, SD, sensors | None |
| **Power brick validation** | LTC44XX + GPIO | None |
| **USB valid detection** | GPIO (PA9) | None |
| **ADC expansion** | 7 channels + HW rev | 2 channels only |
| **Sensor redundancy** | 3 IMU + 2 baro | 1 IMU + 1 baro |
| **Hardware version detect** | ADC + GPIO | None |

### 3.3 Nice-to-Have

| Item | Notes |
|------|-------|
| Camera trigger/capture | GPIO + timer capture |
| Tone alarm/buzzer | PWM or GPIO toggle |
| LED strip | SPI or timer-based |
| Ethernet | Not available on SAMV71-XULT |
| Extra I2C/SPI buses | Board rework required |
| Heater control | PWM output + thermistor |

---

## 4. Implementation Effort Estimate

### 4.1 Must-Have Items

| Item | Complexity | Hours | Dependencies |
|------|------------|-------|--------------|
| DShot + DMA driver | High | 60-80 | XDMAC driver, struct changes, dshot.h |
| IO-timer allocation | Medium | 20-30 | Struct alignment, IRQ setup |
| PWM capture framework | Medium | 20-30 | TC driver, callback system |
| Dynamic prescaler (50 Hz) | Low | 8-12 | Prescaler selection logic |
| RC input capture | Medium | 16-24 | TC capture + driver integration |
| SD card detect resolution | Low | 4-8 | Pin reassignment or policy |
| **Subtotal** | | **128-184** | |

### 4.2 Should-Have Items

| Item | Complexity | Hours | Dependencies |
|------|------------|-------|--------------|
| PWM1 module (4 more outputs) | Medium | 16-24 | Pin selection, timer_config |
| EEPROM/FRAM driver | Medium | 16-24 | I2C + MTD integration |
| Power rail GPIO control | Low | 8-12 | GPIO setup, sequencing |
| ADC expansion (5+ channels) | Low | 8-12 | AFEC configuration |
| Hardware version detect | Low | 4-8 | ADC + param storage |
| Second IMU integration | Medium | 16-24 | SPI + driver config |
| **Subtotal** | | **68-104** | |

### 4.3 Nice-to-Have Items

| Item | Complexity | Hours |
|------|------------|-------|
| Camera trigger | Low | 4-8 |
| Tone alarm | Low | 4-8 |
| LED strip | Low | 8-12 |
| Extra I2C bus | Low | 4-8 |
| **Subtotal** | | **20-36** |

### 4.4 Total Effort Summary

| Category | Hours | Cost @ $150/hr |
|----------|-------|----------------|
| Must-Have (Production blockers) | 128-184 | $19,200-27,600 |
| Should-Have (Strong parity) | 68-104 | $10,200-15,600 |
| Nice-to-Have | 20-36 | $3,000-5,400 |
| **Total** | **216-324** | **$32,400-48,600** |

---

## 5. Recommendations

### 5.1 Current State (Evaluation/Prototyping)

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

### 5.2 Path to Production

1. **Custom PCB Design** - XULT dev board has fundamental limitations
2. **Struct Alignment** - Extend `io_timers_t` and `timer_io_channels_t`
3. **DShot + DMA** - Port STM32 dshot.c to SAMV7 XDMAC
4. **Allocation System** - Add timer/channel tracking
5. **Prescaler Fix** - Enable 50 Hz servo support
6. **Sensor Redundancy** - Add 2nd/3rd IMU, 2nd baro
7. **Storage Redundancy** - Add EEPROM/FRAM
8. **Flight Testing** - Extensive validation

### 5.3 Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| Continue SAMV7 dev | Lower chip cost, learning | High software effort |
| Use FMUv6X for production | Proven, full-featured | Higher unit cost |
| Custom STM32 design | Proven software stack | Hardware development |
| Add PX4IO to SAMV7 | Hardware safety isolation | Two MCUs to manage |

---

## 6. Reference Files

### STM32 FMUv6X References:
- `boards/px4/fmu-v6x/src/board_config.h`
- `boards/px4/fmu-v6x/src/timer_config.cpp`
- `boards/px4/fmu-v6x/default.px4board`
- `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c`
- `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c`
- `platforms/nuttx/src/px4/stm/stm32_common/io_pins/input_capture.c`
- `platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/io_timer.h`

### SAMV71-XULT References:
- `boards/microchip/samv71-xult-clickboards/src/board_config.h`
- `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`
- `boards/microchip/samv71-xult-clickboards/default.px4board`
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h`

---

*Document Version: 2.0*
*Date: 2025*
*Major revision: Added comprehensive feature delta analysis*
*Author: Claude Code Assistant*

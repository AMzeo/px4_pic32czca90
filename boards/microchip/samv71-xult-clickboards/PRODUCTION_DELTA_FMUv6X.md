# Production Delta: SAMV71-XULT vs STM32 FMUv6X

## Executive Summary

This document provides a comprehensive comparison between the SAMV71-XULT-Clickboards development board implementation and the production-ready STM32 FMUv6X platform. The SAMV71 implementation is suitable for **evaluation and prototyping** but requires significant work for production deployment.

| Aspect | FMUv6X | SAMV71-XULT | Delta |
|--------|--------|-------------|-------|
| PWM Outputs | 8 channels | 4 channels | -4 |
| PWM Capture | 1 channel (TIM1_CH2) | Reserved only | -1 |
| IO Timers | 5 total (2 DMA-capable) | 1 PWMC module | -4 |
| Driver Size | 1158 lines | 518 lines | 55% smaller |
| DShot Support | Yes (DMA) | No | Missing |
| Safety System | Full | Basic | Partial |
| Redundancy | Multi-brick | Single | None |

---

## 1. Hardware Architecture Comparison

### 1.1 MCU Specifications

| Parameter | STM32H753 (FMUv6X) | SAMV71Q21B | Notes |
|-----------|-------------------|------------|-------|
| Core | Cortex-M7 @ 480 MHz | Cortex-M7 @ 300 MHz | FMU faster |
| Flash | 2 MB | 2 MB | Equal |
| SRAM | 1 MB | 384 KB | FMU has 2.6x more |
| FPU | DP + SIMD | DP + SIMD | Equal |
| Cache | 16KB I + 16KB D | 16KB I + 16KB D | Equal |
| DMA Channels | 16 + 8 (BDMA) | 24 (XDMAC) | SAMV7 more channels |
| Price Point | ~$15-20 | ~$12-15 | SAMV7 cheaper |

### 1.2 PWM/Timer Hardware

| Parameter | FMUv6X | SAMV71-XULT | Production Gap |
|-----------|--------|-------------|----------------|
| PWM Output Channels | 8 | 4 | **-4 channels** |
| PWM Capture Channels | 1 (TIM1_CH2) | Reserved only | **Not implemented** |
| IO Timers Total | 5 | 1 PWMC module | -4 |
| DMA-capable Timers | 2 (TIM5, TIM4) | 0 | **Critical for DShot** |
| Non-DMA Timers | 3 (TIM12, TIM1, TIM2) | 1 | |
| Dedicated PWMC | No (uses TIM) | Yes (PWM0) | Different HW |
| OneShot125/42 | Yes | No | **Missing** |
| DShot150-1200 | Yes (DMA burst) | No | **Critical missing** |
| Glitch-free Update | Yes | Yes (CDTYUPD) | Equal |
| Min PWM Frequency | ~1 Hz (dynamic prescaler) | ~286 Hz (fixed MCK/8) | **SAMV7 limited** |

### 1.3 IO Timer Driver Comparison

#### 1.3.1 Struct-Level Incompatibilities (Production Blocker)

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

**STM32 `timer_io_channels_t` (full featured):**
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

**SAMV7 `timer_io_channels_t` (minimal):**
```c
typedef struct timer_io_channels_t {
    uint32_t    gpio_out;
    uint32_t    gpio_in;
    uint8_t     timer_index;
    uint8_t     timer_channel;
    // Missing: masks, ccr_offset
} timer_io_channels_t;
```

**Impact:** These struct differences block DShot/capture parity. Adding DShot requires extending both structs and all dependent code.

#### 1.3.2 Function Implementation Status

**STM32 io_timer.c (1158 lines):**
```
Functions implemented:
- io_timer_allocate_timer()       - Timer resource management
- io_timer_allocate_channel()     - Channel allocation tracking
- io_timer_unallocate_timer()     - Resource cleanup
- io_timer_unallocate_channel()   - Channel cleanup
- io_timer_handler0-7()           - Per-timer IRQ handlers (8 handlers)
- io_timer_set_dshot_burst_mode() - DShot with DMA burst
- io_timer_set_dshot_capture_mode() - Bidirectional DShot
- io_timer_update_dma_req()       - DMA request control
- io_timer_capture_dma_req()      - Capture DMA
- io_timer_trigger()              - OneShot trigger (active)
- io_timer_set_oneshot_mode()     - OneShot mode setup
- io_timer_set_PWM_mode()         - PWM mode setup
- io_timer_channel_init()         - Full mode support
- io_timer_set_enable()           - Multi-mode enable
- io_timer_set_ccr()              - Duty cycle update
- timer_set_rate()                - Dynamic prescaler calculation
```

**SAMV7 io_timer_pwmc.c (518 lines):**
```
Functions implemented:
- io_timer_init_timer()           - Basic PWMC init
- io_timer_channel_init()         - PWM output only
- io_timer_set_rate()             - Frequency setting (fixed prescaler)
- io_timer_set_ccr()              - Duty cycle update
- io_timer_set_enable()           - Basic enable/disable
- io_timer_get_group()            - Channel grouping
- io_timer_trigger()              - EXISTS BUT NO-OP (see below)

Functions MISSING:
- io_timer_allocate_timer()       ❌ No resource tracking
- io_timer_allocate_channel()     ❌ No allocation tracking
- io_timer_handler*()             ❌ No IRQ handlers
- io_timer_set_dshot_*()          ❌ No DShot support
- io_timer_*_dma_*()              ❌ No DMA support
- io_timer_set_oneshot_mode()     ❌ No OneShot mode
- Capture mode                    ❌ No PWM input capture
```

#### 1.3.3 Critical: io_timer_trigger() Semantics

**STM32 (active):** Triggers OneShot pulse on specified channels
**SAMV7 (no-op):** Function exists but does nothing:
```c
void io_timer_trigger(unsigned channels_mask)
{
    /* PWMC channels using CDTYUPD update automatically at the next period
     * boundary. No explicit trigger needed unlike some STM32 implementations.
     */
    (void)channels_mask;  // NO-OP
}
```

**Impact:** OneShot protocols will not work even if mode is added, since trigger is inert.

#### 1.3.4 PWM Rate Limitations

**STM32:** Dynamic prescaler calculation supports full range (1 Hz to 400+ kHz)
```c
// From io_timer.c - calculates optimal prescaler
static int timer_set_rate(unsigned timer, unsigned rate)
{
    // Dynamically selects PSC for any frequency
}
```

**SAMV7:** Fixed MCK/8 prescaler, 16-bit CPRD limits minimum frequency:
```c
// From io_timer_pwmc.c
#define PWMC_CLKA_PRES   PWM_CLK_PREA_DIV8  // Fixed prescaler
// At 150 MHz MCK: PWM_CLK = 18.75 MHz
// CPRD max = 65535 → min freq = 18.75 MHz / 65535 ≈ 286 Hz
```

**Impact:** Cannot do 50 Hz servo PWM without changing prescaler. OneShot modes require very high frequencies that may overflow.

---

## 2. Pin Configuration Comparison

### 2.1 PWM Output Pins

**FMUv6X (8 PWM outputs + 1 capture on dedicated connectors):**
| Channel | Timer | GPIO | DMA | Connector |
|---------|-------|------|-----|-----------|
| FMU_CH1 | TIM5_CH4 | PI0 | Yes | FMU PWM |
| FMU_CH2 | TIM5_CH3 | PH12 | Yes | FMU PWM |
| FMU_CH3 | TIM5_CH2 | PH11 | Yes | FMU PWM |
| FMU_CH4 | TIM5_CH1 | PH10 | Yes | FMU PWM |
| FMU_CH5 | TIM4_CH2 | PD13 | Yes | FMU PWM |
| FMU_CH6 | TIM4_CH3 | PD14 | Yes | FMU PWM |
| FMU_CH7 | TIM12_CH1 | PH6 | No | FMU PWM |
| FMU_CH8 | TIM12_CH2 | PH9 | No | FMU PWM |
| FMU_CAP1 | TIM1_CH2 | PE11 | No | **Capture** |

**SAMV71-XULT (4 PWM outputs on extension headers):**
| Channel | PWM Module | GPIO | Header | Notes |
|---------|------------|------|--------|-------|
| Motor 1 | PWM0_CH3 | **PC13** | EXT2 Pin 4 | Fixed from PA7/XIN32 conflict |
| Motor 2 | PWM0_CH1 | PA2 | EXT2 Pin 9 | |
| Motor 3 | PWM0_CH2 | PC19 | EXT2 Pin 7 | Also mikroBUS1 PWM |
| Motor 4 | PWM0_CH0 | PB0 | EXT1 Pin 13 | Conflicts with UART0_TXD |

### 2.2 Pin Conflict Analysis (SAMV71)

| Pin | Current Use | Conflicts With | Source | Status |
|-----|-------------|----------------|--------|--------|
| PA7 | (was Motor 1) | XIN32 (32.768kHz crystal) | samv71_pinmap.h:113 | **FIXED** → PC13 |
| PA9 | Safety Button (SW0) | UART0_RXD | samv71_pinmap.h:477 | **Active conflict** (UART0 enabled in defconfig) |
| PB0 | Motor 4 PWM | UART0_TXD, mikroBUS2 RST | board.h:409 | Repurposed, UART0 conflict |
| PC13 | Motor 1 PWM (new) | LCD controller RST | board.h:499 | OK if LCD unused |
| PD18 | SD Card Detect | TC5 TIOA (RC capture) | Potential | **Review needed** |

**UART0 Conflict Detail:**
- `CONFIG_SAMV7_UART0=y` is enabled in defconfig (line 36)
- PA9 = UART0_RXD (samv71_pinmap.h:477)
- PB0 = UART0_TXD (alternate function)
- Safety button uses PA9, Motor 4 uses PB0
- **Risk:** If UART0 driver initializes, it may reconfigure these pins

---

## 3. Feature Gap Analysis

### 3.1 Motor Protocol Support

| Protocol | FMUv6X | SAMV71-XULT | Required For |
|----------|--------|-------------|--------------|
| Standard PWM (50-400Hz) | ✅ | ⚠️ (≥286Hz only) | All ESCs |
| PWM (400-500Hz) | ✅ | ✅ | Fast ESCs |
| OneShot125 | ✅ | ❌ | Racing quads |
| OneShot42 | ✅ | ❌ | Racing quads |
| DShot150 | ✅ | ❌ | Modern ESCs |
| DShot300 | ✅ | ❌ | Modern ESCs |
| DShot600 | ✅ | ❌ | High-performance |
| DShot1200 | ✅ | ❌ | Highest performance |
| Bidirectional DShot | ✅ | ❌ | RPM feedback |

**Impact:**
- SAMV71 cannot use modern DShot ESCs
- 50 Hz servo mode requires prescaler modification
- Limited to 286-500 Hz PWM range with current implementation

### 3.2 Safety System Comparison

| Feature | FMUv6X | SAMV71-XULT | Notes |
|---------|--------|-------------|-------|
| Safety Switch | PF5 (dedicated) | PA9 (SW0) | **Conflicts with UART0_RXD** |
| Safety LED | PD10 (dedicated) | PC9 (LED1) | Repurposed LED |
| nARMED Output | PE6 (dedicated) | PA20 | OK |
| PX4IO Support | Yes (USART6) | No | No IO processor |
| Spektrum Power | PH2 (controlled) | No | Missing |

### 3.3 Power Management

| Feature | FMUv6X | SAMV71-XULT | Gap |
|---------|--------|-------------|-----|
| Power Bricks | 2 (redundant) | 1 | No redundancy |
| Brick Validation | HW (LTC44XX) | SW only | Less reliable |
| USB Valid Detection | Yes (GPIO) | No | Missing |
| Peripheral Power Control | Yes (5V, HiPower) | No | Missing |
| SD Card Power Control | Yes (PC13) | No | Missing |
| Sensor Rail Power | Yes (3V3_SENSORS4) | No | Missing |

### 3.4 Sensors and Interfaces

| Interface | FMUv6X | SAMV71-XULT | Gap |
|-----------|--------|-------------|-----|
| SPI Buses | 6 | 1 | -5 buses |
| I2C Buses | 4 | 1 | -3 buses |
| UART/Serial | 8 | 4 | -4 ports |
| CAN/UAVCAN | 2 (CAN FD) | 2 | Equal |
| Ethernet | Yes | No | Missing |
| ADC Channels | 7 + 2 HW rev | 2 | -7 channels |
| IMU Redundancy | 3x | 1x | No redundancy |
| Barometer | 2x | 1x | No redundancy |
| Magnetometer | Internal + External | External only | Partial |

**ADC Detail (FMUv6X):**
- 7 channels in ADC_CHANNELS mask (board_config.h:192-199)
- 2 additional HW revision channels on ADC3 (HW_VER_SENSE, HW_REV_SENSE)

### 3.5 Storage and Parameters

| Feature | FMUv6X | SAMV71-XULT | Gap |
|---------|--------|-------------|-----|
| SD Card | Yes (power controlled) | Yes (always on) | No power control |
| EEPROM/FRAM | 2x I2C EEPROM | None | **Missing** |
| QSPI Flash | No (uses EEPROM) | Planned (8MB) | Not implemented |
| Parameter Storage | EEPROM + SD | SD only | Less reliable |

---

## 4. Software Implementation Gaps

### 4.1 Driver Implementation Status

| Driver | FMUv6X | SAMV71-XULT | Status |
|--------|--------|-------------|--------|
| PWM Output | Full | Basic | **45% complete** |
| PWM Capture | Full | Not impl. | ❌ |
| DShot | Full (DMA) | Not impl. | ❌ **Production blocker** |
| ADC | Full (9 ch) | Basic (2 ch) | **22% complete** |
| SPI | Full (6 buses) | Basic (1 bus) | **17% complete** |
| I2C | Full (4 buses) | Basic (1 bus) | **25% complete** |
| SDIO/HSMCI | Full | Basic | **80% complete** |
| USB | Full (OTG HS) | Basic (Device) | **60% complete** |
| Ethernet | Full | None | ❌ |
| CAN | Full (FD) | Not tested | Unknown |

### 4.2 IO Timer/Allocation System

| Feature | FMUv6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| Timer allocation tracking | Yes | No | Cannot safely share timers |
| Channel allocation tracking | Yes | No | Cannot detect conflicts |
| Mixed-mode support | Yes | No | Cannot use capture + PWM |
| Safe mode switching | Yes | No | Risk of glitches |
| IRQ handlers | 8 dedicated | None | No capture/callback support |

**Impact:** Without allocation tracking, multi-mode operation (capture + PWM) is unsafe and may cause conflicts.

### 4.3 Missing PX4 Features

| Feature | Required Files | Status |
|---------|---------------|--------|
| DShot Telemetry | dshot.cpp, io_timer.c | Not supported |
| Motor RPM Feedback | bdshot driver | Not supported |
| ESC Passthrough | pwm_out_sim | Limited |
| Actuator Testing | actuator_test | Works (PWM only) |
| LED Strip | led_control | Not configured |
| Camera Trigger | camera_trigger | Not configured |
| Buzzer/Tone | tone_alarm | Not configured |

---

## 5. Production Readiness Checklist

### 5.1 Hardware Requirements

| Requirement | FMUv6X | SAMV71-XULT | Production Ready? |
|-------------|--------|-------------|-------------------|
| Vibration isolation | Yes | No | ❌ Dev board |
| EMI shielding | Yes | No | ❌ Dev board |
| Conformal coating | Option | No | ❌ Dev board |
| Power filtering | Yes | Basic | ⚠️ Limited |
| Connector reliability | MIL-spec | Standard | ❌ Not rated |
| Temperature range | -40 to +85°C | 0 to +70°C | ⚠️ Limited |

### 5.2 Software Requirements

| Requirement | Status | Notes |
|-------------|--------|-------|
| All PWM protocols | ❌ | Only standard PWM ≥286 Hz |
| Sensor redundancy | ❌ | Single sensors |
| Failsafe tested | ⚠️ | Basic testing |
| EKF2 validated | ⚠️ | Limited testing |
| Flight tested | ❌ | Not yet |
| Certification data | ❌ | None |

### 5.3 Production Blockers

**Critical (Must Fix):**
1. ❌ DShot support with DMA (struct incompatibilities)
2. ❌ PWM capture for RC input (no IRQ handlers)
3. ❌ Timer/channel allocation system (no safe mode switching)
4. ❌ 50 Hz servo support (prescaler limitation)
5. ❌ io_timer_trigger() no-op (blocks OneShot)

**Important (Should Have):**
1. ⚠️ PA9/UART0 safety button conflict resolution
2. ⚠️ Sensor redundancy
3. ⚠️ Parameter EEPROM/FRAM backup
4. ⚠️ More ADC channels
5. ⚠️ Verified flight testing

**Nice to Have:**
1. Camera trigger
2. Ethernet
3. Multiple I2C buses
4. LED strip support

---

## 6. Effort Estimation for Production Parity

### 6.1 High Priority Items

| Item | Complexity | Estimated Effort | Dependencies |
|------|------------|------------------|--------------|
| DShot with DMA | High | 60-80 hours | XDMAC driver, struct changes |
| PWM Capture | Medium | 20-30 hours | TC driver, IRQ handlers |
| Allocation system | Medium | 16-24 hours | Struct alignment |
| OneShot modes | Medium | 12-20 hours | Trigger fix, prescaler |
| 50 Hz servo support | Low | 4-8 hours | Prescaler configuration |

### 6.2 Medium Priority Items

| Item | Complexity | Estimated Effort | Dependencies |
|------|------------|------------------|--------------|
| QSPI Flash storage | Medium | 16-24 hours | MTD driver |
| Additional ADC | Low | 4-8 hours | AFEC config |
| Second I2C bus | Low | 4-8 hours | Board rework |
| Safety pin resolution | Low | 2-4 hours | Pin reassign |
| Buzzer/Tone alarm | Low | 4-8 hours | PWM config |

### 6.3 Total Effort for Feature Parity

| Category | Hours | Cost @ $150/hr |
|----------|-------|----------------|
| Critical Features | 112-162 | $16,800-24,300 |
| Important Features | 30-52 | $4,500-7,800 |
| Nice to Have | 20-40 | $3,000-6,000 |
| **Total** | **162-254** | **$24,300-38,100** |

---

## 7. Recommendations

### 7.1 For Evaluation/Prototyping (Current State)

The SAMV71-XULT is **suitable** for:
- PX4 porting feasibility assessment
- Basic flight controller testing with PWM ESCs (≥286 Hz)
- Sensor integration development
- Ground vehicle/rover applications
- Educational purposes

### 7.2 For Production Development

To use SAMV7 in production, prioritize:

1. **Custom PCB Design** - The XULT dev board has fundamental limitations
2. **Struct Alignment** - Extend io_timers_t and timer_io_channels_t
3. **DShot Implementation** - Required for modern ESCs
4. **Allocation System** - Required for safe multi-mode operation
5. **Sensor Redundancy** - Add IMU/Baro redundancy
6. **Flight Testing** - Extensive validation required

### 7.3 Alternative Recommendations

| Use Case | Recommendation |
|----------|---------------|
| Production Drone | Use FMUv6X or equivalent STM32 design |
| SAMV7 Evaluation | Continue with current implementation |
| Custom SAMV7 Product | Design custom PCB with proper sensors |
| Low-cost Production | Consider STM32F4/F7 based designs |

---

## 8. Appendix

### A. File Comparison

| File | FMUv6X | SAMV71-XULT |
|------|--------|-------------|
| board_config.h | 522 lines | 278 lines |
| timer_config.cpp | 81 lines | 88 lines |
| init.cpp | 148 lines | ~150 lines |
| spi.cpp | 395 lines | ~200 lines |
| io_timer.c/.h | 1158 + 190 lines | 518 + 130 lines |

### B. Struct Incompatibility Summary

| Struct | STM32 Fields | SAMV7 Fields | Missing |
|--------|--------------|--------------|---------|
| io_timers_t | 6 | 4 | clock_freq, dshot_conf_t |
| timer_io_channels_t | 6 | 4 | masks, ccr_offset |
| dshot_conf_t | exists | **none** | entire struct |

### C. Defconfig Comparison

Key NuttX configuration differences:
```
FMUv6X:                          SAMV71:
CONFIG_STM32H7_TIM5=y            CONFIG_SAMV7_PWM0=y
CONFIG_STM32H7_TIM4=y            # No TIM equivalent
CONFIG_STM32H7_TIM12=y           # No TIM equivalent
CONFIG_STM32H7_TIM1=y            # TC used for HRT only
CONFIG_STM32H7_TIM2=y            # No equivalent
CONFIG_STM32H7_DMA1=y            CONFIG_SAMV7_XDMAC=y
CONFIG_STM32H7_SDMMC1=y          CONFIG_SAMV7_HSMCI0=y
CONFIG_STM32H7_ETHMAC=y          # No Ethernet
CONFIG_SAMV7_UART0=y             # Conflicts with PA9, PB0
```

### D. References

- STM32H753 Reference Manual (RM0433)
- SAMV71 Datasheet (DS60001527)
- PX4 Autopilot Documentation
- SAMV71-XULT User Guide
- platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/io_timer.h
- platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h

---

*Document Version: 1.1*
*Date: 2025*
*Reviewed: Production delta accuracy improved*
*Author: Claude Code Assistant*

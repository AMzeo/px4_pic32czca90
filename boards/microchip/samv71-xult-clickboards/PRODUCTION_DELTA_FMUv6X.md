# Production Delta: SAMV71-XULT vs STM32 FMUv6X

## Executive Summary

This document provides a comprehensive comparison between the SAMV71-XULT-Clickboards development board implementation and the production-ready STM32 FMUv6X platform. The SAMV71 implementation is suitable for **evaluation and prototyping** but requires significant work for production deployment.

| Aspect | FMUv6X | SAMV71-XULT | Delta |
|--------|--------|-------------|-------|
| PWM Outputs | 9 channels | 4 channels | -5 |
| IO Timers | 5 timers (DMA) | 1 PWMC module | -4 |
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
| PWM Output Channels | 9 | 4 | **-5 channels** |
| PWM Input/Capture | Yes (TIM1_CH2) | Reserved only | **Not implemented** |
| Dedicated PWMC | No (uses TIM) | Yes (PWM0) | Different HW |
| DMA for PWM | Yes (TIM5, TIM4) | No | **Critical for DShot** |
| OneShot125/42 | Yes | No | **Missing** |
| DShot150-1200 | Yes (DMA burst) | No | **Critical missing** |
| Glitch-free Update | Yes | Yes (CDTYUPD) | Equal |

### 1.3 IO Timer Driver Comparison

**STM32 io_timer.c (1158 lines):**
```
Functions implemented:
- io_timer_allocate_timer()      - Timer resource management
- io_timer_allocate_channel()    - Channel allocation tracking
- io_timer_handler0-7()          - Per-timer IRQ handlers
- io_timer_set_dshot_burst_mode()- DShot with DMA burst
- io_timer_set_dshot_capture_mode() - Bidirectional DShot
- io_timer_update_dma_req()      - DMA request control
- io_timer_capture_dma_req()     - Capture DMA
- io_timer_trigger()             - OneShot trigger
- io_timer_set_oneshot_mode()    - OneShot mode setup
- io_timer_channel_init()        - Full mode support
- io_timer_set_enable()          - Multi-mode enable
- io_timer_set_ccr()             - Duty cycle update
```

**SAMV7 io_timer_pwmc.c (518 lines):**
```
Functions implemented:
- io_timer_init_timer()          - Basic PWMC init
- io_timer_channel_init()        - PWM output only
- io_timer_set_rate()            - Frequency setting
- io_timer_set_ccr()             - Duty cycle update
- io_timer_set_enable()          - Basic enable/disable
- io_timer_get_group()           - Channel grouping

Functions MISSING:
- io_timer_allocate_timer()      ❌ No resource tracking
- io_timer_allocate_channel()    ❌ No allocation tracking
- io_timer_handler*()            ❌ No IRQ handlers
- io_timer_set_dshot_*()         ❌ No DShot support
- io_timer_*_dma_*()             ❌ No DMA support
- io_timer_trigger()             ❌ No OneShot trigger
- io_timer_set_oneshot_mode()    ❌ No OneShot mode
- Capture mode                   ❌ No PWM input capture
```

---

## 2. Pin Configuration Comparison

### 2.1 PWM Output Pins

**FMUv6X (9 channels on dedicated connectors):**
| Channel | Timer | GPIO | Connector |
|---------|-------|------|-----------|
| FMU_CH1 | TIM5_CH4 | PI0 | FMU PWM |
| FMU_CH2 | TIM5_CH3 | PH12 | FMU PWM |
| FMU_CH3 | TIM5_CH2 | PH11 | FMU PWM |
| FMU_CH4 | TIM5_CH1 | PH10 | FMU PWM |
| FMU_CH5 | TIM4_CH2 | PD13 | FMU PWM |
| FMU_CH6 | TIM4_CH3 | PD14 | FMU PWM |
| FMU_CH7 | TIM12_CH1 | PH6 | FMU PWM |
| FMU_CH8 | TIM12_CH2 | PH9 | FMU PWM |
| FMU_CAP1 | TIM1_CH2 | PE11 | Capture |

**SAMV71-XULT (4 channels on extension headers):**
| Channel | PWM Module | GPIO | Header | Notes |
|---------|------------|------|--------|-------|
| Motor 1 | PWM0_CH3 | **PC13** | EXT2 Pin 4 | Fixed from PA7/XIN32 conflict |
| Motor 2 | PWM0_CH1 | PA2 | EXT2 Pin 9 | |
| Motor 3 | PWM0_CH2 | PC19 | EXT2 Pin 7 | Also mikroBUS1 PWM |
| Motor 4 | PWM0_CH0 | PB0 | EXT1 Pin 13 | Was mikroBUS2 RST |

### 2.2 Critical Pin Conflicts (SAMV71)

| Pin | Intended Use | Conflict | Status |
|-----|--------------|----------|--------|
| PA7 | Motor 1 PWM | XIN32 (32.768kHz crystal) | **FIXED** → PC13 |
| PA9 | Safety Button | UART0 RX | **Review needed** |
| PB0 | Motor 4 PWM | mikroBUS2 RST | Repurposed |
| PD18 | SD Card Detect | Potential RC | **Review needed** |

---

## 3. Feature Gap Analysis

### 3.1 Motor Protocol Support

| Protocol | FMUv6X | SAMV71-XULT | Required For |
|----------|--------|-------------|--------------|
| Standard PWM (50-400Hz) | ✅ | ✅ | All ESCs |
| PWM (400-500Hz) | ✅ | ✅ | Fast ESCs |
| OneShot125 | ✅ | ❌ | Racing quads |
| OneShot42 | ✅ | ❌ | Racing quads |
| DShot150 | ✅ | ❌ | Modern ESCs |
| DShot300 | ✅ | ❌ | Modern ESCs |
| DShot600 | ✅ | ❌ | High-performance |
| DShot1200 | ✅ | ❌ | Highest performance |
| Bidirectional DShot | ✅ | ❌ | RPM feedback |

**Impact:** SAMV71 cannot use modern DShot ESCs, limiting it to legacy PWM ESCs only.

### 3.2 Safety System Comparison

| Feature | FMUv6X | SAMV71-XULT | Notes |
|---------|--------|-------------|-------|
| Safety Switch | PF5 (dedicated) | PA9 (SW0) | Shared button |
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
| ADC Channels | 9+ | 2 | -7+ channels |
| IMU Redundancy | 3x | 1x | No redundancy |
| Barometer | 2x | 1x | No redundancy |
| Magnetometer | Internal + External | External only | Partial |

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
| DShot | Full (DMA) | Not impl. | ❌ |
| ADC | Full (9 ch) | Basic (2 ch) | **22% complete** |
| SPI | Full (6 buses) | Basic (1 bus) | **17% complete** |
| I2C | Full (4 buses) | Basic (1 bus) | **25% complete** |
| SDIO/HSMCI | Full | Basic | **80% complete** |
| USB | Full (OTG HS) | Basic (Device) | **60% complete** |
| Ethernet | Full | None | ❌ |
| CAN | Full (FD) | Not tested | Unknown |

### 4.2 Missing PX4 Features

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
| All PWM protocols | ❌ | Only standard PWM |
| Sensor redundancy | ❌ | Single sensors |
| Failsafe tested | ⚠️ | Basic testing |
| EKF2 validated | ⚠️ | Limited testing |
| Flight tested | ❌ | Not yet |
| Certification data | ❌ | None |

### 5.3 Missing for Production

**Critical (Must Have):**
1. ❌ DShot support with DMA
2. ❌ PWM capture for RC input
3. ❌ Sensor redundancy
4. ❌ Parameter EEPROM/FRAM backup
5. ❌ Verified flight testing

**Important (Should Have):**
1. ⚠️ OneShot125/42 support
2. ⚠️ Power brick validation
3. ⚠️ SD card power control
4. ⚠️ More ADC channels
5. ⚠️ LED strip support

**Nice to Have:**
1. Camera trigger
2. Ethernet
3. Multiple I2C buses
4. Trace debugging

---

## 6. Effort Estimation for Production Parity

### 6.1 High Priority Items

| Item | Complexity | Estimated Effort | Dependencies |
|------|------------|------------------|--------------|
| DShot with DMA | High | 40-60 hours | XDMAC driver |
| PWM Capture | Medium | 16-24 hours | TC driver |
| OneShot modes | Medium | 8-16 hours | Timer rework |
| QSPI Flash storage | Medium | 16-24 hours | MTD driver |
| Additional ADC | Low | 4-8 hours | AFEC config |

### 6.2 Medium Priority Items

| Item | Complexity | Estimated Effort | Dependencies |
|------|------------|------------------|--------------|
| Second I2C bus | Low | 4-8 hours | Board rework |
| Safety system review | Low | 4-8 hours | Pin reassign |
| Power monitoring | Medium | 8-16 hours | ADC + GPIO |
| Buzzer/Tone alarm | Low | 4-8 hours | PWM config |

### 6.3 Total Effort for Feature Parity

| Category | Hours | Cost @ $150/hr |
|----------|-------|----------------|
| Critical Features | 80-120 | $12,000-18,000 |
| Important Features | 40-60 | $6,000-9,000 |
| Nice to Have | 20-40 | $3,000-6,000 |
| **Total** | **140-220** | **$21,000-33,000** |

---

## 7. Recommendations

### 7.1 For Evaluation/Prototyping (Current State)

The SAMV71-XULT is **suitable** for:
- PX4 porting feasibility assessment
- Basic flight controller testing with PWM ESCs
- Sensor integration development
- Ground vehicle/rover applications
- Educational purposes

### 7.2 For Production Development

To use SAMV7 in production, prioritize:

1. **Custom PCB Design** - The XULT dev board has fundamental limitations
2. **DShot Implementation** - Required for modern ESCs
3. **Sensor Redundancy** - Add IMU/Baro redundancy
4. **EEPROM/FRAM** - Add reliable parameter storage
5. **Flight Testing** - Extensive validation required

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
| timer_config.cpp | 81 lines | 85 lines |
| init.cpp | 148 lines | ~150 lines |
| spi.cpp | 395 lines | ~200 lines |
| io_timer.c | 1158 lines | 518 lines |

### B. Defconfig Comparison

Key NuttX configuration differences:
```
FMUv6X:                          SAMV71:
CONFIG_STM32H7_TIM5=y            CONFIG_SAMV7_PWM0=y
CONFIG_STM32H7_TIM4=y            # No TIM equivalent
CONFIG_STM32H7_TIM12=y           # No TIM equivalent
CONFIG_STM32H7_DMA1=y            CONFIG_SAMV7_XDMAC=y
CONFIG_STM32H7_SDMMC1=y          CONFIG_SAMV7_HSMCI0=y
CONFIG_STM32H7_ETHMAC=y          # No Ethernet
```

### C. References

- STM32H753 Reference Manual (RM0433)
- SAMV71 Datasheet (DS60001527)
- PX4 Autopilot Documentation
- SAMV71-XULT User Guide

---

*Document Version: 1.0*
*Date: 2025*
*Author: Claude Code Assistant*

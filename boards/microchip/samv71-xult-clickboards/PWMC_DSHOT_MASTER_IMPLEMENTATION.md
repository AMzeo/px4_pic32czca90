# SAMV71 PWMC + DShot Master Implementation Guide

**Created:** January 2026
**Status:** READY FOR IMPLEMENTATION
**Priority:** HIGH (Required for flight capability)
**Estimated Effort:** Phase 1: 3-5 days, Phase 2: 5-7 days

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [Hardware Configuration](#3-hardware-configuration)
4. [Phase 1: PWMC Basic PWM](#4-phase-1-pwmc-basic-pwm)
5. [Phase 2: DShot Protocol](#5-phase-2-dshot-protocol)
6. [Complete Code Implementation](#6-complete-code-implementation)
7. [Build Configuration](#7-build-configuration)
8. [Testing Procedures](#8-testing-procedures)
9. [Troubleshooting](#9-troubleshooting)
10. [References](#10-references)

---

## 1. Executive Summary

### 1.1 Current State

| Component | Status | Issue |
|-----------|--------|-------|
| PWM Backend | TC-based | Only 3 channels (PA26 conflict) |
| PWMOut Driver | DISABLED | Crashes on startup |
| DShot | Not Implemented | Requires PWMC + DMA |
| Motor Testing | HITL Only | No real motor output |

### 1.2 Implementation Goals

| Phase | Goal | Channels | Protocol |
|-------|------|----------|----------|
| Phase 1 | Basic PWMC PWM | 4 | Standard 400Hz PWM |
| Phase 2 | DShot via DMA | 4 | DShot150/300/600 |
| Phase 3 | Bidirectional (optional) | 4 | eRPM telemetry |

### 1.3 Selected Pin Configuration (Pin Set A)

| Motor | PWMC | Channel | Pin | Location | Peripheral |
|-------|------|---------|-----|----------|------------|
| 1 | PWM0 | CH3 | PA7 | Arduino A1 | Peripheral B |
| 2 | PWM0 | CH1 | PA2 | EXT2 pin 9 | Peripheral A |
| 3 | PWM0 | CH2 | PC19 | mikroBUS1 PWM | Peripheral B |
| 4 | PWM1 | CH1 | PA14 | EXT2 pin 8 | Peripheral C |

**Critical Pins AVOIDED:**
- PA26 - HSMCI0 DA2 (SD card corruption!)
- PA0 - mikroBUS1 INT (preserve for sensors)
- PA3/PA4 - I2C0 bus

---

## 2. Architecture Overview

### 2.1 SAMV71 PWMC Peripheral

The SAMV71 has two PWM Controller instances:

```
PWM0: Base 0x40020000 - 4 channels (CH0-CH3)
PWM1: Base 0x4005C000 - 4 channels (CH0-CH3)

Each channel provides:
├── PWMH (High) output - Active high signal
├── PWML (Low) output - Complementary (inverted)
├── Independent period (CPRD) and duty (CDTY)
├── Dead-time generator
└── Shared DMA per PWMC instance
```

### 2.2 PWMC Register Map (from Harmony CSP analysis)

```c
/* Global Registers */
#define PWM_CLK_OFFSET      0x000   /* Clock Register */
#define PWM_ENA_OFFSET      0x004   /* Enable Register */
#define PWM_DIS_OFFSET      0x008   /* Disable Register */
#define PWM_SR_OFFSET       0x00C   /* Status Register */
#define PWM_SCM_OFFSET      0x020   /* Sync Channels Mode */
#define PWM_DMAR_OFFSET     0x024   /* DMA Register (multi-ch updates) */
#define PWM_SCUC_OFFSET     0x028   /* Sync Update Control */
#define PWM_SCUP_OFFSET     0x02C   /* Sync Update Period */

/* Channel Registers (base + 0x200 + channel * 0x20) */
#define PWM_CMR_OFFSET      0x00    /* Channel Mode Register */
#define PWM_CDTY_OFFSET     0x04    /* Duty Cycle */
#define PWM_CDTYUPD_OFFSET  0x08    /* Duty Cycle Update (buffered) */
#define PWM_CPRD_OFFSET     0x0C    /* Period */
#define PWM_CPRDUPD_OFFSET  0x10    /* Period Update (buffered) */
#define PWM_DT_OFFSET       0x18    /* Dead Time */
#define PWM_DTUPD_OFFSET    0x1C    /* Dead Time Update */

/* Comparison Units (base + 0x130 + unit * 0x10) */
#define PWM_CMPM_OFFSET     0x00    /* Comparison Mode */
#define PWM_CMPV_OFFSET     0x04    /* Comparison Value */
#define PWM_CMPVUPD_OFFSET  0x08    /* Comparison Value Update */

/* Event Line Multiplexer */
#define PWM_ELMR0_OFFSET    0x07C   /* Event Line 0 Mapping */
#define PWM_ELMR1_OFFSET    0x080   /* Event Line 1 Mapping */
```

### 2.3 CMR Register Bits

```c
/* Channel Mode Register (PWM_CMR) */
#define PWM_CMR_CPRE_MASK   0x0F    /* Clock Prescaler */
#define PWM_CMR_CPRE_MCK    0       /* MCK */
#define PWM_CMR_CPRE_MCK2   1       /* MCK/2 */
#define PWM_CMR_CPRE_MCK4   2       /* MCK/4 */
#define PWM_CMR_CPRE_MCK8   3       /* MCK/8 */
#define PWM_CMR_CPRE_MCK16  4       /* MCK/16 */
#define PWM_CMR_CPRE_MCK32  5       /* MCK/32 */
#define PWM_CMR_CPRE_MCK64  6       /* MCK/64 */
#define PWM_CMR_CPRE_MCK128 7       /* MCK/128 */
#define PWM_CMR_CPRE_MCK256 8       /* MCK/256 */
#define PWM_CMR_CPRE_MCK512 9       /* MCK/512 */
#define PWM_CMR_CPRE_MCK1024 10     /* MCK/1024 */
#define PWM_CMR_CPRE_CLKA   11      /* CLKA */
#define PWM_CMR_CPRE_CLKB   12      /* CLKB */
#define PWM_CMR_CALG        (1 << 8)  /* Center Aligned */
#define PWM_CMR_CPOL        (1 << 9)  /* Channel Polarity */
#define PWM_CMR_CES         (1 << 10) /* Counter Event Selection */
#define PWM_CMR_UPDS        (1 << 11) /* Update Selection */
#define PWM_CMR_DPOLI       (1 << 12) /* Dead-Time PWML Polarity Inverted */
#define PWM_CMR_TCTS        (1 << 13) /* Timer Counter Trigger Selection */
#define PWM_CMR_DTE         (1 << 16) /* Dead-Time Enable */
#define PWM_CMR_DTHI        (1 << 17) /* Dead-Time PWMH Inverted */
#define PWM_CMR_DTLI        (1 << 18) /* Dead-Time PWML Inverted */
```

### 2.4 Sync Channels Mode (PWM_SCM)

```c
/* PWM_SCM - Synchronous Channels Mode Register */
#define PWM_SCM_SYNC0       (1 << 0)  /* Sync Channel 0 */
#define PWM_SCM_SYNC1       (1 << 1)  /* Sync Channel 1 */
#define PWM_SCM_SYNC2       (1 << 2)  /* Sync Channel 2 */
#define PWM_SCM_SYNC3       (1 << 3)  /* Sync Channel 3 */
#define PWM_SCM_UPDM_SHIFT  16        /* Update Mode */
#define PWM_SCM_UPDM_MODE0  (0 << 16) /* Manual (SCUC.UPDULOCK) */
#define PWM_SCM_UPDM_MODE1  (1 << 16) /* Auto immediate */
#define PWM_SCM_UPDM_MODE2  (2 << 16) /* Auto on period */
#define PWM_SCM_PTRM        (1 << 20) /* DMA Transfer Request Mode */
#define PWM_SCM_PTRCS_SHIFT 21        /* Comparison for DMA trigger */
```

### 2.5 DShot vs STM32 Comparison

| Aspect | STM32 Approach | SAMV7 Approach |
|--------|----------------|----------------|
| DMA Trigger | Timer UPDATE event | PWMC period end or comparison |
| Multi-Channel | Timer burst to CCR1-4 via DMAR | PWMC DMAR auto-distributes to sync channels |
| Buffer Layout | Interleaved [Ch0b0][Ch1b0]... | Same interleaved format |
| Capture (BDSHOT) | Timer input capture | TC capture or PWMC fault input |
| Sync Start | Single enable bit | Critical section enable both PWMC |

**Key Insight:** SAMV7 PWMC has native sync update (PWM_SCM) which simplifies multi-channel atomic updates compared to STM32.

---

## 3. Hardware Configuration

### 3.1 Physical Connections

```
SAMV71-XULT Board - Motor Output Locations
==========================================

Arduino Header:
┌─────────────────────────────────────┐
│ A1 (PA7)  ← Motor 1 (PWMC0 CH3)    │
└─────────────────────────────────────┘

EXT2 Header:
┌─────────────────────────────────────┐
│ Pin 8 (PA14) ← Motor 4 (PWMC1 CH1) │
│ Pin 9 (PA2)  ← Motor 2 (PWMC0 CH1) │
└─────────────────────────────────────┘

mikroBUS Socket 1:
┌─────────────────────────────────────┐
│ PWM (PC19) ← Motor 3 (PWMC0 CH2)   │
│ INT (PA0)  - FREE for sensors      │
└─────────────────────────────────────┘
```

### 3.2 GPIO Peripheral Function Selection

From SAMV71 datasheet pin multiplexing table:

| Pin | Function | Peripheral | GPIO Config |
|-----|----------|------------|-------------|
| PA7 | PWM0_H3 | Peripheral B | `GPIO_PERIPHB \| GPIO_PORT_PIOA \| GPIO_PIN7` |
| PA2 | PWM0_H1 | Peripheral A | `GPIO_PERIPHA \| GPIO_PORT_PIOA \| GPIO_PIN2` |
| PC19 | PWM0_H2 | Peripheral B | `GPIO_PERIPHB \| GPIO_PORT_PIOC \| GPIO_PIN19` |
| PA14 | PWM1_H1 | Peripheral C | `GPIO_PERIPHC \| GPIO_PORT_PIOA \| GPIO_PIN14` |

### 3.3 Clock Configuration

```
SAMV71 MCK = 150 MHz

Standard PWM (400 Hz):
  Prescaler: MCK/32 (CPRE=5) → 4.6875 MHz
  Period: 4687500 / 400 = 11718 ticks
  Duty 1000µs: 1000 × 4.6875 = 4687 ticks
  Duty 1500µs: 1500 × 4.6875 = 7031 ticks
  Duty 2000µs: 2000 × 4.6875 = 9375 ticks

DShot600:
  Prescaler: MCK/8 (CPRE=3) → 18.75 MHz
  Bit period: 1.67µs × 18.75MHz = 31 ticks
  T0H (37.5%): 31 × 0.375 = 12 ticks
  T1H (75.0%): 31 × 0.750 = 23 ticks

DShot300:
  Bit period: 3.33µs × 18.75MHz = 62 ticks
  T0H: 23 ticks, T1H: 47 ticks

DShot150:
  Bit period: 6.67µs × 18.75MHz = 125 ticks
  T0H: 47 ticks, T1H: 94 ticks
```

---

## 4. Phase 1: PWMC Basic PWM

### 4.1 Files to Create/Modify

```
CREATE:
  platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/pwmc.h

MODIFY:
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h
  platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt
  boards/microchip/samv71-xult-clickboards/src/board_config.h
  boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
  boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig
  boards/microchip/samv71-xult-clickboards/default.px4board
```

### 4.2 NuttX defconfig Changes

```diff
# boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig

# Enable PWMC peripherals
+CONFIG_SAMV7_PWM0=y
+CONFIG_SAMV7_PWM0_CH1=y
+CONFIG_SAMV7_PWM0_CH2=y
+CONFIG_SAMV7_PWM0_CH3=y
+CONFIG_SAMV7_PWM1=y
+CONFIG_SAMV7_PWM1_CH1=y

# Keep TC0 for HRT only
CONFIG_SAMV7_TC0=y

# Remove TC channels no longer needed for PWM
-CONFIG_SAMV7_TC1=y
-CONFIG_SAMV7_TC3=y
```

### 4.3 board_config.h Changes

```c
/* boards/microchip/samv71-xult-clickboards/src/board_config.h */

/*
 * PWMC Motor Output Configuration
 *
 * Pin Set A:
 *   Motor 1: PWM0 CH3 -> PA7  (Peripheral B) - Arduino A1
 *   Motor 2: PWM0 CH1 -> PA2  (Peripheral A) - EXT2 pin 9
 *   Motor 3: PWM0 CH2 -> PC19 (Peripheral B) - mikroBUS1 PWM
 *   Motor 4: PWM1 CH1 -> PA14 (Peripheral C) - EXT2 pin 8
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  4

/* Enable PWMC backend selection */
#define BOARD_IO_TIMER_PWMC         1

/* PWMC GPIO configurations */
#define GPIO_PWM_MOTOR1  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)
#define GPIO_PWM_MOTOR2  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)
#define GPIO_PWM_MOTOR3  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)
#define GPIO_PWM_MOTOR4  (GPIO_PERIPHC | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN14)
```

---

## 5. Phase 2: DShot Protocol

### 5.1 DShot Frame Format

```
DShot Frame (16 bits):
┌────────────────────────────────────────────────────────┐
│  11-bit Throttle  │ Telemetry │   4-bit CRC           │
│   (0-2047)        │   (1 bit) │   (XOR checksum)      │
└────────────────────────────────────────────────────────┘
     Bits 15-5          Bit 4        Bits 3-0

Special Commands (throttle 0-47):
  0: Motor stop
  1-5: Beep (1-5 times)
  6: ESC info request
  7-11: Rotation direction
  12: Save settings
  ...
  48-2047: Throttle (48 = 0%, 2047 = 100%)
```

### 5.2 DShot Bit Encoding

```
Each bit is a PWM pulse within the bit period:

Bit '1' (75% duty):  ████████████████████░░░░░░░
Bit '0' (37.5% duty): ████████░░░░░░░░░░░░░░░░░░
                      ←─────── Bit Period ───────→

DShot600: Bit period = 1.67µs
  T1H = 1.25µs (75%)
  T0H = 0.625µs (37.5%)

Frame time = 16 bits × 1.67µs = 26.7µs
Max frame rate = ~37 kHz (typically run at ~8-16 kHz)
```

### 5.3 DMA Architecture

```
                    ┌──────────────────────────────────┐
                    │         PX4 DShot Driver          │
                    │  up_dshot_motor_data_set(M1-M4)  │
                    └──────────┬───────────────────────┘
                               │
             ┌─────────────────┴─────────────────────┐
             │                                       │
             ▼                                       ▼
   ┌─────────────────────┐               ┌─────────────────────┐
   │   PWM0 DMA Buffer   │               │   PWM1 DMA Buffer   │
   │   Motors 1,2,3      │               │   Motor 4           │
   │   [51 × uint32_t]   │               │   [17 × uint32_t]   │
   │   Interleaved:      │               │   Linear:           │
   │   [M1b15][M2b15]    │               │   [M4b15][M4b14]... │
   │   [M3b15][M1b14]... │               │                     │
   └──────────┬──────────┘               └──────────┬──────────┘
              │                                     │
              ▼                                     ▼
   ┌─────────────────────┐               ┌─────────────────────┐
   │    XDMAC Channel    │               │    XDMAC Channel    │
   │  Periph ID = 13     │               │  Periph ID = 39     │
   │  (PWM0_TX)          │               │  (PWM1_TX)          │
   └──────────┬──────────┘               └──────────┬──────────┘
              │                                     │
              ▼                                     ▼
   ┌─────────────────────┐               ┌─────────────────────┐
   │       PWM0          │               │       PWM1          │
   │    PWM_DMAR         │               │   CH1_CDTYUPD       │
   │  (auto-distributes  │               │                     │
   │   to sync CH1,2,3)  │               │                     │
   └─────────┬───────────┘               └──────────┬──────────┘
             │                                      │
   ┌─────────┼─────────┐                           │
   ▼         ▼         ▼                           ▼
 PA2(M2)  PC19(M3)  PA7(M1)                    PA14(M4)
```

### 5.4 PWM0 Synchronous Mode Configuration

```c
/*
 * PWM0 Sync Mode Setup for Multi-Channel DMA
 *
 * The PWM_DMAR register accepts duty values that are automatically
 * distributed to synchronized channels in order (CH1, CH2, CH3).
 */
static void pwm0_configure_sync_mode(void)
{
    uint32_t scm = 0;

    /* Enable sync for channels 1, 2, 3 (used for motors 2, 3, 1) */
    scm |= PWM_SCM_SYNC1 | PWM_SCM_SYNC2 | PWM_SCM_SYNC3;

    /* Auto update on period end */
    scm |= PWM_SCM_UPDM_MODE2;

    /* DMA request on period end (not comparison match) */
    /* PTRM = 0 means period trigger */

    putreg32(scm, SAM_PWM0_BASE + PWM_SCM_OFFSET);
}
```

### 5.5 DShot Packet Encoding

```c
/*
 * Encode DShot packet and fill DMA buffer
 *
 * Based on STM32 implementation pattern
 */
static void dshot_encode_packet(uint8_t motor, uint16_t throttle, bool telemetry)
{
    uint16_t packet = 0;
    uint16_t checksum = 0;

    /* Build packet: [Throttle:11][Telemetry:1][Checksum:4] */
    packet = (throttle << 5) | ((telemetry ? 1 : 0) << 4);

    /* Calculate XOR checksum over upper 12 bits */
    uint16_t csum_data = packet >> 4;
    for (int i = 0; i < 3; i++) {
        checksum ^= (csum_data & 0x0F);
        csum_data >>= 4;
    }

    /* For bidirectional DShot, invert checksum */
    if (g_bidirectional) {
        checksum = (~checksum) & 0x0F;
    }

    packet |= checksum;

    /* Fill DMA buffer with PWM duty values for each bit */
    uint32_t *buffer = get_motor_buffer(motor);
    int stride = get_motor_stride(motor);

    for (int bit = 0; bit < 16; bit++) {
        uint32_t duty = (packet & 0x8000) ? DSHOT_T1H : DSHOT_T0H;
        buffer[bit * stride] = duty;
        packet <<= 1;
    }

    /* Add reset pulse (0 duty) */
    buffer[16 * stride] = 0;
}
```

---

## 6. Complete Code Implementation

### 6.1 io_timer_pwmc.c

```c
/**
 * @file io_timer_pwmc.c
 *
 * SAMV7 IO Timer implementation using PWMC peripheral.
 *
 * Supports both standard PWM (Phase 1) and DShot (Phase 2).
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>

#include <drivers/drv_pwm_output.h>
#include <px4_arch/io_timer.h>
#include <board_config.h>

#include "arm_internal.h"
#include "sam_gpio.h"
#include "sam_periphclks.h"

/*
 * PWMC Base Addresses
 */
#define SAM_PWM0_BASE       0x40020000
#define SAM_PWM1_BASE       0x4005C000

/*
 * Register Offsets
 */
#define PWM_CLK_OFFSET      0x000
#define PWM_ENA_OFFSET      0x004
#define PWM_DIS_OFFSET      0x008
#define PWM_SR_OFFSET       0x00C
#define PWM_SCM_OFFSET      0x020
#define PWM_DMAR_OFFSET     0x024
#define PWM_SCUC_OFFSET     0x028

#define PWM_CH_BASE         0x200
#define PWM_CH_SIZE         0x020
#define PWM_CMR_CH_OFFSET   0x00
#define PWM_CDTY_CH_OFFSET  0x04
#define PWM_CDTYUPD_OFFSET  0x08
#define PWM_CPRD_CH_OFFSET  0x0C
#define PWM_CPRDUPD_OFFSET  0x10

/*
 * CMR Bits
 */
#define PWM_CMR_CPRE_MCK32  5
#define PWM_CMR_CPRE_MCK8   3
#define PWM_CMR_CPOL        (1 << 9)

/*
 * Clock Configuration
 */
#define SAMV7_MCK_FREQ      150000000UL

/* Standard PWM: MCK/32 = 4.6875 MHz */
#define PWM_STD_PRESCALER   PWM_CMR_CPRE_MCK32
#define PWM_STD_CLOCK       (SAMV7_MCK_FREQ / 32)
#define PWM_DEFAULT_RATE    400
#define PWM_DEFAULT_PERIOD  (PWM_STD_CLOCK / PWM_DEFAULT_RATE)

/* DShot: MCK/8 = 18.75 MHz */
#define DSHOT_PRESCALER     PWM_CMR_CPRE_MCK8
#define DSHOT_CLOCK         (SAMV7_MCK_FREQ / 8)

/*
 * State
 */
static bool g_pwmc_initialized[2] = {false, false};
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static uint32_t g_channel_period[MAX_TIMER_IO_CHANNELS];

/*
 * Motor to PWMC mapping
 */
typedef struct {
    uint32_t base;          /* PWMC base address */
    uint8_t  channel;       /* PWMC channel (0-3) */
    uint32_t gpio;          /* GPIO configuration */
} motor_config_t;

static const motor_config_t motor_config[4] = {
    { SAM_PWM0_BASE, 3, GPIO_PWM_MOTOR1 },  /* Motor 1: PWM0 CH3, PA7 */
    { SAM_PWM0_BASE, 1, GPIO_PWM_MOTOR2 },  /* Motor 2: PWM0 CH1, PA2 */
    { SAM_PWM0_BASE, 2, GPIO_PWM_MOTOR3 },  /* Motor 3: PWM0 CH2, PC19 */
    { SAM_PWM1_BASE, 1, GPIO_PWM_MOTOR4 },  /* Motor 4: PWM1 CH1, PA14 */
};

/*
 * Register Access Helpers
 */
static inline void pwmc_putreg(uint32_t base, uint32_t offset, uint32_t value)
{
    putreg32(value, base + offset);
}

static inline uint32_t pwmc_getreg(uint32_t base, uint32_t offset)
{
    return getreg32(base + offset);
}

static inline uint32_t get_channel_reg(uint32_t base, uint8_t channel, uint32_t offset)
{
    return base + PWM_CH_BASE + (channel * PWM_CH_SIZE) + offset;
}

/*
 * Enable PWMC peripheral clock
 */
static void pwmc_enable_clock(uint32_t base)
{
    if (base == SAM_PWM0_BASE) {
        sam_pwm0_enableclk();
    } else if (base == SAM_PWM1_BASE) {
        sam_pwm1_enableclk();
    }
}

/*
 * Initialize PWMC instance
 */
int io_timer_init_timer(unsigned timer)
{
    if (timer >= MAX_IO_TIMERS) {
        return -EINVAL;
    }

    uint32_t base = io_timers[timer].base;
    if (base == 0) {
        return -EINVAL;
    }

    int instance = (base == SAM_PWM0_BASE) ? 0 : 1;

    if (!g_pwmc_initialized[instance]) {
        pwmc_enable_clock(base);
        g_pwmc_initialized[instance] = true;
    }

    return OK;
}

/*
 * Initialize a PWM channel
 */
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
                          channel_handler_t handler, void *context)
{
    (void)handler;
    (void)context;

    if (channel >= MAX_TIMER_IO_CHANNELS || channel >= 4) {
        return -EINVAL;
    }

    if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_NotUsed) {
        return -EINVAL;
    }

    const motor_config_t *cfg = &motor_config[channel];

    /* Enable PWMC clock if needed */
    int instance = (cfg->base == SAM_PWM0_BASE) ? 0 : 1;
    if (!g_pwmc_initialized[instance]) {
        pwmc_enable_clock(cfg->base);
        g_pwmc_initialized[instance] = true;
    }

    if (mode == IOTimerChanMode_PWMOut) {
        /* Configure GPIO for PWMC */
        sam_configgpio(cfg->gpio);

        /* Disable channel first */
        pwmc_putreg(cfg->base, PWM_DIS_OFFSET, (1 << cfg->channel));

        /* Configure channel mode:
         * - Prescaler MCK/32 for standard PWM
         * - Left-aligned
         * - Output starts low, goes high on match
         */
        uint32_t cmr = PWM_STD_PRESCALER;
        putreg32(cmr, get_channel_reg(cfg->base, cfg->channel, PWM_CMR_CH_OFFSET));

        /* Set default period (400 Hz) */
        putreg32(PWM_DEFAULT_PERIOD, get_channel_reg(cfg->base, cfg->channel, PWM_CPRD_CH_OFFSET));
        g_channel_period[channel] = PWM_DEFAULT_PERIOD;

        /* Set initial duty to disarm (900 µs) */
        uint32_t duty = (900 * PWM_STD_CLOCK) / 1000000UL;
        putreg32(duty, get_channel_reg(cfg->base, cfg->channel, PWM_CDTY_CH_OFFSET));

        /* Enable channel */
        pwmc_putreg(cfg->base, PWM_ENA_OFFSET, (1 << cfg->channel));
    }

    g_channel_modes[channel] = mode;
    return OK;
}

/*
 * Set PWM duty cycle (pulse width in microseconds)
 */
int io_timer_set_ccr(unsigned channel, uint16_t value)
{
    if (channel >= MAX_TIMER_IO_CHANNELS || channel >= 4) {
        return -EINVAL;
    }

    if (g_channel_modes[channel] != IOTimerChanMode_PWMOut) {
        return -EINVAL;
    }

    const motor_config_t *cfg = &motor_config[channel];

    /* Convert microseconds to ticks */
    uint32_t ticks = ((uint32_t)value * PWM_STD_CLOCK) / 1000000UL;

    /* Clamp to period */
    if (ticks > g_channel_period[channel]) {
        ticks = g_channel_period[channel];
    }

    /* Use update register for glitch-free update */
    putreg32(ticks, get_channel_reg(cfg->base, cfg->channel, PWM_CDTYUPD_OFFSET));

    return OK;
}

/*
 * Set PWM rate (frequency) for all channels
 */
int io_timer_set_rate(unsigned timer, unsigned rate)
{
    if (rate < 50 || rate > 8000) {
        return -EINVAL;
    }

    uint32_t period = PWM_STD_CLOCK / rate;

    /* Update all channels */
    for (unsigned ch = 0; ch < 4; ch++) {
        if (g_channel_modes[ch] == IOTimerChanMode_PWMOut) {
            const motor_config_t *cfg = &motor_config[ch];
            putreg32(period, get_channel_reg(cfg->base, cfg->channel, PWM_CPRDUPD_OFFSET));
            g_channel_period[ch] = period;
        }
    }

    return OK;
}

/*
 * Enable/disable PWM output
 */
int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
                        io_timer_channel_allocation_t masks)
{
    if (mode != IOTimerChanMode_PWMOut) {
        return -EINVAL;
    }

    for (unsigned ch = 0; ch < 4; ch++) {
        if (masks & (1 << ch)) {
            if (g_channel_modes[ch] != IOTimerChanMode_PWMOut) {
                continue;
            }

            const motor_config_t *cfg = &motor_config[ch];

            if (state) {
                pwmc_putreg(cfg->base, PWM_ENA_OFFSET, (1 << cfg->channel));
            } else {
                pwmc_putreg(cfg->base, PWM_DIS_OFFSET, (1 << cfg->channel));
            }
        }
    }

    return OK;
}

/*
 * Get channel group bitmask
 */
uint32_t io_timer_get_group(unsigned timer)
{
    (void)timer;
    /* All 4 channels belong to same "group" for our purposes */
    return 0x0F;
}

/*
 * Additional required io_timer API functions
 */
int io_timer_validate_channel_index(unsigned channel)
{
    return (channel < 4) ? 0 : -EINVAL;
}

int io_timer_is_channel_free(unsigned channel)
{
    if (channel >= 4) return -EINVAL;
    return (g_channel_modes[channel] == IOTimerChanMode_NotUsed) ? 0 : -EBUSY;
}

int io_timer_free_channel(unsigned channel)
{
    if (channel >= 4) return -EINVAL;
    g_channel_modes[channel] = IOTimerChanMode_NotUsed;
    return OK;
}

int io_timer_get_channel_mode(unsigned channel)
{
    if (channel >= 4) return IOTimerChanMode_NotUsed;
    return g_channel_modes[channel];
}

int io_timer_get_mode_channels(io_timer_channel_mode_t mode)
{
    int mask = 0;
    for (unsigned ch = 0; ch < 4; ch++) {
        if (g_channel_modes[ch] == mode) {
            mask |= (1 << ch);
        }
    }
    return mask;
}

uint16_t io_channel_get_ccr(unsigned channel)
{
    if (channel >= 4) return 0;

    const motor_config_t *cfg = &motor_config[channel];
    uint32_t ticks = getreg32(get_channel_reg(cfg->base, cfg->channel, PWM_CDTY_CH_OFFSET));

    return (uint16_t)((ticks * 1000000UL) / PWM_STD_CLOCK);
}

uint32_t io_timer_channel_get_as_pwm_input(unsigned channel)
{
    (void)channel;
    return 0;  /* PWMC does not support input capture */
}

int io_timer_unallocate_channel(unsigned channel)
{
    return io_timer_free_channel(channel);
}

int io_timer_set_pwm_rate(unsigned timer, unsigned rate)
{
    return io_timer_set_rate(timer, rate);
}

void io_timer_trigger(unsigned channels_mask)
{
    /* Updates applied automatically via CDTYUPD registers */
    (void)channels_mask;
}
```

### 6.2 timer_config.cpp

```cpp
/**
 * @file timer_config.cpp
 *
 * PWMC-based PWM configuration for SAMV71-XULT
 */

#include <px4_arch/io_timer_hw_description.h>

/*
 * IO Timer configuration
 *
 * We use 2 PWMC instances:
 *   io_timers[0] = PWM0 (Motors 1, 2, 3)
 *   io_timers[1] = PWM1 (Motor 4)
 */
constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimerPWMC(PWMC::PWM0),
    initIOTimerPWMC(PWMC::PWM1),
};

/*
 * Timer channel to motor mapping
 *
 * Index = Motor number - 1
 */
constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    /* Motor 1: PWM0 CH3 -> PA7 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH3, PWMC::PeriphB},
        {GPIO::PortA, GPIO::Pin7}),

    /* Motor 2: PWM0 CH1 -> PA2 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH1, PWMC::PeriphA},
        {GPIO::PortA, GPIO::Pin2}),

    /* Motor 3: PWM0 CH2 -> PC19 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH2, PWMC::PeriphB},
        {GPIO::PortC, GPIO::Pin19}),

    /* Motor 4: PWM1 CH1 -> PA14 */
    initIOTimerChannelPWMC(io_timers, 1,
        {PWMC::PWM1, PWMC::CH1, PWMC::PeriphC},
        {GPIO::PortA, GPIO::Pin14}),
};

constexpr io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

### 6.3 hw_description.h Addition

```cpp
/* Add to platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h */

/*
 * PWMC (PWM Controller) Namespace
 */
namespace PWMC
{

enum Instance {
    PWM0 = 0,
    PWM1 = 1,
};

enum Channel {
    CH0 = 0,
    CH1 = 1,
    CH2 = 2,
    CH3 = 3,
};

enum Peripheral {
    PeriphA = 0,
    PeriphB = 1,
    PeriphC = 2,
};

struct PWMChannel {
    Instance instance;
    Channel channel;
    Peripheral periph;
};

} // namespace PWMC
```

---

## 7. Build Configuration

### 7.1 default.px4board

```cmake
# Enable PWM output
CONFIG_DRIVERS_PWM_OUT=y

# For Phase 2: Enable DShot
# CONFIG_DRIVERS_DSHOT=y
```

### 7.2 CMakeLists.txt

```cmake
# platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt

if(CONFIG_DRIVERS_PWM_OUT OR CONFIG_DRIVERS_DSHOT)
    px4_add_library(arch_io_pins
        io_timer_pwmc.c
        pwm_servo.c
    )
else()
    px4_add_library(arch_io_pins
        io_timer_stub.c
    )
endif()
```

---

## 8. Testing Procedures

### 8.1 Phase 1: Basic PWM Tests

```bash
# Build
make microchip_samv71-xult-clickboards_default

# Flash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/.../firmware.bin 0x00400000 verify reset exit"

# Verify boot (no crash)
# Connect USB serial and check for shell prompt

# Check PWM driver
nsh> pwm_out status

# Expected output:
# pwm_out: running, 4 channels

# Test individual motors
nsh> actuator_test set -m 1 -v 0.0    # Motor 1 at 0% (disarm)
nsh> actuator_test set -m 1 -v 0.5    # Motor 1 at 50%
nsh> actuator_test set -m 1 -v 1.0    # Motor 1 at 100%

# Test all motors sequentially
nsh> actuator_test iterate-motors
```

### 8.2 Oscilloscope Verification

| Test Point | Pin | Expected |
|------------|-----|----------|
| Motor 1 | PA7 (Arduino A1) | 400 Hz, 900-2000 µs |
| Motor 2 | PA2 (EXT2 pin 9) | 400 Hz, 900-2000 µs |
| Motor 3 | PC19 (mikroBUS1 PWM) | 400 Hz, 900-2000 µs |
| Motor 4 | PA14 (EXT2 pin 8) | 400 Hz, 900-2000 µs |

**Measurements:**
- Frequency: 400 Hz ±1%
- Period: 2.5 ms
- Duty at 0%: ~900 µs
- Duty at 50%: ~1500 µs
- Duty at 100%: ~2000 µs

### 8.3 Regression Tests

```bash
# Verify SD card still works
nsh> param save
nsh> param load

# Verify sensors work
nsh> listener sensor_accel -n 1
nsh> listener sensor_baro -n 1

# Verify I2C bus
nsh> i2cdetect -b 1

# Verify SPI sensors
nsh> icm20689 status
nsh> bmp388 status
```

### 8.4 Phase 2: DShot Tests

```bash
# Start DShot driver
nsh> dshot start -m 600    # DShot600

# Check status
nsh> dshot status

# Test motors
nsh> actuator_test set -m 1 -v 0.5

# Oscilloscope verification:
# - Bit period: 1.67 µs (±5%)
# - T0H: ~0.625 µs (37.5%)
# - T1H: ~1.25 µs (75%)
# - Frame: 16 bits = ~27 µs
```

---

## 9. Troubleshooting

### 9.1 Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No PWM output | GPIO not configured | Verify sam_configgpio() called |
| Wrong frequency | Period register incorrect | Check CPRD calculation |
| Glitchy output | Not using update registers | Use CDTYUPD instead of CDTY |
| PWMOut crashes | Module init issue | Check MixingOutput, WorkQueue |
| SD card corruption | Using PA26 | Verify PA26 not in GPIO list |
| Sensors fail | I2C/SPI conflict | Verify no pin conflicts |

### 9.2 Debug Commands

```bash
# Check PWMC registers (requires debug build)
nsh> perf        # Performance counters
nsh> dmesg       # Kernel messages
nsh> top         # Task status

# GPIO state
nsh> gpio status

# Memory check
nsh> free
```

### 9.3 Known PWMOut Crash Issue

The PWMOut module has crashed previously due to:
1. WorkQueue initialization timing
2. MixingOutput configuration
3. DIRECT_PWM_OUTPUT_CHANNELS mismatch

**Workaround if crashes persist:**
- Use actuator_test directly
- Debug with --debug build
- Check stack traces in dmesg

---

## 10. References

### 10.1 Documentation

- SAMV71 Datasheet Chapter 47: PWM Controller
- SAMV71 Datasheet Chapter 16: XDMAC Controller
- Microchip Harmony CSP: `csp/peripheral/pwm_6343/`
- PX4 STM32 DShot: `platforms/nuttx/src/px4/stm/stm32_common/dshot/`

### 10.2 Key Files

```
NuttX:
  arch/arm/src/samv7/sam_pwm.c
  arch/arm/src/samv7/hardware/sam_pwm.h
  arch/arm/src/samv7/sam_xdmac.c
  arch/arm/src/samv7/hardware/sam_xdmac.h

PX4 SAMV7:
  platforms/nuttx/src/px4/microchip/samv7/io_pins/
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/

Board:
  boards/microchip/samv71-xult-clickboards/src/
  boards/microchip/samv71-xult-clickboards/nuttx-config/
```

### 10.3 Implementation Checklist

**Phase 1:**
- [ ] Add PWMC to NuttX defconfig
- [ ] Create io_timer_pwmc.c
- [ ] Update hw_description.h with PWMC namespace
- [ ] Update timer_config.cpp
- [ ] Update board_config.h
- [ ] Update CMakeLists.txt
- [ ] Build and test
- [ ] Oscilloscope verify all 4 channels
- [ ] Regression test SD card and sensors

**Phase 2:**
- [ ] Create dshot.c with DMA support
- [ ] Configure PWM0 sync mode
- [ ] Implement packet encoding
- [ ] Test DShot150/300/600
- [ ] Verify timing on oscilloscope
- [ ] Test with real ESCs

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** January 2026
**Author:** Claude Code Assistant

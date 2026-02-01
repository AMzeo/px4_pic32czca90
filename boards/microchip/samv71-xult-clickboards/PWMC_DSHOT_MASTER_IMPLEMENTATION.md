# SAMV71 PWMC + DShot Master Implementation Guide

**Created:** January 2026
**Status:** READY FOR IMPLEMENTATION
**Priority:** HIGH (Required for flight capability)
**Revision:** 2.3 - Fixed rate grouping params (PWM_MAIN_TIM0/TIM1), clarified MAX_IO_TIMERS

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
| PWM Backend | TC-based | Only 3 channels (PA26 conflict), crashes on startup |
| PWMOut Driver | **Enabled but unstable** | CONFIG_DRIVERS_PWM_OUT=y in default.px4board |
| DShot | Not Implemented | Requires PWMC + DMA + arch_dshot library |
| Motor Testing | HITL Only | No real motor output until PWMC implemented |

> **Note:** PWMOut is NOT disabled. It is enabled in `default.px4board` (line 25) but crashes
> due to TC-based io_timer implementation issues.

### 1.2 Implementation Goals

| Phase | Goal | Channels | Protocol |
|-------|------|----------|----------|
| Phase 1 | Basic PWMC PWM | 4 | Standard 400Hz PWM |
| Phase 2 | DShot via DMA | 4 | DShot150/300/600 |
| Phase 3 | Bidirectional (optional) | 4 | eRPM telemetry |

### 1.3 Selected Pin Configuration

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

### 1.4 Key Integration Requirements (from Code Review)

1. **DShot requires arch_dshot library** - DShot.cpp driver calls `up_dshot_*` functions
2. **DMA DMAR distributes to sync channels in order CH1→CH2→CH3** - buffer layout must match
3. **io_timer_get_group() must return per-timer channel masks** - pwm_servo.c depends on this
4. **io_timer_set_enable() must support OneShot mode** - pwm_servo.c:147 calls with OneShot
5. **timer_io_channels[] is the single source of truth** - no duplicate motor_config[]
6. **Keep TC1/TC3 in defconfig** - RC input uses TC1 CH2 (PC29 = TC5 TIOA)

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
└── Shared DMA per PWMC instance (via PWM_DMAR)
```

### 2.2 PWMC Register Map

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
#define PWM_CMR_DTE         (1 << 16) /* Dead-Time Enable */
```

### 2.4 Sync Channels Mode (PWM_SCM)

```c
/* PWM_SCM - Synchronous Channels Mode Register */
#define PWM_SCM_SYNC0       (1 << 0)  /* Sync Channel 0 */
#define PWM_SCM_SYNC1       (1 << 1)  /* Sync Channel 1 */
#define PWM_SCM_SYNC2       (1 << 2)  /* Sync Channel 2 */
#define PWM_SCM_SYNC3       (1 << 3)  /* Sync Channel 3 */
#define PWM_SCM_UPDM_MODE0  (0 << 16) /* Manual (SCUC.UPDULOCK) */
#define PWM_SCM_UPDM_MODE1  (1 << 16) /* Auto immediate */
#define PWM_SCM_UPDM_MODE2  (2 << 16) /* Auto on period */
#define PWM_SCM_PTRM        (1 << 20) /* DMA Transfer Request Mode */
```

### 2.5 Critical: DMA DMAR Channel Ordering

**The PWM_DMAR register distributes duty values to synchronized channels in ascending channel order.**

For PWM0 with CH1, CH2, CH3 synchronized:
- First write to DMAR → CH1_CDTYUPD (Motor 2: PA2)
- Second write to DMAR → CH2_CDTYUPD (Motor 3: PC19)
- Third write to DMAR → CH3_CDTYUPD (Motor 1: PA7)

**DMA buffer layout for DShot (PWM0, 3 motors):**
```
Index:  [0]       [1]       [2]       [3]       [4]       ...
Value:  M2_bit15  M3_bit15  M1_bit15  M2_bit14  M3_bit14  M1_bit14 ...
        ↓         ↓         ↓
        CH1       CH2       CH3
```

This is **different** from the logical motor ordering! The io_timer code must map:
- Motor 1 (output channel 0) → buffer index 2 (CH3)
- Motor 2 (output channel 1) → buffer index 0 (CH1)
- Motor 3 (output channel 2) → buffer index 1 (CH2)
- Motor 4 (output channel 3) → direct CDTYUPD on PWM1 CH1

### 2.6 DShot vs STM32 Comparison

| Aspect | STM32 Approach | SAMV7 Approach |
|--------|----------------|----------------|
| DMA Trigger | Timer UPDATE event burst mode | PWMC period end via DMAR |
| Multi-Channel | DMAR burst to CCR1-4 | PWM_DMAR auto-distributes to sync channels |
| Buffer Layout | Interleaved by timer | Interleaved by sync channel order |
| Capture (BDSHOT) | Timer input capture | TC capture (separate peripheral) |
| Width | 16-bit or 32-bit CCR | **16-bit CDTYUPD** |

**Key SAMV7 Advantage:** Native sync update (PWM_SCM) simplifies multi-channel atomic updates.

**Key SAMV7 Constraint:** CDTYUPD is 16-bit. DMA transfers must use 16-bit width.

---

## 3. Hardware Configuration

### 3.1 Physical Connections

```
SAMV71-XULT Board - Motor Output Locations
==========================================

Arduino Header:
┌─────────────────────────────────────┐
│ A1 (PA7)  ← Motor 1 (PWM0 CH3)      │
└─────────────────────────────────────┘

EXT2 Header:
┌─────────────────────────────────────┐
│ Pin 8 (PA14) ← Motor 4 (PWM1 CH1)   │
│ Pin 9 (PA2)  ← Motor 2 (PWM0 CH1)   │
└─────────────────────────────────────┘

mikroBUS Socket 1:
┌─────────────────────────────────────┐
│ PWM (PC19) ← Motor 3 (PWM0 CH2)     │
│ INT (PA0)  - FREE for sensors       │
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

### 3.4 XDMAC Configuration

```
SAMV7 XDMAC Peripheral IDs:
  PWM0_TX = 13
  PWM1_TX = 39

DMA Channel Configuration for DShot:
  - Source: Memory (DShot buffer)
  - Destination: PWM_DMAR (or CDTYUPD for single-channel)
  - Width: 16-bit (CDTYUPD is 16-bit register)
  - Mode: Memory-to-Peripheral
  - Trigger: PWM period end
  - Block size: 17 transfers per motor per frame (16 bits + reset)
```

**Cache Maintenance:** With D-cache enabled, DMA buffers must be:
- Aligned to cache line boundary (32 bytes on Cortex-M7)
- Flushed before DMA transfer: `SCB_CleanDCache_by_Addr()`
- Or placed in non-cacheable region

---

## 4. Phase 1: PWMC Basic PWM

### 4.1 Files to Create/Modify

```
CREATE:
  platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c

MODIFY:
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h
  platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt
  boards/microchip/samv71-xult-clickboards/src/board_config.h
  boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
  boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig
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

# Keep TC0 for HRT
CONFIG_SAMV7_TC0=y

# KEEP TC1 - Required for RC input capture (TC1 CH2 = TC5 TIOA = PC29)
CONFIG_SAMV7_TC1=y

# KEEP TC3 - Used by pck6_test
CONFIG_SAMV7_TC3=y
```

> **Important:** Do NOT remove TC1 or TC3. RC input uses TC1 CH2 (PC29) for pulse capture.
> See board_config.h line 188: `GPIO_RC_INPUT` maps to TC5 TIOA.

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

/*
 * BOARD_NUM_IO_TIMERS - informational only
 *
 * NOTE: SAMV7 io_timer.h hard-codes MAX_IO_TIMERS to 4 and doesn't reference
 * BOARD_NUM_IO_TIMERS. This define is for documentation/logging purposes only.
 * If you want to actually limit timer scanning, update MAX_IO_TIMERS in:
 *   platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h
 *
 * For PWMC with 2 instances (PWM0 + PWM1), having MAX_IO_TIMERS=4 is harmless -
 * io_timers[2] and io_timers[3] will have base=0 and be skipped.
 */
#define BOARD_NUM_IO_TIMERS  2

/* GPIO definitions for PX4_GPIO_INIT_LIST (timer_config.cpp builds gpio_out from PWMC/Pin tuples) */
#define GPIO_PWM0_CH3_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)
#define GPIO_PWM0_CH1_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)
#define GPIO_PWM0_CH2_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)
#define GPIO_PWM1_CH1_OUT  (GPIO_PERIPHC | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN14)
```

**Also update `PX4_GPIO_INIT_LIST`** to remove old TC-based PWM pins and add PWMC pins:

```diff
/* Remove old TC pins (PA15, PC23, PC26) - now stale after PWMC switch */
#define PX4_GPIO_INIT_LIST { \
        GPIO_nLED_BLUE,           \
        GPIO_SPI0_CS_ICM20689,    \
        GPIO_SPI0_DRDY_ICM20689,  \
        GPIO_SPI0_CS_BMP388,      \
        GPIO_MB1_RST,             \
        GPIO_MB2_RST,             \
        GPIO_EXT1_RST,            \
        GPIO_EXT2_RST,            \
-       GPIO_PWM1_OUT,            \  /* OLD: TC1 TIOA PA15 */
-       GPIO_PWM2_OUT,            \  /* OLD: TC3 TIOA PC23 */
-       GPIO_PWM3_OUT,            \  /* OLD: TC4 TIOA PC26 */
+       GPIO_PWM0_CH3_OUT,        \  /* NEW: PWMC Motor 1 PA7 */
+       GPIO_PWM0_CH1_OUT,        \  /* NEW: PWMC Motor 2 PA2 */
+       GPIO_PWM0_CH2_OUT,        \  /* NEW: PWMC Motor 3 PC19 */
+       GPIO_PWM1_CH1_OUT,        \  /* NEW: PWMC Motor 4 PA14 */
        GPIO_BTN_SAFETY,          \
        GPIO_LED_SAFETY,          \
        GPIO_nARMED_INIT,         \
    }
```

> **Note:** The old GPIO_PWM1_OUT/GPIO_PWM2_OUT/GPIO_PWM3_OUT defines for TC-based PWM
> should also be removed or renamed to avoid confusion. io_timer_channel_init() will
> configure the PWMC pins at runtime from timer_io_channels[].gpio_out.

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

### 5.3 DShot Architecture Layer Requirements

**The PX4 DShot API (src/drivers/drv_dshot.h) defines these required symbols:**

```c
// EXPORTED functions that MUST be implemented (link-time symbols):
int up_dshot_init(uint32_t channel_mask, unsigned dshot_pwm_freq, bool enable_bidirectional_dshot);
void up_dshot_trigger(void);
int up_dshot_arm(bool armed);

// CRITICAL: The actual motor data function is dshot_motor_data_set (not up_dshot_*)
// up_dshot_motor_data_set() and up_dshot_motor_command() are INLINE wrappers in drv_dshot.h
void dshot_motor_data_set(unsigned channel, uint16_t throttle, bool telemetry);

// Bidirectional DShot stubs - REQUIRED even if bidirectional is disabled:
void up_bdshot_status(void);
int up_bdshot_num_erpm_ready(void);
int up_bdshot_get_erpm(uint8_t channel, int *erpm);
int up_bdshot_channel_status(uint8_t channel);
```

> **IMPORTANT:** The header file drv_dshot.h defines `up_dshot_motor_data_set()` and
> `up_dshot_motor_command()` as `static inline` functions that call `dshot_motor_data_set()`.
> You must export `dshot_motor_data_set`, NOT the up_ versions. See drv_dshot.h:99-123.

### 5.4 Files to Create for DShot

```
CREATE:
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/dshot.h
  platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c
  platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt

MODIFY:
  platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt (add dshot subdirectory)
```

### 5.5 DMA Buffer Layout for PWM0 (3 motors)

```
PWM0 synchronizes CH1, CH2, CH3
DMAR distributes in order: CH1 → CH2 → CH3

Motor mapping:
  Motor 1 = CH3 (buffer position 2)
  Motor 2 = CH1 (buffer position 0)
  Motor 3 = CH2 (buffer position 1)

Buffer layout for 16-bit frame + reset (17 transfers per motor):
  Total = 17 × 3 = 51 transfers

  Index:  0   1   2   3   4   5   ...  48  49  50
  Motor:  M2  M3  M1  M2  M3  M1  ...  M2  M3  M1
  Bit:    15  15  15  14  14  14  ...  RST RST RST

Code to fill buffer:
  for (int bit = 0; bit < 17; bit++) {
      buffer[bit * 3 + 0] = motor2_duty[bit];  // CH1
      buffer[bit * 3 + 1] = motor3_duty[bit];  // CH2
      buffer[bit * 3 + 2] = motor1_duty[bit];  // CH3
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
 * Supports both standard PWM (Phase 1) and provides foundation for DShot (Phase 2).
 *
 * Key Design Decisions:
 * 1. timer_io_channels[] is the single source of truth for channel configuration
 * 2. io_timer_get_group() returns bitmask of channels belonging to specified timer
 * 3. io_timer_set_enable() supports both PWMOut and OneShot modes (required by pwm_servo.c)
 * 4. io_timer_set_rate() applies rate to the specified timer only
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

/*
 * State tracking
 */
static bool g_pwmc_initialized[2] = {false, false};
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static uint32_t g_timer_period[MAX_IO_TIMERS];  /* Per-timer period for independent rates */

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
 * Get PWMC base and channel from timer_io_channels[]
 */
static inline uint32_t get_pwmc_base(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return 0;
    uint8_t timer_index = timer_io_channels[channel].timer_index;
    if (timer_index >= MAX_IO_TIMERS) return 0;
    return io_timers[timer_index].base;
}

static inline uint8_t get_pwmc_channel(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return 0xFF;
    return timer_io_channels[channel].timer_channel;
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
        g_timer_period[timer] = PWM_DEFAULT_PERIOD;
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

    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }

    /* Accept PWMOut, OneShot, and NotUsed modes */
    if (mode != IOTimerChanMode_PWMOut &&
        mode != IOTimerChanMode_OneShot &&
        mode != IOTimerChanMode_NotUsed) {
        return -EINVAL;
    }

    uint32_t base = get_pwmc_base(channel);
    uint8_t pwm_ch = get_pwmc_channel(channel);

    if (base == 0 || pwm_ch == 0xFF) {
        return -EINVAL;
    }

    uint8_t timer_index = timer_io_channels[channel].timer_index;

    /* Ensure timer is initialized */
    int ret = io_timer_init_timer(timer_index);
    if (ret != OK) {
        return ret;
    }

    if (mode == IOTimerChanMode_PWMOut || mode == IOTimerChanMode_OneShot) {
        /* Configure GPIO for PWMC */
        sam_configgpio(timer_io_channels[channel].gpio_out);

        /* Disable channel first */
        pwmc_putreg(base, PWM_DIS_OFFSET, (1 << pwm_ch));

        /* Configure channel mode:
         * - Prescaler MCK/32 for standard PWM
         * - Left-aligned
         * - Output starts low, goes high on match
         */
        uint32_t cmr = PWM_STD_PRESCALER;
        putreg32(cmr, get_channel_reg(base, pwm_ch, PWM_CMR_CH_OFFSET));

        /* Set period from timer's current setting */
        uint32_t period = g_timer_period[timer_index];
        putreg32(period, get_channel_reg(base, pwm_ch, PWM_CPRD_CH_OFFSET));

        /* Set initial duty to disarm (900 µs) */
        uint32_t duty = (900 * PWM_STD_CLOCK) / 1000000UL;
        putreg32(duty, get_channel_reg(base, pwm_ch, PWM_CDTY_CH_OFFSET));

        /* Enable channel */
        pwmc_putreg(base, PWM_ENA_OFFSET, (1 << pwm_ch));
    }

    g_channel_modes[channel] = mode;
    return OK;
}

/*
 * Set PWM duty cycle (pulse width in microseconds)
 */
int io_timer_set_ccr(unsigned channel, uint16_t value)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }

    io_timer_channel_mode_t mode = g_channel_modes[channel];
    if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_OneShot) {
        return -EINVAL;
    }

    uint32_t base = get_pwmc_base(channel);
    uint8_t pwm_ch = get_pwmc_channel(channel);
    uint8_t timer_index = timer_io_channels[channel].timer_index;

    if (base == 0 || pwm_ch == 0xFF) {
        return -EINVAL;
    }

    /* Convert microseconds to ticks */
    uint32_t ticks = ((uint32_t)value * PWM_STD_CLOCK) / 1000000UL;

    /* Clamp to period */
    if (ticks > g_timer_period[timer_index]) {
        ticks = g_timer_period[timer_index];
    }

    /* Use update register for glitch-free update */
    putreg32(ticks, get_channel_reg(base, pwm_ch, PWM_CDTYUPD_OFFSET));

    return OK;
}

/*
 * Set PWM rate (frequency) for a specific timer
 * This affects all channels on that timer.
 */
int io_timer_set_rate(unsigned timer, unsigned rate)
{
    if (timer >= MAX_IO_TIMERS) {
        return -EINVAL;
    }

    /* Rate of 0 means OneShot mode - keep current period */
    if (rate == 0) {
        return OK;
    }

    if (rate < 50 || rate > 8000) {
        return -ERANGE;
    }

    uint32_t base = io_timers[timer].base;
    if (base == 0) {
        return -EINVAL;
    }

    uint32_t period = PWM_STD_CLOCK / rate;
    g_timer_period[timer] = period;

    /* Update period for all channels on this timer */
    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (timer_io_channels[ch].timer_index == timer) {
            io_timer_channel_mode_t mode = g_channel_modes[ch];
            if (mode == IOTimerChanMode_PWMOut || mode == IOTimerChanMode_OneShot) {
                uint8_t pwm_ch = get_pwmc_channel(ch);
                putreg32(period, get_channel_reg(base, pwm_ch, PWM_CPRDUPD_OFFSET));
            }
        }
    }

    return OK;
}

/*
 * Enable/disable PWM output
 * IMPORTANT: Must support both IOTimerChanMode_PWMOut and IOTimerChanMode_OneShot
 * because pwm_servo.c calls this with both modes (see pwm_servo.c:147-148)
 */
int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
                        io_timer_channel_allocation_t masks)
{
    /* Accept both PWMOut and OneShot modes */
    if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_OneShot) {
        return -EINVAL;
    }

    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (masks & (1 << ch)) {
            io_timer_channel_mode_t ch_mode = g_channel_modes[ch];

            /* Only operate on channels in matching mode */
            if (ch_mode != mode) {
                continue;
            }

            uint32_t base = get_pwmc_base(ch);
            uint8_t pwm_ch = get_pwmc_channel(ch);

            if (base == 0 || pwm_ch == 0xFF) {
                continue;
            }

            if (state) {
                pwmc_putreg(base, PWM_ENA_OFFSET, (1 << pwm_ch));
            } else {
                pwmc_putreg(base, PWM_DIS_OFFSET, (1 << pwm_ch));
            }
        }
    }

    return OK;
}

/*
 * Get channel group bitmask for a specific timer
 * Returns bitmask of output channels that belong to the specified timer.
 * This is used by pwm_servo.c:142 to filter channels for rate grouping.
 */
uint32_t io_timer_get_group(unsigned timer)
{
    if (timer >= MAX_IO_TIMERS) {
        return 0;
    }

    uint32_t mask = 0;

    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (timer_io_channels[ch].timer_index == timer) {
            mask |= (1 << ch);
        }
    }

    return mask;
}

/*
 * Additional required io_timer API functions
 */
int io_timer_validate_channel_index(unsigned channel)
{
    return (channel < MAX_TIMER_IO_CHANNELS) ? 0 : -EINVAL;
}

int io_timer_is_channel_free(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return -EINVAL;
    return (g_channel_modes[channel] == IOTimerChanMode_NotUsed) ? 0 : -EBUSY;
}

int io_timer_free_channel(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return -EINVAL;
    g_channel_modes[channel] = IOTimerChanMode_NotUsed;
    return OK;
}

int io_timer_get_channel_mode(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return IOTimerChanMode_NotUsed;
    return g_channel_modes[channel];
}

int io_timer_get_mode_channels(io_timer_channel_mode_t mode)
{
    int mask = 0;
    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (g_channel_modes[ch] == mode) {
            mask |= (1 << ch);
        }
    }
    return mask;
}

uint16_t io_channel_get_ccr(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return 0;

    uint32_t base = get_pwmc_base(channel);
    uint8_t pwm_ch = get_pwmc_channel(channel);

    if (base == 0 || pwm_ch == 0xFF) return 0;

    uint32_t ticks = getreg32(get_channel_reg(base, pwm_ch, PWM_CDTY_CH_OFFSET));

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
    /* Updates applied automatically via CDTYUPD registers on next period */
    (void)channels_mask;
}
```

### 6.2 timer_config.cpp

```cpp
/**
 * @file timer_config.cpp
 *
 * PWMC-based PWM configuration for SAMV71-XULT
 *
 * This file defines timer_io_channels[] which is the SINGLE SOURCE OF TRUTH
 * for channel configuration. The io_timer_pwmc.c reads from this array.
 */

#include <px4_arch/io_timer_hw_description.h>

/*
 * IO Timer configuration
 *
 * We use 2 PWMC instances:
 *   io_timers[0] = PWM0 (Motors 1, 2, 3 on CH3, CH1, CH2)
 *   io_timers[1] = PWM1 (Motor 4 on CH1)
 */
constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimerPWMC(PWMC::PWM0),
    initIOTimerPWMC(PWMC::PWM1),
};

/*
 * Timer channel to motor mapping
 *
 * Index = Motor number - 1 (output channel index)
 * timer_index = which io_timers[] entry
 * timer_channel = PWMC channel number (1-3 for PWM0, 1 for PWM1)
 *
 * NOTE: For DShot, the DMA buffer ordering must account for DMAR
 * distributing to sync channels in ascending order (CH1, CH2, CH3).
 */
constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    /* Motor 1 (output channel 0): PWM0 CH3 -> PA7 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH3, PWMC::PeriphB},
        {GPIO::PortA, GPIO::Pin7}),

    /* Motor 2 (output channel 1): PWM0 CH1 -> PA2 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH1, PWMC::PeriphA},
        {GPIO::PortA, GPIO::Pin2}),

    /* Motor 3 (output channel 2): PWM0 CH2 -> PC19 */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH2, PWMC::PeriphB},
        {GPIO::PortC, GPIO::Pin19}),

    /* Motor 4 (output channel 3): PWM1 CH1 -> PA14 */
    initIOTimerChannelPWMC(io_timers, 1,
        {PWMC::PWM1, PWMC::CH1, PWMC::PeriphC},
        {GPIO::PortA, GPIO::Pin14}),
};

constexpr io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

### 6.3 hw_description.h Additions

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
    PeriphD = 3,
};

struct PWMChannel {
    Instance instance;
    Channel channel;
    Peripheral periph;
};

} // namespace PWMC
```

### 6.4 io_timer_hw_description.h Additions

```cpp
/* Add PWMC initialization helpers */

/**
 * Initialize an IO timer for PWMC peripheral
 *
 * NOTE: The current io_timers_t structure (io_timer.h:73-78) does NOT have a
 * dshot field. For Phase 2 DShot support, you must add to io_timers_t:
 *
 *   typedef struct io_timers_t {
 *       uint32_t  base;
 *       uint32_t  clock_register;
 *       uint32_t  clock_bit;
 *       uint32_t  vectorno;
 *       // Add for DShot Phase 2:
 *       struct {
 *           uint32_t dma_base;
 *           uint32_t dma_map_up;
 *       } dshot;
 *   } io_timers_t;
 */
static inline constexpr io_timers_t initIOTimerPWMC(PWMC::Instance instance)
{
    io_timers_t ret{};

    switch (instance) {
    case PWMC::PWM0:
        ret.base = 0x40020000;  /* SAM_PWM0_BASE */
        break;
    case PWMC::PWM1:
        ret.base = 0x4005C000;  /* SAM_PWM1_BASE */
        break;
    }

    ret.clock_register = 0;
    ret.clock_bit = 0;
    ret.vectorno = 0;

    /* NOTE: DShot configuration requires io_timers_t struct modification.
     * For Phase 1 (basic PWM), no dshot field is needed.
     * For Phase 2, add dshot field to io_timers_t and uncomment:
     * ret.dshot.dma_base = 0;
     * ret.dshot.dma_map_up = 0;
     */

    return ret;
}

/**
 * Initialize a timer channel for PWMC output
 */
static inline constexpr timer_io_channels_t initIOTimerChannelPWMC(
    const io_timers_t io_timers_conf[MAX_IO_TIMERS],
    uint8_t timer_index,
    PWMC::PWMChannel pwm_channel,
    GPIO::GPIOPin pin)
{
    timer_io_channels_t ret{};

    /* Build GPIO configuration */
    uint32_t gpio_mode;
    switch (pwm_channel.periph) {
    case PWMC::PeriphA: gpio_mode = (1 << 21); break;  /* GPIO_PERIPHA */
    case PWMC::PeriphB: gpio_mode = (4 << 21); break;  /* GPIO_PERIPHB */
    case PWMC::PeriphC: gpio_mode = (5 << 21); break;  /* GPIO_PERIPHC */
    case PWMC::PeriphD: gpio_mode = (6 << 21); break;  /* GPIO_PERIPHD */
    default: gpio_mode = 0; break;
    }

    uint32_t gpio_port = ((uint32_t)pin.port << 5);
    uint32_t gpio_pin = (uint32_t)pin.pin;

    ret.gpio_out = gpio_mode | gpio_port | gpio_pin;
    ret.gpio_in = 0;
    ret.timer_index = timer_index;
    ret.timer_channel = (uint8_t)pwm_channel.channel;

    return ret;
}
```

### 6.5 DShot Arch Layer (Phase 2)

**File: platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/dshot.h**

```c
/**
 * @file dshot.h
 *
 * SAMV7 DShot architecture definitions
 */

#pragma once

#include <drivers/drv_pwm_output.h>

/**
 * DSHOT_MOTOR_PWM_BIT_WIDTH - Timer ticks per DShot bit period
 *
 * This is NOT the register width. It's the number of PWM duty cycle
 * resolution steps per DShot bit. STM32 uses 20; for SAMV7 we calculate:
 *
 * For DShot600 with MCK/8 = 18.75MHz:
 *   Bit period = 1.67µs = 31 ticks
 *   We want ~20 steps of resolution → 31 ticks / 1.5 ≈ 20
 *
 * The formula used in STM32 io_timer.c:
 *   rARR = DSHOT_MOTOR_PWM_BIT_WIDTH (auto-reload = period)
 *   rPSC = (clock_freq / dshot_freq) / DSHOT_MOTOR_PWM_BIT_WIDTH - 1
 *
 * For SAMV7 PWMC, this translates to CPRD (period) register value.
 */
#define DSHOT_MOTOR_PWM_BIT_WIDTH    20u

/**
 * DShot configuration for each PWMC instance
 * Similar to STM32 dshot_conf_t but adapted for SAMV7 XDMAC
 */
typedef struct dshot_conf_t {
    uint32_t xdmac_base;       /* XDMAC base address (0x40078000) */
    uint8_t  xdmac_periph_id;  /* Peripheral ID: PWM0_TX=13, PWM1_TX=39 */
    uint8_t  sync_channels;    /* Bitmask of synchronized channels */
} dshot_conf_t;
```

**File: platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c** (complete stubs)

> **CRITICAL:** These stubs provide ALL symbols required by drv_dshot.h to prevent
> link errors when CONFIG_DRIVERS_DSHOT=y. The key function is `dshot_motor_data_set`
> (NOT `up_dshot_motor_data_set`), as the up_ versions are inline wrappers.

```c
/**
 * @file dshot.c
 *
 * SAMV7 DShot implementation using PWMC + XDMAC
 *
 * This file provides stubs for all required DShot symbols.
 * Phase 2 will implement actual DMA-based DShot functionality.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <px4_arch/io_timer.h>
#include <drivers/drv_dshot.h>
#include <errno.h>

/* ============================================================================
 * Required exported symbols from drv_dshot.h
 * ============================================================================ */

/**
 * Initialize DShot outputs
 */
int up_dshot_init(uint32_t channel_mask, unsigned dshot_pwm_freq, bool enable_bidirectional_dshot)
{
    (void)channel_mask;
    (void)dshot_pwm_freq;
    (void)enable_bidirectional_dshot;
    PX4_WARN("SAMV7 DShot not yet implemented");
    return -ENOSYS;
}

/**
 * Trigger DShot frame transmission
 */
void up_dshot_trigger(void)
{
    /* TODO: Start XDMAC transfer to PWM_DMAR */
}

/**
 * Arm/disarm DShot outputs
 */
int up_dshot_arm(bool armed)
{
    (void)armed;
    return -ENOSYS;
}

/**
 * Set motor throttle/command data
 *
 * CRITICAL: This is the actual exported symbol. The inline functions
 * up_dshot_motor_data_set() and up_dshot_motor_command() in drv_dshot.h
 * call THIS function. Do NOT implement up_dshot_motor_data_set().
 */
void dshot_motor_data_set(unsigned channel, uint16_t throttle, bool telemetry)
{
    (void)channel;
    (void)throttle;
    (void)telemetry;
    /* TODO: Encode DShot packet and fill DMA buffer at correct position
     * accounting for DMAR CH1→CH2→CH3 distribution order */
}

/* ============================================================================
 * Bidirectional DShot stubs (required even if bidirectional is disabled)
 * ============================================================================ */

/**
 * Print bidirectional DShot status
 */
void up_bdshot_status(void)
{
    PX4_INFO("Bidirectional DShot not implemented on SAMV7");
}

/**
 * Get number of eRPM channels ready
 */
int up_bdshot_num_erpm_ready(void)
{
    return 0;  /* No bidirectional support yet */
}

/**
 * Get eRPM for a channel
 */
int up_bdshot_get_erpm(uint8_t channel, int *erpm)
{
    (void)channel;
    if (erpm) {
        *erpm = 0;
    }
    return -ENOSYS;
}

/**
 * Get channel status (online/offline)
 *
 * IMPORTANT: Return 0 (offline), NOT -ENOSYS!
 * DShot.cpp:279 uses `if (up_bdshot_channel_status(i))` where any non-zero
 * value is treated as "online". Returning -ENOSYS (-88) would erroneously
 * mark all ESCs as online when bidirectional DShot is enabled.
 */
int up_bdshot_channel_status(uint8_t channel)
{
    (void)channel;
    return 0;  /* 0 = offline/unsupported, 1 = online */
}
```

**File: platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt**

```cmake
px4_add_library(arch_dshot
    dshot.c
)

# NOTE: STM32 arch_dshot does NOT link to arch_io_pins.
# Match that pattern - no extra dependencies needed.
target_compile_options(arch_dshot PRIVATE ${MAX_CUSTOM_OPT_LEVEL})
```

### 6.6 DShot Integration Approach (Phase 2 Design Decision)

**Question:** Should SAMV7 DShot follow the STM32 model (`io_timers[].dshot`) or be standalone?

**Recommendation: Standalone for Phase 2**

The STM32 DShot implementation deeply integrates with `io_timers[].dshot` configuration,
which requires:
1. Adding `dshot` field to `io_timers_t` struct
2. Populating `dshot_conf_t` in each `io_timers[]` entry
3. Coordinating between io_timer.c and dshot.c

For SAMV7, a **standalone approach** is simpler for initial implementation:
- dshot.c directly accesses PWMC registers (already knows PWM0/PWM1 bases)
- Uses `timer_io_channels[]` for channel-to-motor mapping (single source of truth)
- XDMAC configuration is SAMV7-specific anyway

**If io_timers[].dshot integration is desired later**, add to `io_timer.h`:

```c
typedef struct io_timers_t {
    uint32_t  base;
    uint32_t  clock_register;
    uint32_t  clock_bit;
    uint32_t  vectorno;
#ifdef CONFIG_DRIVERS_DSHOT
    struct {
        uint32_t xdmac_periph_id;  /* PWM0_TX=13, PWM1_TX=39 */
        uint8_t  sync_channels;    /* Bitmask of channels to sync */
    } dshot;
#endif
} io_timers_t;
```

Then update `initIOTimerPWMC()` to populate these fields.

**For Phase 2, the standalone approach is recommended** - keep dshot.c self-contained
until the basic implementation is proven, then refactor to match io_timers pattern if needed.

---

## 7. Build Configuration

### 7.1 default.px4board

```cmake
# Enable PWM output (already set)
CONFIG_DRIVERS_PWM_OUT=y

# For Phase 2: Enable DShot (uncomment when ready)
# CONFIG_DRIVERS_DSHOT=y
```

### 7.2 io_pins/CMakeLists.txt

```cmake
# platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt

px4_add_library(arch_io_pins
    io_timer_pwmc.c
    pwm_servo.c
)

# IMPORTANT: Keep this link - needed for board symbols
target_link_libraries(arch_io_pins PRIVATE drivers_board)
```

### 7.3 samv7/CMakeLists.txt

```cmake
# platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt
#
# IMPORTANT: Match existing subdirectories exactly. Current structure:
#   adc, hrt, version, board_reset, io_pins, board_critmon
# Do NOT add tone_alarm (directory does not exist)

add_subdirectory(adc)
add_subdirectory(hrt)
add_subdirectory(version)
add_subdirectory(board_reset)
add_subdirectory(io_pins)
add_subdirectory(board_critmon)

# Add for Phase 2 DShot support
if(CONFIG_DRIVERS_DSHOT)
    add_subdirectory(dshot)
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

### 8.3 Rate Grouping Test

> **Note:** There is NO `pwm_out rate -g` CLI command. PWM rates are configured
> via parameters. The PWMOut driver reads `PWM_MAIN_TIM0` and `PWM_MAIN_TIM1` at startup.
>
> **Parameter naming:** PWMOut uses `PARAM_PREFIX_TIM%u` where:
> - `PARAM_PREFIX` = `PWM_MAIN` (default) or `PWM_AUX` (if CONFIG_BOARD_IO set)
> - `%u` = timer index starting at **0** (not 1)
>
> Since SAMV71 doesn't set CONFIG_BOARD_IO, the prefix is `PWM_MAIN`.

```bash
# Set timer rates via parameters:
# Timer 0 (PWM0): Motors 1, 2, 3
# Timer 1 (PWM1): Motor 4

nsh> param set PWM_MAIN_TIM0 400   # Set PWM0 (timer 0) to 400 Hz
nsh> param set PWM_MAIN_TIM1 50    # Set PWM1 (timer 1) to 50 Hz
nsh> param save

# Reboot for parameters to take effect
nsh> reboot

# After reboot, verify with pwm_out status:
nsh> pwm_out status
# Expected: Timer 0: rate: 400, Timer 1: rate: 50

# Verify with oscilloscope:
# PA7, PA2, PC19 should be 400 Hz
# PA14 should be 50 Hz
```

### 8.4 Regression Tests

```bash
# Verify SD card still works
nsh> param save
nsh> param load

# Verify sensors work
nsh> listener sensor_accel -n 1
nsh> listener sensor_baro -n 1

# Verify I2C bus
nsh> i2cdetect -b 1

# Verify RC input (TC1 CH2 still functional)
nsh> listener input_rc -n 1
```

### 8.5 Phase 2: DShot Tests (Future)

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
| No PWM output | GPIO not configured | Verify timer_io_channels[].gpio_out |
| Wrong frequency | Period register incorrect | Check g_timer_period[] and CPRD |
| Glitchy output | Not using update registers | Use CDTYUPD instead of CDTY |
| PWMOut crashes | Missing io_timer functions | Verify all API functions implemented |
| SD card corruption | Using PA26 | Verify PA26 not in timer_io_channels |
| RC input broken | TC1 removed from defconfig | Keep CONFIG_SAMV7_TC1=y |
| Wrong motor mapping | DMAR order mismatch | Verify buffer layout matches CH1→CH2→CH3 |
| DMA transfer fails | Cache not flushed | Call SCB_CleanDCache_by_Addr() |

### 9.2 Debug Commands

```bash
# Check PWMC registers
nsh> perf        # Performance counters
nsh> dmesg       # Kernel messages
nsh> top         # Task status

# Memory check
nsh> free

# Work queue status
nsh> work_queue status
```

### 9.3 io_timer API Compatibility Checklist

The io_timer_pwmc.c must implement ALL these functions called by pwm_servo.c and PWMOut:

- [x] `io_timer_init_timer()`
- [x] `io_timer_channel_init()` - must accept IOTimerChanMode_PWMOut AND IOTimerChanMode_OneShot
- [x] `io_timer_set_ccr()`
- [x] `io_timer_set_rate()` - must be per-timer, not global
- [x] `io_timer_set_enable()` - must handle IOTimerChanMode_OneShot (pwm_servo.c:147)
- [x] `io_timer_get_group()` - must return per-timer channel mask (pwm_servo.c:142)
- [x] `io_timer_get_mode_channels()`
- [x] `io_timer_free_channel()`
- [x] `io_timer_unallocate_channel()`
- [x] `io_timer_set_pwm_rate()`
- [x] `io_timer_trigger()`
- [x] `io_channel_get_ccr()`

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
  platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c
  platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/

Board:
  boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
  boards/microchip/samv71-xult-clickboards/src/board_config.h
```

### 10.3 Implementation Checklist

**Phase 1:**
- [ ] Add PWMC configs to NuttX defconfig (keep TC1/TC3!)
- [ ] Add PWMC namespace to hw_description.h
- [ ] Add initIOTimerPWMC() to io_timer_hw_description.h
- [ ] Create io_timer_pwmc.c with full API compatibility
- [ ] Update timer_config.cpp
- [ ] Update board_config.h:
  - [ ] Add GPIO_PWM0_CH[1-3]_OUT and GPIO_PWM1_CH1_OUT defines
  - [ ] Update BOARD_NUM_IO_TIMERS from 3 to 2
  - [ ] Update PX4_GPIO_INIT_LIST (remove TC pins, add PWMC pins)
  - [ ] Remove or rename old GPIO_PWM[1-3]_OUT TC defines
- [ ] Update io_pins/CMakeLists.txt to use io_timer_pwmc.c
- [ ] Build and test
- [ ] Oscilloscope verify all 4 channels
- [ ] Test independent rate groups (via PWM_MAIN_TIM0/TIM1 params)
- [ ] Regression test SD card, sensors, RC input

**Phase 2:**
- [ ] Create px4_arch/dshot.h
- [ ] Create dshot/dshot.c with up_dshot_* functions
- [ ] Create dshot/CMakeLists.txt
- [ ] Add dshot subdirectory to samv7/CMakeLists.txt
- [ ] Implement DMA buffer with correct CH1→CH2→CH3 ordering
- [ ] Add cache maintenance (SCB_CleanDCache_by_Addr)
- [ ] Configure PWM0 sync mode (PWM_SCM)
- [ ] Implement packet encoding
- [ ] Test DShot150/300/600
- [ ] Verify timing on oscilloscope
- [ ] Test with real ESCs

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** January 2026
**Revision:** 2.3 - Fixed rate grouping params (PWM_MAIN_TIM0/TIM1), clarified MAX_IO_TIMERS
**Author:** Claude Code Assistant

**Key Corrections from v1.0:**
1. Fixed "PWMOut DISABLED" → "Enabled but unstable"
2. Fixed io_timer_get_group() to return per-timer channel masks
3. Fixed io_timer_set_rate() to be per-timer
4. Fixed io_timer_set_enable() to support OneShot mode
5. Removed duplicate motor_config[] - now uses timer_io_channels[]
6. Documented DMA DMAR ordering (CH1→CH2→CH3)
7. Kept TC1/TC3 in defconfig (RC input dependency)
8. Added complete DShot arch layer stubs
9. Removed dead BOARD_IO_TIMER_PWMC config
10. Added cache maintenance notes for DMA

**Key Corrections from v2.0:**
11. **CRITICAL:** Fixed DShot API - export `dshot_motor_data_set()` not `up_dshot_motor_data_set()`
    (the up_ versions are inline wrappers in drv_dshot.h)
12. **CRITICAL:** Added all `up_bdshot_*` stubs required at link time (status, num_erpm_ready,
    get_erpm, channel_status)
13. **HIGH:** Removed invalid `ret.dshot.*` from initIOTimerPWMC() (io_timers_t lacks dshot field)
    Added note about required struct modification for Phase 2
14. **MEDIUM:** Fixed DSHOT_MOTOR_PWM_BIT_WIDTH comment - it's timer ticks per bit period (20),
    not register width (16)
15. **MEDIUM:** Added missing `target_link_libraries(arch_io_pins PRIVATE drivers_board)`
16. **LOW:** Fixed samv7/CMakeLists.txt - removed non-existent tone_alarm, added missing
    version/board_reset/board_critmon subdirectories

**Key Corrections from v2.1:**
17. **HIGH:** Fixed `up_bdshot_channel_status()` to return 0 (offline) instead of -ENOSYS
    (DShot.cpp:279 treats non-zero as "online", causing false positives)
18. **MEDIUM:** Added `BOARD_NUM_IO_TIMERS` update (3→2 for PWM0/PWM1)
19. **MEDIUM:** Added `PX4_GPIO_INIT_LIST` update to replace TC pins with PWMC pins
20. **MEDIUM:** Added Section 6.6 clarifying dshot_conf_t integration approach (standalone vs io_timers)
21. **LOW:** Fixed rate grouping test - removed invalid `pwm_out rate -g` command,
    use PWM_MAIN_TIM0/TIM1 parameters instead
22. **LOW:** Removed `arch_io_pins` dependency from arch_dshot CMakeLists.txt (match STM32 pattern)

**Key Corrections from v2.2 (this revision):**
23. **MEDIUM:** Fixed rate grouping parameters: PWM_MAIN_TIM0/TIM1 (not PWM_AUX_TIM1/TIM2)
    - PWMOut uses `PARAM_PREFIX_TIM%u` where PARAM_PREFIX=PWM_MAIN by default
    - Timer index starts at 0, so first timer is TIM0
24. **LOW:** Clarified BOARD_NUM_IO_TIMERS is informational only
    - MAX_IO_TIMERS is hard-coded to 4 in io_timer.h, not derived from board config
    - BOARD_NUM_IO_TIMERS serves as documentation for other tooling

# SAMV71 PWMC + DShot Implementation Plan

**Created:** January 2026
**Revised:** January 2026 (API consistency fixes)
**Status:** PLANNING
**Priority:** HIGH
**Estimated Effort:** 2-3 weeks

---

## Executive Summary

Implement 4-channel motor output using the SAMV71 PWMC (PWM Controller) peripheral with DShot protocol support. This replaces the current TC-based 3-channel PWM implementation.

### Goals
1. **Phase 1:** 4-channel standard PWM using PWMC (400 Hz, 1000-2000 µs)
2. **Phase 2:** DShot protocol support using PWMC + XDMAC (DShot150/300/600)

### Selected Pin Configuration (Pin Set A)
| Motor | PWMC | Channel | Pin | Location | Peripheral |
|-------|------|---------|-----|----------|------------|
| 1 | PWM0 | CH3 | PA7 | Arduino A1 | Peripheral B |
| 2 | PWM0 | CH1 | PA2 | EXT2 pin 9 | Peripheral A |
| 3 | PWM0 | CH2 | PC19 | mikroBUS1 PWM | Peripheral B |
| 4 | PWM1 | CH1 | PA14 | EXT2 pin 8 | Peripheral C |

**Note:** Motors 1-3 share PWM0 (single DMA channel), Motor 4 uses PWM1 (separate DMA channel).

---

## Part 1: Architecture Overview

### 1.1 SAMV71 PWMC Peripheral

The SAMV71 has two PWM Controller instances:
- **PWM0:** 4 channels (CH0-CH3), base address 0x40020000
- **PWM1:** 4 channels (CH0-CH3), base address 0x4005C000

Each channel has:
- **PWMH** (High) output - active high signal
- **PWML** (Low) output - complementary signal (inverted)
- Independent period (CPRD) and duty cycle (CDTY) registers
- Dead-time generator
- DMA support for duty cycle updates (shared per PWMC instance)

### 1.2 PWMC vs TC Comparison

| Feature | TC (Current) | PWMC (Target) |
|---------|--------------|---------------|
| Channels | 3 (limited by conflicts) | 4+ |
| DMA Duty Update | No | Yes |
| DShot Support | No | Yes |
| Complementary Output | No | Yes |
| Dead-Time | No | Yes |
| Center-Aligned PWM | No | Yes |
| Shared with HRT | Yes (TC0 CH0) | No |

### 1.3 DShot Protocol Overview

DShot is a digital protocol for ESC communication:

```
DShot Frame (16 bits):
┌─────────────────────────────────────────────────────────┐
│  11-bit Throttle  │ Telemetry │   4-bit CRC            │
│   (0-2047)        │   (1 bit) │   (XOR checksum)       │
└─────────────────────────────────────────────────────────┘
     Bits 15-5          Bit 4        Bits 3-0
```

**Timing Requirements:**
| Protocol | Bit Period | T0H (37.5%) | T1H (75%) | Frame Rate |
|----------|------------|-------------|-----------|------------|
| DShot150 | 6.67 µs | 2.50 µs | 5.00 µs | ~8 kHz |
| DShot300 | 3.33 µs | 1.25 µs | 2.50 µs | ~16 kHz |
| DShot600 | 1.67 µs | 0.625 µs | 1.25 µs | ~32 kHz |

### 1.4 DShot Timing Analysis

**Clock Selection for DShot:**

The PWMC can use:
- Direct MCK prescalers (MCK/1, MCK/2, MCK/4, MCK/8, MCK/16, MCK/32, MCK/64, etc.)
- CLKA/CLKB programmable dividers

For adequate bit resolution at DShot600:
```
DShot600 bit period = 1.67 µs
Target: ≥16 ticks per bit for <6.25% duty cycle resolution

Option 1: MCK/8 = 150MHz/8 = 18.75 MHz
  - Bit period: 1.67 µs × 18.75 MHz = 31.25 ticks ✓ GOOD
  - T0H (37.5%): 11.7 ticks → 12 ticks = 38.4% (0.9% error)
  - T1H (75.0%): 23.4 ticks → 23 ticks = 73.6% (1.4% error)

Option 2: MCK/32 = 150MHz/32 = 4.6875 MHz (REJECTED)
  - Bit period: 1.67 µs × 4.6875 MHz = 7.8 ticks ✗ TOO LOW
  - T0H: 2.9 ticks (unacceptable resolution)
```

**Recommendation:** Use MCK/8 (CPRE=3) for DShot, MCK/32 (CPRE=5) for standard PWM.

### 1.5 XDMAC (DMA Controller)

SAMV71 uses XDMAC (Extended DMA Controller):
- 24 DMA channels
- Supports memory-to-peripheral transfers
- Linked list descriptor support

**PWMC DMA Peripheral IDs (from sam_xdmac.h):**
```c
XDMACH_PWM0_TX = 13  /* Single DMA trigger for ALL PWM0 channels */
XDMACH_PWM1_TX = 39  /* Single DMA trigger for ALL PWM1 channels */
```

**CRITICAL:** Each PWMC instance has ONE shared DMA trigger, NOT per-channel.
For DShot with motors on both PWM0 and PWM1, we need:
- 1 DMA channel for PWM0 (Motors 1, 2, 3)
- 1 DMA channel for PWM1 (Motor 4)

---

## Part 2: Implementation Phases

### Phase 1: Basic PWMC PWM (Standard ESC Control)

**Goal:** Replace TC-based PWM with PWMC, 4 channels, 400 Hz, 1000-2000 µs

**Duration:** 3-5 days

### Phase 2: DShot Protocol

**Goal:** Add DShot150/300/600 support using PWMC + XDMAC

**Duration:** 5-7 days

### Phase 3: Bidirectional DShot (Optional)

**Goal:** Add ESC telemetry (RPM, temperature) via bidirectional DShot

**Duration:** 3-5 days (if implemented)

---

## Part 3: Phase 1 - PWMC Basic PWM

### 3.1 Files to Create/Modify

```
platforms/nuttx/src/px4/microchip/samv7/
├── include/px4_arch/
│   ├── hw_description.h                 [MODIFY] - Add PWMC namespace
│   └── io_timer_hw_description.h        [MODIFY] - Add PWMC init functions
├── io_pins/
│   ├── io_timer_pwmc.c                  [CREATE] - PWMC backend
│   └── CMakeLists.txt                   [MODIFY]

boards/microchip/samv71-xult-clickboards/
├── nuttx-config/nsh/defconfig           [MODIFY]
├── src/board_config.h                   [MODIFY]
├── src/timer_config.cpp                 [MODIFY]
└── default.px4board                     [MODIFY]
```

### 3.2 NuttX Configuration

**File:** `nuttx-config/nsh/defconfig`

```diff
# Enable PWMC peripherals (real NuttX config symbols)
+CONFIG_SAMV7_PWM0=y
+CONFIG_SAMV7_PWM0_CH1=y       # PA2 - Motor 2
+CONFIG_SAMV7_PWM0_CH2=y       # PC19 - Motor 3
+CONFIG_SAMV7_PWM0_CH3=y       # PA7 - Motor 1
+CONFIG_SAMV7_PWM1=y
+CONFIG_SAMV7_PWM1_CH1=y       # PA14 - Motor 4

# Keep TC0 CH0 for HRT (CONFIG_SAMV7_TC0 enables TC block 0)
CONFIG_SAMV7_TC0=y

# TC1 and TC3 no longer needed for PWM (can disable if not used elsewhere)
-CONFIG_SAMV7_TC1=y
-CONFIG_SAMV7_TC3=y
```

### 3.3 Hardware Description Extension

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`

Add PWMC namespace after existing Timer namespace:

```cpp
/*
 * PWMC (PWM Controller)
 *
 * PWM0: 4 channels, base 0x40020000
 * PWM1: 4 channels, base 0x4005C000
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
    PeriphA = 0,  /* GPIO_PERIPHA */
    PeriphB = 1,  /* GPIO_PERIPHB */
    PeriphC = 2,  /* GPIO_PERIPHC */
};

struct PWMChannel {
    Instance instance;
    Channel channel;
    Peripheral periph;
};

} // namespace PWMC
```

### 3.4 IO Timer HW Description Extension

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

Add PWMC initialization functions (keep existing TC functions):

```cpp
/*
 * PWMC base addresses (from samv71_memorymap.h)
 */
#ifndef SAM_PWM0_BASE
#define SAM_PWM0_BASE  0x40020000
#endif
#ifndef SAM_PWM1_BASE
#define SAM_PWM1_BASE  0x4005C000
#endif

/**
 * Initialize an IO timer for PWMC
 *
 * Maps PWMC instance to io_timers_t structure using the same fields
 * as TC-based timers for API consistency.
 */
static inline constexpr io_timers_t initIOTimerPWMC(PWMC::Instance instance)
{
    io_timers_t ret{};

    switch (instance) {
    case PWMC::PWM0:
        ret.base = SAM_PWM0_BASE;
        break;
    case PWMC::PWM1:
        ret.base = SAM_PWM1_BASE;
        break;
    }

    ret.clock_register = 0;  /* Handled by io_timer_pwmc.c */
    ret.clock_bit = 0;
    ret.vectorno = 0;

    return ret;
}

/**
 * Initialize a PWMC timer channel with GPIO mapping
 *
 * Uses same timer_io_channels_t fields as TC backend:
 *   - gpio_out: GPIO configuration
 *   - timer_index: Index into io_timers[] array
 *   - timer_channel: PWMC channel number (0-3)
 */
static inline constexpr timer_io_channels_t initIOTimerChannelPWMC(
    const io_timers_t io_timers_conf[MAX_IO_TIMERS],
    unsigned timer_index,
    PWMC::PWMChannel pwm_ch,
    GPIO::GPIOPin pin)
{
    timer_io_channels_t ret{};

    /* Build GPIO configuration
     * NuttX GPIO encoding for SAMV7:
     * - Bits 21-23: Mode (PERIPHA=3, PERIPHB=4, PERIPHC=5)
     * - Bits 5-7:   Port (PIOA=0, PIOB=1, PIOC=2, PIOD=3)
     * - Bits 0-4:   Pin number
     */
    uint32_t gpio_mode;
    switch (pwm_ch.periph) {
    case PWMC::PeriphA: gpio_mode = (3 << 21); break;  /* GPIO_PERIPHA */
    case PWMC::PeriphB: gpio_mode = (4 << 21); break;  /* GPIO_PERIPHB */
    case PWMC::PeriphC: gpio_mode = (5 << 21); break;  /* GPIO_PERIPHC */
    default: gpio_mode = (4 << 21); break;
    }

    uint32_t gpio_port = ((uint32_t)pin.port << 5);
    uint32_t gpio_pin = (uint32_t)pin.pin;

    ret.gpio_out = gpio_mode | gpio_port | gpio_pin;
    ret.gpio_in = 0;

    /* Use existing struct fields consistently with TC backend */
    ret.timer_index = timer_index;
    ret.timer_channel = (uint8_t)pwm_ch.channel;

    return ret;
}
```

### 3.5 Board Configuration

**File:** `src/board_config.h`

```c
/*
 * PWMC Configuration (4-channel motor output)
 *
 * Pin Set A:
 *   Motor 1: PWM0 CH3 -> PA7 (PWMH3)  - Arduino A1
 *   Motor 2: PWM0 CH1 -> PA2 (PWMH1)  - EXT2 pin 9
 *   Motor 3: PWM0 CH2 -> PC19 (PWMH2) - mikroBUS1 PWM
 *   Motor 4: PWM1 CH1 -> PA14 (PWMH1) - EXT2 pin 8
 *
 * NOTE: MAX_IO_TIMERS and MAX_TIMER_IO_CHANNELS are defined in
 *       platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h
 *       Do NOT redefine them here.
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  4

/* PWMC instance indicator for io_timer backend selection */
#define BOARD_IO_TIMER_PWMC         1
```

### 3.6 Timer Configuration

**File:** `src/timer_config.cpp`

```cpp
/**
 * @file timer_config.cpp
 *
 * PWMC-based PWM configuration for SAMV71-XULT
 *
 * Motor mapping:
 *   Motor 1: PWM0 CH3 -> PA7  (Peripheral B)
 *   Motor 2: PWM0 CH1 -> PA2  (Peripheral A)
 *   Motor 3: PWM0 CH2 -> PC19 (Peripheral B)
 *   Motor 4: PWM1 CH1 -> PA14 (Peripheral C)
 */

#include <px4_arch/io_timer_hw_description.h>

/**
 * IO Timer configuration - one entry per PWMC instance used
 *
 * IMPORTANT: We only initialize 2 entries (PWM0 and PWM1) but the array
 * is sized to MAX_IO_TIMERS (currently 4). The remaining entries will be
 * zero-initialized, meaning their .base field = 0.
 *
 * The io_timer_pwmc.c implementation MUST defensively handle base==0:
 *   - io_timer_init_timer() should return -EINVAL for invalid base
 *   - Any code iterating 0..MAX_IO_TIMERS-1 must check for valid base
 *
 * This mirrors the existing TC implementation pattern.
 */
constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimerPWMC(PWMC::PWM0),  /* Timer 0 - Motors 1,2,3 */
    initIOTimerPWMC(PWMC::PWM1),  /* Timer 1 - Motor 4 */
    /* Entries 2 and 3 are zero-initialized (base=0, not used) */
};

/**
 * Timer channel to GPIO pin mapping
 *
 * Uses existing timer_io_channels_t struct fields:
 *   - gpio_out: GPIO configuration for PWMC output
 *   - timer_index: Index into io_timers[] (0 for PWM0, 1 for PWM1)
 *   - timer_channel: PWMC channel number (0-3)
 */
constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    /* Motor 1: PWM0 CH3 -> PA7 (Peripheral B) */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH3, PWMC::PeriphB},
        {GPIO::PortA, GPIO::Pin7}),

    /* Motor 2: PWM0 CH1 -> PA2 (Peripheral A) */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH1, PWMC::PeriphA},
        {GPIO::PortA, GPIO::Pin2}),

    /* Motor 3: PWM0 CH2 -> PC19 (Peripheral B) */
    initIOTimerChannelPWMC(io_timers, 0,
        {PWMC::PWM0, PWMC::CH2, PWMC::PeriphB},
        {GPIO::PortC, GPIO::Pin19}),

    /* Motor 4: PWM1 CH1 -> PA14 (Peripheral C) */
    initIOTimerChannelPWMC(io_timers, 1,
        {PWMC::PWM1, PWMC::CH1, PWMC::PeriphC},
        {GPIO::PortA, GPIO::Pin14}),
};

constexpr io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

### 3.7 IO Timer PWMC Backend

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

```c
/**
 * @file io_timer_pwmc.c
 *
 * SAMV7 IO Timer implementation using PWMC peripheral.
 *
 * Uses the SAME struct fields as TC backend for API consistency:
 *   - io_timers[].base = PWMC base address
 *   - timer_io_channels[].timer_index = index into io_timers[]
 *   - timer_io_channels[].timer_channel = PWMC channel (0-3)
 *   - timer_io_channels[].gpio_out = GPIO configuration
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
#include "hardware/sam_pwm.h"
#include "hardware/sam_pmc.h"

/*
 * PWMC Register Offsets (from SAMV71 datasheet Chapter 47)
 */
#define PWM_CLK_OFFSET      0x000   /* PWM Clock Register */
#define PWM_ENA_OFFSET      0x004   /* PWM Enable Register */
#define PWM_DIS_OFFSET      0x008   /* PWM Disable Register */
#define PWM_SR_OFFSET       0x00C   /* PWM Status Register */
#define PWM_SCM_OFFSET      0x020   /* PWM Sync Channels Mode */
#define PWM_SCUC_OFFSET     0x028   /* PWM Sync Channels Update Control */
#define PWM_SCUP_OFFSET     0x02C   /* PWM Sync Channels Update Period */

/* Channel registers (base + 0x200 + channel * 0x20) */
#define PWM_CH_BASE_OFFSET  0x200
#define PWM_CH_SIZE         0x020
#define PWM_CMR_OFFSET      0x00    /* Channel Mode Register */
#define PWM_CDTY_OFFSET     0x04    /* Channel Duty Cycle */
#define PWM_CDTYUPD_OFFSET  0x08    /* Channel Duty Cycle Update */
#define PWM_CPRD_OFFSET     0x0C    /* Channel Period */
#define PWM_CPRDUPD_OFFSET  0x10    /* Channel Period Update */

/* CMR register bits */
#define PWM_CMR_CPRE_MASK   0x0F    /* Channel Pre-scaler */
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

/*
 * Clock configuration for standard 400 Hz PWM
 * MCK = 150 MHz, Prescaler = 32 (CPRE = 5)
 * PWM clock = 150 MHz / 32 = 4.6875 MHz
 * For 400 Hz: period = 4687500 / 400 = 11718 ticks
 */
#define PWM_STD_PRESCALER   PWM_CMR_CPRE_MCK32
#define PWM_STD_CLOCK_FREQ  (150000000UL / 32)  /* 4.6875 MHz */

#define PWM_DEFAULT_RATE    400
#define PWM_DEFAULT_PERIOD  (PWM_STD_CLOCK_FREQ / PWM_DEFAULT_RATE)

/* PWMC base addresses */
#define SAMV7_PWM0_BASE     0x40020000
#define SAMV7_PWM1_BASE     0x4005C000

/* State tracking */
static bool g_pwmc_initialized[2] = {false, false};
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static uint32_t g_channel_period[MAX_TIMER_IO_CHANNELS];

/*
 * Helper: Get PWMC instance (0 or 1) from channel
 */
static inline int get_pwmc_instance(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -1;
    }

    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    uint32_t base = io_timers[timer_idx].base;

    if (base == SAMV7_PWM0_BASE) return 0;
    if (base == SAMV7_PWM1_BASE) return 1;
    return -1;
}

/*
 * Helper: Get channel base address for register access
 */
static inline uint32_t get_channel_base(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return 0;
    }

    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    uint8_t pwm_ch = timer_io_channels[channel].timer_channel;

    return io_timers[timer_idx].base + PWM_CH_BASE_OFFSET + (pwm_ch * PWM_CH_SIZE);
}

/*
 * Helper: Get PWMC base address
 */
static inline uint32_t get_pwmc_base(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return 0;
    }

    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    return io_timers[timer_idx].base;
}

/*
 * Register access helpers
 */
static inline void pwmc_putreg(uint32_t base, uint32_t offset, uint32_t value)
{
    putreg32(value, base + offset);
}

static inline uint32_t pwmc_getreg(uint32_t base, uint32_t offset)
{
    return getreg32(base + offset);
}

/*
 * Enable PWMC peripheral clock
 */
static void pwmc_enable_clock(int instance)
{
    if (instance == 0) {
        sam_pwm0_enableclk();
    } else if (instance == 1) {
        sam_pwm1_enableclk();
    }
}

/*
 * Initialize PWMC instance
 *
 * NOTE: Defensively handles unused io_timers[] entries (base==0)
 */
int io_timer_init_timer(unsigned timer)
{
    if (timer >= MAX_IO_TIMERS) {
        return -EINVAL;
    }

    /* Defensive check: unused entries have base=0 */
    uint32_t base = io_timers[timer].base;
    if (base == 0) {
        return -EINVAL;
    }

    int instance;
    if (base == SAMV7_PWM0_BASE) {
        instance = 0;
    } else if (base == SAMV7_PWM1_BASE) {
        instance = 1;
    } else {
        return -EINVAL;  /* Unknown base address */
    }

    if (!g_pwmc_initialized[instance]) {
        /* Enable peripheral clock */
        pwmc_enable_clock(instance);
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

    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }

    if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_NotUsed) {
        return -EINVAL;
    }

    /* Initialize the PWMC instance */
    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    int ret = io_timer_init_timer(timer_idx);
    if (ret != OK) {
        return ret;
    }

    if (mode == IOTimerChanMode_PWMOut) {
        uint32_t base = get_pwmc_base(channel);
        uint32_t ch_base = get_channel_base(channel);
        uint8_t pwm_ch = timer_io_channels[channel].timer_channel;

        /* Configure GPIO for PWMC output */
        sam_configgpio(timer_io_channels[channel].gpio_out);

        /* Disable channel first */
        pwmc_putreg(base, PWM_DIS_OFFSET, (1 << pwm_ch));

        /* Configure channel mode:
         * - CPRE = 5 (MCK/32) for standard PWM
         * - Left-aligned (CALG = 0)
         * - Output starts low, goes high on match (CPOL = 0)
         */
        uint32_t cmr = PWM_STD_PRESCALER;
        pwmc_putreg(ch_base, PWM_CMR_OFFSET, cmr);

        /* Set default period (400 Hz) */
        uint32_t period = PWM_DEFAULT_PERIOD;
        pwmc_putreg(ch_base, PWM_CPRD_OFFSET, period);
        g_channel_period[channel] = period;

        /* Set initial duty cycle to disarm value (900 µs) */
        uint32_t duty = (900 * PWM_STD_CLOCK_FREQ) / 1000000UL;
        pwmc_putreg(ch_base, PWM_CDTY_OFFSET, duty);

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

    if (g_channel_modes[channel] != IOTimerChanMode_PWMOut) {
        return -EINVAL;
    }

    uint32_t ch_base = get_channel_base(channel);

    /* Convert microseconds to ticks */
    uint32_t ticks = ((uint32_t)value * PWM_STD_CLOCK_FREQ) / 1000000UL;

    /* Clamp to period */
    if (ticks > g_channel_period[channel]) {
        ticks = g_channel_period[channel];
    }

    /* Use update register for glitch-free update */
    pwmc_putreg(ch_base, PWM_CDTYUPD_OFFSET, ticks);

    return OK;
}

/*
 * Set PWM rate (frequency) for a timer
 */
int io_timer_set_rate(unsigned timer, unsigned rate)
{
    if (timer >= MAX_IO_TIMERS) {
        return -EINVAL;
    }

    if (rate < 50 || rate > 8000) {
        return -EINVAL;
    }

    uint32_t period = PWM_STD_CLOCK_FREQ / rate;

    /* Update all channels using this timer */
    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (timer_io_channels[ch].timer_index == timer &&
            g_channel_modes[ch] == IOTimerChanMode_PWMOut) {

            uint32_t ch_base = get_channel_base(ch);
            pwmc_putreg(ch_base, PWM_CPRDUPD_OFFSET, period);
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

    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (masks & (1 << ch)) {
            if (g_channel_modes[ch] != IOTimerChanMode_PWMOut) {
                continue;
            }

            uint32_t base = get_pwmc_base(ch);
            uint8_t pwm_ch = timer_io_channels[ch].timer_channel;

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
 * Get channel group bitmask for a timer
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

/* Additional required functions... */
int io_timer_validate_channel_index(unsigned channel)
{
    return (channel < MAX_TIMER_IO_CHANNELS) ? 0 : -EINVAL;
}

int io_timer_is_channel_free(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }
    return (g_channel_modes[channel] == IOTimerChanMode_NotUsed) ? 0 : -EBUSY;
}

int io_timer_free_channel(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }
    g_channel_modes[channel] = IOTimerChanMode_NotUsed;
    return OK;
}

int io_timer_get_channel_mode(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return IOTimerChanMode_NotUsed;
    }
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
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return 0;
    }

    uint32_t ch_base = get_channel_base(channel);
    uint32_t ticks = pwmc_getreg(ch_base, PWM_CDTY_OFFSET);

    /* Convert ticks to microseconds */
    return (uint16_t)((ticks * 1000000UL) / PWM_STD_CLOCK_FREQ);
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
    /* For PWMC, updates are applied on next period automatically
     * via the update registers (CDTYUPD, CPRDUPD).
     * No explicit trigger needed for standard PWM.
     */
    (void)channels_mask;
}
```

### 3.8 Build Configuration

**File:** `default.px4board`

```diff
# ESC/Motor Outputs
+CONFIG_DRIVERS_PWM_OUT=y
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`

```cmake
if(CONFIG_DRIVERS_PWM_OUT)
    if(DEFINED CONFIG_BOARD_IO_TIMER_PWMC)
        # PWMC-based PWM output
        px4_add_library(arch_io_pins
            io_timer_pwmc.c
            pwm_servo.c
        )
    else()
        # TC-based PWM output (default)
        px4_add_library(arch_io_pins
            io_timer_tc.c
            pwm_servo.c
        )
    endif()
else()
    px4_add_library(arch_io_pins
        io_timer_stub.c
    )
endif()
```

### 3.9 Phase 1 Testing

```bash
# Build
make microchip_samv71-xult-clickboards_default

# Flash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/.../firmware.bin 0x00400000 verify reset exit"

# Test
nsh> pwm_out status          # Should show 4 channels
nsh> actuator_test set -m 1 -v 0.5   # Test Motor 1 (PA7)
nsh> actuator_test set -m 2 -v 0.5   # Test Motor 2 (PA2)
nsh> actuator_test set -m 3 -v 0.5   # Test Motor 3 (PC19)
nsh> actuator_test set -m 4 -v 0.5   # Test Motor 4 (PA14)
nsh> actuator_test iterate-motors    # Test all motors

# Oscilloscope verification:
# - PA7:  Motor 1 - 400 Hz, variable 900-2000 µs
# - PA2:  Motor 2 - 400 Hz, variable 900-2000 µs
# - PC19: Motor 3 - 400 Hz, variable 900-2000 µs
# - PA14: Motor 4 - 400 Hz, variable 900-2000 µs
```

---

## Part 4: Phase 2 - DShot Implementation

### 4.1 DShot Architecture with Dual PWMC

```
                         ┌──────────────────────────────────┐
                         │         PX4 dshot.c              │
                         │  dshot_set_throttle() for M1-M4  │
                         └──────────┬───────────────────────┘
                                    │
              ┌─────────────────────┴─────────────────────┐
              │                                           │
              ▼                                           ▼
    ┌─────────────────────┐                   ┌─────────────────────┐
    │   PWM0 DMA Buffer   │                   │   PWM1 DMA Buffer   │
    │   Motors 1,2,3      │                   │   Motor 4           │
    │   (51 x uint32_t)   │                   │   (17 x uint32_t)   │
    │   17 bits × 3 ch    │                   │   17 bits × 1 ch    │
    └──────────┬──────────┘                   └──────────┬──────────┘
               │                                         │
               ▼                                         ▼
    ┌─────────────────────┐                   ┌─────────────────────┐
    │    XDMAC CH x       │                   │    XDMAC CH y       │
    │  Periph ID = 13     │                   │  Periph ID = 39     │
    │  (PWM0 TX)          │                   │  (PWM1 TX)          │
    └──────────┬──────────┘                   └──────────┬──────────┘
               │                                         │
               ▼                                         ▼
    ┌─────────────────────┐                   ┌─────────────────────┐
    │       PWM0          │                   │       PWM1          │
    │     PWM_DMAR        │                   │   CH1 CDTYUPD       │
    │  (distributes to    │                   │   (single channel)  │
    │   sync channels)    │                   │                     │
    └──────────┬──────────┘                   └──────────┬──────────┘
               │                                         │
    ┌──────────┼──────────┐                              │
    ▼          ▼          ▼                              ▼
  PA2(M2)   PC19(M3)   PA7(M1)                       PA14(M4)
```

**Key insight:** PWM0 uses the global `PWM_DMAR` register (not per-channel CDTYUPD) which
automatically distributes duty values to synchronized channels in order. PWM1 has only
one channel, so it can write directly to `CH1_CDTYUPD`.

### 4.2 DMA Strategy for Multi-Channel DShot

**Challenge:** PWM0/PWM1 each have ONE DMA trigger for ALL channels.

**Solution:** Use `PWM_DMAR` register + Synchronous Channels Mode (PWM_SCM).

#### 4.2.1 How SAMV7 PWMC Multi-Channel DMA Works

The SAMV7 PWMC has a special DMA mechanism for updating multiple channels:

1. **PWM_DMAR (DMA Register, offset 0x0024):** A single 24-bit holding register
2. **PWM_SCM (Sync Channels Mode, offset 0x0020):** Configures which channels are synchronized
3. **Automatic Distribution:** When DMA writes to `PWM_DMAR`, the PWMC automatically
   transfers the value to the next synchronized channel's `CDTYUPDx` in round-robin order

**Register Configuration (from `sam_pwm.h`):**
```c
/* PWM_SCM - Sync Channels Mode Register */
#define SCM_SYNC_MASK     (0xF << 0)   /* Bits 0-3: Sync channel selection */
#define SCM_UPDM_SHIFT    (16)         /* Bits 16-17: Update mode */
#define SCM_UPDM_MODE0    (0 << 16)    /* Manual (write SCUC.UPDULOCK) */
#define SCM_UPDM_MODE1    (1 << 16)    /* Auto, immediate */
#define SCM_UPDM_MODE2    (2 << 16)    /* Auto, on PWM period */
#define SCM_PTRM          (1 << 20)    /* DMA transfer request mode */
#define SCM_PTRCS_SHIFT   (21)         /* Bits 21-23: Comparison unit for DMA trigger */

/* PWM_DMAR - DMA Register */
#define DMAR_DMADUTY_MASK (0xFFFFFF)   /* Bits 0-23: Duty value written by DMA */
```

#### 4.2.2 DMA Buffer Layout

```c
/*
 * PWM0 DMA Buffer for 3 Synchronized Channels:
 *
 * DMA writes to PWM_DMAR sequentially. The PWMC distributes values
 * to CH1, CH2, CH3 in order (based on SCM_SYNC bits).
 *
 * Buffer layout (interleaved by channel):
 *
 *   Index 0:  CH1_bit15 duty value
 *   Index 1:  CH2_bit15 duty value
 *   Index 2:  CH3_bit15 duty value
 *   Index 3:  CH1_bit14 duty value
 *   Index 4:  CH2_bit14 duty value
 *   ...
 *   Index 48: CH1_reset (0)
 *   Index 49: CH2_reset (0)
 *   Index 50: CH3_reset (0)
 *
 * Total: 17 bits × 3 channels = 51 words
 */

#define PWM0_SYNC_CHANNELS  3  /* CH1, CH2, CH3 */
#define DSHOT_BITS          17 /* 16 data + 1 reset */
#define PWM0_DMA_SIZE       (DSHOT_BITS * PWM0_SYNC_CHANNELS)  /* 51 words */

/* PWM1 only has Motor 4 (CH1), simple linear buffer */
#define PWM1_DMA_SIZE       DSHOT_BITS  /* 17 words */
```

### 4.3 Synchronization Strategy

**Problem:** Motors 1-3 (PWM0) and Motor 4 (PWM1) must output DShot frames simultaneously.

#### 4.3.1 DMA Trigger Mechanism

The XDMACH_PWM0_TX (ID=13) and XDMACH_PWM1_TX (ID=39) peripheral requests are triggered by:

1. **PWM_SCM.PTRM = 0 (End of Period):** DMA request on each PWM period end
2. **PWM_SCM.PTRM = 1 (Comparison Match):** DMA request when comparison unit matches

For DShot, we need one DMA transfer per bit period, so **PTRM = 0** (end of period trigger)
is the correct choice. Each PWM period = one DShot bit.

#### 4.3.2 Synchronization Approach

```c
/*
 * Sync Strategy for PWM0 (3 channels) + PWM1 (1 channel):
 *
 * 1. Configure both PWMC instances with identical bit period
 * 2. PWM0: Use sync mode (SCM) + DMAR for multi-channel update
 * 3. PWM1: Direct CDTYUPD writes (single channel)
 * 4. Start both PWMC counters simultaneously via critical section
 * 5. DMA triggered by end-of-period (PTRM = 0)
 *
 * Timing note: There will be ~10-50ns jitter between PWM0 and PWM1
 * start due to instruction execution time. This is acceptable for
 * DShot (bit period is 1.67µs minimum).
 */

static void dshot_configure_sync(void)
{
    /* PWM0: Configure synchronous channels mode */
    uint32_t scm = 0;
    scm |= (1 << 1) | (1 << 2) | (1 << 3);  /* Sync CH1, CH2, CH3 */
    scm |= SCM_UPDM_MODE2;                   /* Auto update on period */
    scm |= (0 << 20);                        /* PTRM = 0: DMA on period end */
    putreg32(scm, SAM_PWM0_BASE + SAMV7_PWM_SCM);

    /* PWM1: No sync needed (single channel), but still uses period trigger */
    /* DMA writes directly to CH1_CDTYUPD */
}

static void dshot_sync_start(void)
{
    /* Disable both PWMC instances */
    putreg32(0xF, SAM_PWM0_BASE + SAMV7_PWM_DIS);
    putreg32(0xF, SAM_PWM1_BASE + SAMV7_PWM_DIS);

    /* Setup DMA transfers (armed, waiting for peripheral request) */
    /* See section 4.5 for DMA setup details */
    sam_dmastart(dma_handle_pwm0, dshot_dma_callback_pwm0, NULL);
    sam_dmastart(dma_handle_pwm1, dshot_dma_callback_pwm1, NULL);

    /* Enable both PWM instances simultaneously (critical section) */
    irqstate_t flags = enter_critical_section();
    putreg32((1 << 1) | (1 << 2) | (1 << 3), SAM_PWM0_BASE + SAMV7_PWM_ENA);
    putreg32((1 << 1), SAM_PWM1_BASE + SAMV7_PWM_ENA);
    leave_critical_section(flags);

    /* Both PWMC instances now running with <100ns skew */
}
```

### 4.4 DShot Clock Configuration

```c
/*
 * DShot Clock Configuration
 *
 * For DShot600 with good resolution, use MCK/8:
 *   Clock = 150 MHz / 8 = 18.75 MHz
 *   Bit period = 1.67 µs × 18.75 MHz = 31.25 ticks
 *
 * Duty cycles:
 *   T0H (37.5%) = 31.25 × 0.375 = 11.7 → 12 ticks
 *   T1H (75.0%) = 31.25 × 0.750 = 23.4 → 23 ticks
 *
 * Error analysis:
 *   T0H actual = 12/31 = 38.7% (target 37.5%, error +1.2%)
 *   T1H actual = 23/31 = 74.2% (target 75.0%, error -0.8%)
 *   Both within DShot spec tolerance (±5%)
 */

#define DSHOT_PRESCALER     PWM_CMR_CPRE_MCK8   /* MCK/8 = 18.75 MHz */
#define DSHOT_CLOCK_FREQ    (150000000UL / 8)   /* 18.75 MHz */

/* DShot600 timing */
#define DSHOT600_BIT_NS     1670                /* 1.67 µs */
#define DSHOT600_PERIOD     ((DSHOT_CLOCK_FREQ * DSHOT600_BIT_NS) / 1000000000UL)  /* ~31 */
#define DSHOT600_T0H        ((DSHOT600_PERIOD * 375) / 1000)  /* 37.5% = 12 */
#define DSHOT600_T1H        ((DSHOT600_PERIOD * 750) / 1000)  /* 75.0% = 23 */

/* DShot300 timing */
#define DSHOT300_BIT_NS     3330                /* 3.33 µs */
#define DSHOT300_PERIOD     ((DSHOT_CLOCK_FREQ * DSHOT300_BIT_NS) / 1000000000UL)  /* ~62 */
#define DSHOT300_T0H        ((DSHOT300_PERIOD * 375) / 1000)  /* 23 */
#define DSHOT300_T1H        ((DSHOT300_PERIOD * 750) / 1000)  /* 47 */

/* DShot150 timing */
#define DSHOT150_BIT_NS     6670                /* 6.67 µs */
#define DSHOT150_PERIOD     ((DSHOT_CLOCK_FREQ * DSHOT150_BIT_NS) / 1000000000UL)  /* ~125 */
#define DSHOT150_T0H        ((DSHOT150_PERIOD * 375) / 1000)  /* 47 */
#define DSHOT150_T1H        ((DSHOT150_PERIOD * 750) / 1000)  /* 94 */
```

### 4.5 DShot DMA Configuration

**NOTE:** The code below uses the actual NuttX `sam_xdmac.h` API. The flag-based
configuration replaces the struct-based pseudo-API shown in earlier revisions.

```c
/**
 * Configure XDMAC for DShot
 *
 * CRITICAL: Use correct peripheral IDs (from sam_xdmac.h):
 *   - XDMACH_PWM0_TX = 13 (for PWM0 channels via DMAR)
 *   - XDMACH_PWM1_TX = 39 (for PWM1 single channel)
 *
 * DMA Destinations:
 *   - PWM0: Write to PWM_DMAR (0x40020024) - auto-distributes to sync channels
 *   - PWM1: Write to CH1_CDTYUPD (0x4005C208) - single channel
 */

#include "sam_xdmac.h"
#include "hardware/sam_xdmac.h"

/* DMA handles - one per PWMC instance, NOT per motor */
static DMA_HANDLE dma_handle_pwm0;  /* Motors 1,2,3 via PWM0 DMAR */
static DMA_HANDLE dma_handle_pwm1;  /* Motor 4 via PWM1 CH1 */

/* DMA buffers - cache-line aligned for DMA coherency */
static uint32_t dma_buffer_pwm0[PWM0_DMA_SIZE] __attribute__((aligned(32)));
static uint32_t dma_buffer_pwm1[PWM1_DMA_SIZE] __attribute__((aligned(32)));

/* PWMC register addresses */
#define PWM0_DMAR_ADDR      (SAM_PWM0_BASE + SAMV7_PWM_DMAR)    /* 0x40020024 */
#define PWM1_CH1_CDTYUPD    (SAM_PWM1_BASE + 0x208)             /* 0x4005C208 */

int dshot_init_dma(void)
{
    uint32_t chflags;

    /*
     * PWM0 DMA: Memory -> PWM_DMAR (multi-channel via sync mode)
     *
     * NuttX XDMAC flags (from sam_xdmac.h):
     *   DMACH_FLAG_PERIPHPID(n)       - Peripheral ID for handshaking
     *   DMACH_FLAG_PERIPHISPERIPH     - Target is peripheral (not memory)
     *   DMACH_FLAG_PERIPHWIDTH_32BITS - 32-bit transfers
     *   DMACH_FLAG_MEMINCREMENT       - Auto-increment memory address
     *   DMACH_FLAG_MEMBURST_1         - Single beat burst
     */
    chflags = DMACH_FLAG_PERIPHPID(XDMACH_PWM0_TX) |
              DMACH_FLAG_PERIPHISPERIPH |
              DMACH_FLAG_PERIPHWIDTH_32BITS |
              DMACH_FLAG_PERIPHCHUNKSIZE_1 |
              DMACH_FLAG_MEMINCREMENT |
              DMACH_FLAG_MEMBURST_1;

    dma_handle_pwm0 = sam_dmachannel(0, chflags);
    if (dma_handle_pwm0 == NULL) {
        return -ENOMEM;
    }

    /*
     * PWM1 DMA: Memory -> CH1_CDTYUPD (single channel)
     */
    chflags = DMACH_FLAG_PERIPHPID(XDMACH_PWM1_TX) |
              DMACH_FLAG_PERIPHISPERIPH |
              DMACH_FLAG_PERIPHWIDTH_32BITS |
              DMACH_FLAG_PERIPHCHUNKSIZE_1 |
              DMACH_FLAG_MEMINCREMENT |
              DMACH_FLAG_MEMBURST_1;

    dma_handle_pwm1 = sam_dmachannel(0, chflags);
    if (dma_handle_pwm1 == NULL) {
        sam_dmafree(dma_handle_pwm0);
        return -ENOMEM;
    }

    return OK;
}

/**
 * Start DShot DMA transfers
 *
 * Called each time we need to send a new DShot frame.
 * DMA is triggered by PWMC end-of-period events.
 */
int dshot_start_dma(void)
{
    int ret;

    /* Flush cache before DMA (ensure buffer is in main memory) */
    up_clean_dcache((uintptr_t)dma_buffer_pwm0,
                    (uintptr_t)dma_buffer_pwm0 + sizeof(dma_buffer_pwm0));
    up_clean_dcache((uintptr_t)dma_buffer_pwm1,
                    (uintptr_t)dma_buffer_pwm1 + sizeof(dma_buffer_pwm1));

    /* Setup PWM0 DMA: buffer -> DMAR */
    ret = sam_dmatxsetup(dma_handle_pwm0,
                         PWM0_DMAR_ADDR,
                         (uint32_t)dma_buffer_pwm0,
                         PWM0_DMA_SIZE * sizeof(uint32_t));
    if (ret < 0) return ret;

    /* Setup PWM1 DMA: buffer -> CH1_CDTYUPD */
    ret = sam_dmatxsetup(dma_handle_pwm1,
                         PWM1_CH1_CDTYUPD,
                         (uint32_t)dma_buffer_pwm1,
                         PWM1_DMA_SIZE * sizeof(uint32_t));
    if (ret < 0) return ret;

    /* Start both DMA channels (they wait for peripheral trigger) */
    ret = sam_dmastart(dma_handle_pwm0, dshot_dma_callback, (void *)0);
    if (ret < 0) return ret;

    ret = sam_dmastart(dma_handle_pwm1, dshot_dma_callback, (void *)1);
    return ret;
}
```

### 4.6 Files to Create

```
platforms/nuttx/src/px4/microchip/samv7/
├── dshot/
│   ├── dshot.c                          [CREATE] - DShot implementation
│   └── CMakeLists.txt                   [CREATE]
└── include/px4_arch/
    └── dshot.h                          [CREATE] - DShot header
```

### 4.7 Phase 2 Testing

```bash
# Build with DShot enabled
make microchip_samv71-xult-clickboards_default

# Test DShot
nsh> dshot start -m 600        # Start DShot600
nsh> dshot status
nsh> actuator_test set -m 1 -v 0.5

# Oscilloscope verification:
# - All 4 motors: Bit period = 1.67 µs (±5%)
# - Bit 0 high time: ~0.625 µs (37.5%)
# - Bit 1 high time: ~1.25 µs (75.0%)
# - Frame rate: ~32 kHz
# - Check sync: All 4 channels start frame simultaneously
```

---

## Part 5: Implementation Checklist

### Phase 1: PWMC Basic PWM

- [ ] **5.1 NuttX Config**
  - [ ] Add CONFIG_SAMV7_PWM0=y
  - [ ] Add CONFIG_SAMV7_PWM0_CH1=y, CH2=y, CH3=y
  - [ ] Add CONFIG_SAMV7_PWM1=y
  - [ ] Add CONFIG_SAMV7_PWM1_CH1=y
  - [ ] Keep CONFIG_SAMV7_TC0=y (for HRT)

- [ ] **5.2 Hardware Description**
  - [ ] Add PWMC namespace to hw_description.h
  - [ ] Add initIOTimerPWMC() to io_timer_hw_description.h
  - [ ] Add initIOTimerChannelPWMC() function

- [ ] **5.3 Board Config**
  - [ ] Set DIRECT_PWM_OUTPUT_CHANNELS=4
  - [ ] Add BOARD_IO_TIMER_PWMC=1 define

- [ ] **5.4 Timer Config**
  - [ ] Create io_timers[] with 2 entries (PWM0, PWM1)
  - [ ] Create timer_io_channels[] with 4 entries
  - [ ] Verify GPIO peripheral selections (A/B/C)

- [ ] **5.5 IO Timer Backend**
  - [ ] Create io_timer_pwmc.c
  - [ ] Implement io_timer_init_timer()
  - [ ] Implement io_timer_channel_init()
  - [ ] Implement io_timer_set_ccr()
  - [ ] Implement io_timer_set_rate()
  - [ ] Implement io_timer_set_enable()
  - [ ] Implement io_timer_get_group()
  - [ ] Update CMakeLists.txt for PWMC selection

- [ ] **5.6 Testing**
  - [ ] Build succeeds
  - [ ] Boot without crash
  - [ ] pwm_out status shows 4 channels
  - [ ] actuator_test works on all 4 channels
  - [ ] Oscilloscope: 400 Hz, 900-2000 µs pulse width

### Phase 2: DShot

- [ ] **5.7 DShot Clock Config**
  - [ ] Use MCK/8 prescaler (18.75 MHz)
  - [ ] Verify timing calculations for DShot150/300/600
  - [ ] Test with oscilloscope

- [ ] **5.8 DShot DMA Setup**
  - [ ] Allocate 2 DMA channels (PWM0, PWM1)
  - [ ] Configure with correct peripheral IDs (13, 39)
  - [ ] Create interleaved buffer for PWM0
  - [ ] Create simple buffer for PWM1

- [ ] **5.9 PWM0/PWM1 Sync**
  - [ ] Implement sync channel mode for PWM0
  - [ ] Implement simultaneous start
  - [ ] Verify all motors output frames at same time

- [ ] **5.10 DShot Protocol**
  - [ ] Implement packet encoding
  - [ ] Implement CRC calculation
  - [ ] Implement buffer fill
  - [ ] Implement DMA trigger

- [ ] **5.11 DShot Testing**
  - [ ] DShot150 verified on oscilloscope
  - [ ] DShot300 verified on oscilloscope
  - [ ] DShot600 verified on oscilloscope
  - [ ] All 4 motors in sync
  - [ ] ESC responds to commands

---

## Part 6: Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| PWM0/PWM1 sync drift | Medium | High | Use hardware sync features, verify with scope |
| Interleaved DMA complexity | High | Medium | Start with PWM1 (single channel) first |
| Cache coherency issues | Medium | High | Use cache-aligned buffers, flush before DMA |
| DShot timing edge cases | Medium | Medium | Test all 3 speeds, verify with scope |
| PWM_OUT module integration | Low | High | Keep TC fallback option |

---

## Part 7: References

1. **SAMV71 Datasheet**
   - Chapter 47: PWM Controller (PWMC)
   - Chapter 16: XDMAC Controller

2. **NuttX Source**
   - `arch/arm/src/samv7/sam_pwm.c` - PWMC driver reference
   - `arch/arm/src/samv7/sam_xdmac.c` - DMA driver
   - `arch/arm/src/samv7/hardware/sam_pwm.h` - Register definitions
   - `arch/arm/src/samv7/hardware/sam_xdmac.h` - DMA peripheral IDs

3. **PX4 Reference Implementations**
   - `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c`
   - `platforms/nuttx/src/px4/nxp/imxrt/dshot/dshot.c`

4. **Existing SAMV7 Implementation**
   - `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c`
   - `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h`
   - `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`

---

## Appendix A: GPIO Peripheral Mapping

From SAMV71 datasheet, PWMC pin alternate functions:

| Pin | Peripheral A | Peripheral B | Peripheral C | Selected |
|-----|--------------|--------------|--------------|----------|
| PA2 | PWM0_H1 | - | - | Periph A |
| PA7 | - | PWM0_H3 | - | Periph B |
| PA14 | - | - | PWM1_H1 | Periph C |
| PC19 | - | PWM0_H2 | - | Periph B |

---

**Document Status:** Ready for Implementation
**Revision:** 2 (API consistency fixes applied)

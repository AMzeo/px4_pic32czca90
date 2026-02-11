# SAMV71 PWMC Implementation Plan

## Document Info
- **Date**: 2026-02-03
- **Status**: Approved - Ready for Implementation
- **Safe Harbor**: Commit `a86c82886b` with tag `pre-pwmc-checkpoint`

---

## 1. Overview

### 1.1 Objective
Replace the current TC (Timer/Counter) based PWM implementation with PWMC (PWM Controller) for motor control on the SAMV71-XULT board.

### 1.2 Benefits of PWMC over TC
| Aspect | TC (Current) | PWMC (Target) |
|--------|--------------|---------------|
| Channels per module | 1 per timer | 4 per module |
| Independent control | Requires multiple TC blocks | Single PWM0 module |
| DShot potential | Limited | Native support possible |
| Resource usage | TC0=HRT, TC1/TC3=PWM | TC0=HRT only, PWM0=motors |

### 1.3 Scope
- Implement 4-channel PWMC driver for motor PWM
- Keep TC0 for HRT (High Resolution Timer)
- Keep TC1/TC3 for pck6_test and potential RC capture
- Do NOT implement RC input changes (deferred)

---

## 2. Pin Mapping (Option A - Approved)

### 2.1 Motor Pin Assignments
| Motor | Pin | PWMC Channel | NuttX Define | Peripheral | Header Location |
|-------|-----|--------------|--------------|------------|-----------------|
| Motor 1 | PA7 | PWM0_CH3 | `GPIO_PWMC0_H3_3` | B | Arduino A1 |
| Motor 2 | PA2 | PWM0_CH1 | `GPIO_PWMC0_H1_1` | A | EXT2 pin 9 |
| Motor 3 | PC19 | PWM0_CH2 | `GPIO_PWMC0_H2_5` | B | EXT2 pin 7 |
| Motor 4 | PB0 | PWM0_CH0 | `GPIO_PWMC0_H0_2` | A | EXT1 pin 13 |

### 2.2 Pin Verification (from samv71_pinmap.h)
```c
GPIO_PWMC0_H0_2  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOB | GPIO_PIN0)   /* PB0 */
GPIO_PWMC0_H1_1  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)   /* PA2 */
GPIO_PWMC0_H2_5  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)  /* PC19 */
GPIO_PWMC0_H3_3  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)   /* PA7 */
```

### 2.3 PB0 Conflict Resolution
- **Previous use**: `GPIO_MB2_RST` (mikroBUS Socket 2 reset)
- **New use**: Motor 4 PWM output (PWM0_CH0)
- **Action**: Remove `GPIO_MB2_RST` from `PX4_GPIO_INIT_LIST`
- **Impact**: mikroBUS Socket 2 reset functionality lost (acceptable for this board)

---

## 3. Clock Configuration

### 3.1 PWMC Clock Calculation
```
MCK (Master Clock) = 150 MHz
CPRE (Prescaler)   = 3 (MCK/8)
PWM Clock          = 150 MHz / 8 = 18.75 MHz

For 400 Hz PWM:
  CPRD = PWM_Clock / Frequency
  CPRD = 18,750,000 / 400 = 46,875

Duty Cycle Resolution: 46,875 steps per period
Minimum pulse: 1/46875 * 2.5ms = 53.3 ns
```

### 3.2 Clock Constants (io_timer_pwmc.c)
```c
#define PWM_DEFAULT_RATE        400             /* Default PWM frequency (Hz) */
#define PWM_CLOCK_PRESCALER     3               /* CPRE = MCK/8 */
#define PWM_CLOCK_DIVIDER       8               /* Matches CPRE=3 */
#define PWM_MCK_FREQUENCY       150000000UL     /* MCK = 150 MHz */
#define PWM_CLOCK_FREQ          (PWM_MCK_FREQUENCY / PWM_CLOCK_DIVIDER)  /* 18.75 MHz */
```

### 3.3 Harmony CSP Cross-Reference
Verified against `/media/bhanu1234/Development/csp/peripheral/pwm_6343/templates/plib_pwm.c.ftl`:

| Register | Harmony Pattern | Our Implementation |
|----------|-----------------|-------------------|
| CMR | `PWM_CH_NUM[ch].PWM_CMR` | `base + 0x200 + (ch * 0x20)` |
| CPRD | `PWM_CH_NUM[ch].PWM_CPRD` | `base + 0x20C + (ch * 0x20)` |
| CDTY | `PWM_CH_NUM[ch].PWM_CDTY` | `base + 0x204 + (ch * 0x20)` |
| CDTYUPD | `PWM_CH_NUM[ch].PWM_CDTYUPD` | `base + 0x208 + (ch * 0x20)` |
| CPRDUPD | `PWM_CH_NUM[ch].PWM_CPRDUPD` | `base + 0x210 + (ch * 0x20)` |
| ENA | `PWM_REGS->PWM_ENA` | `base + 0x004` |
| DIS | `PWM_REGS->PWM_DIS` | `base + 0x008` |

**Key**: Use `CDTYUPD` (not `CDTY`) for glitch-free duty cycle updates at runtime.

---

## 4. Implementation Steps

### Step 1: Extend hw_description.h

**File**: `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`

**Action**: Add PWM namespace alongside existing Timer namespace

```cpp
/* Add after existing Timer namespace */

namespace PWM
{
enum PWMModule {
    PWM0 = 0,  /* PWM Controller 0 - 4 channels */
    PWM1 = 1,  /* PWM Controller 1 - 4 channels */
};

enum Channel {
    Channel0 = 0,
    Channel1 = 1,
    Channel2 = 2,
    Channel3 = 3,
};

struct PWMChannel {
    PWMModule module;
    Channel channel;
};
}

/* PWM base address helper */
static inline constexpr uint32_t pwmBaseRegister(PWM::PWMModule module)
{
    switch (module) {
    case PWM::PWM0: return SAM_PWM0_BASE;  /* 0x40020000 */
    case PWM::PWM1: return SAM_PWM1_BASE;  /* 0x4005C000 */
    }
    return 0;
}
```

---

### Step 2: Extend io_timer_hw_description.h

**File**: `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

**Action**: Add PWMC initialization helpers

```cpp
/* Add after existing TC helpers */

/**
 * PWMC peripheral function selection
 */
enum class PWMCPeripheral { A, B, C };

/**
 * Initialize a PWM timer (one PWM module = one "timer")
 */
static inline constexpr io_timers_t initIOPWMTimer(PWM::PWMModule module)
{
    io_timers_t ret{};
    ret.base = pwmBaseRegister(module);
    ret.clock_register = 0;  /* Handled by io_timer_pwmc.c */
    ret.clock_bit = 0;
    ret.vectorno = 0;
    return ret;
}

/**
 * Initialize a PWMC channel with GPIO pin mapping
 *
 * NOTE: timer_index is derived from pwm_channel.module:
 *   - PWM0 -> timer_index = 0
 *   - PWM1 -> timer_index = 1
 *
 * If PWM1 is added later:
 *   1. MAX_IO_TIMERS must be >= 2
 *   2. io_timers[] in timer_config.cpp must include both PWM0 and PWM1
 */
static inline constexpr timer_io_channels_t initIOPWMChannel(
    const io_timers_t io_timers_conf[MAX_IO_TIMERS],
    PWM::PWMChannel pwm_channel,
    GPIO::GPIOPin pin,
    PWMCPeripheral periph)
{
    timer_io_channels_t ret{};

    /* GPIO configuration with correct peripheral function */
    uint32_t gpio_mode;
    switch (periph) {
    case PWMCPeripheral::A: gpio_mode = (3 << 21); break;  /* GPIO_PERIPHA */
    case PWMCPeripheral::B: gpio_mode = (4 << 21); break;  /* GPIO_PERIPHB */
    case PWMCPeripheral::C: gpio_mode = (5 << 21); break;  /* GPIO_PERIPHC */
    }
    uint32_t gpio_cfg = (0 << 16);  /* GPIO_CFG_DEFAULT - explicit for consistency */

    ret.gpio_out = gpio_mode | gpio_cfg | ((uint32_t)pin.port << 5) | (uint32_t)pin.pin;
    ret.gpio_in = 0;

    /* Derive timer_index from PWM module */
    ret.timer_index = (uint8_t)pwm_channel.module;  /* PWM0=0, PWM1=1 */
    ret.timer_channel = (uint8_t)pwm_channel.channel;  /* CH0-CH3 */

    return ret;
}
```

---

### Step 3: Update io_timer.h

**File**: `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h`

**Action**: Update comments to reflect PWMC support

```cpp
/**
 * SAMV7 IO Timer implementation
 *
 * Supports two peripheral types:
 * - TC (Timer/Counter): timer_channel 0-2 per TC block (used for HRT, RC capture)
 * - PWMC (PWM Controller): timer_channel 0-3 per PWM module (used for motor PWM)
 */

#ifndef MAX_TIMER_IO_CHANNELS
#define MAX_TIMER_IO_CHANNELS  4  /* 4 PWM channels (PWMC CH0-CH3) */
#endif
```

---

### Step 4: Create io_timer_pwmc.c

**File**: `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

**Action**: Create new PWMC driver

```c
/****************************************************************************
 * SAMV7 IO Timer implementation using PWMC (PWM Controller)
 *
 * PWM generation using PWMC module:
 * - CPRE prescaler in CMR selects clock source
 * - CPRD register = period (determines PWM frequency)
 * - CDTY register = duty cycle
 * - CDTYUPD for glitch-free duty updates
 * - ENA/DIS registers for channel control
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>

#include <px4_arch/io_timer.h>
#include <board_config.h>

#include "arm_internal.h"
#include "sam_gpio.h"
#include "sam_periphclks.h"       /* For sam_pwm0_enableclk() */
#include "hardware/sam_pwm.h"     /* PWMC register definitions */

/* PWM Clock Configuration
 * MCK = 150 MHz, CPRE = 3 (MCK/8) -> PWM clock = 18.75 MHz
 */
#define PWM_DEFAULT_RATE        400
#define PWM_CLOCK_PRESCALER     3               /* CPRE = MCK/8 */
#define PWM_CLOCK_DIVIDER       8
#define PWM_MCK_FREQUENCY       150000000UL
#define PWM_CLOCK_FREQ          (PWM_MCK_FREQUENCY / PWM_CLOCK_DIVIDER)

/* PWM Register Offsets */
#define PWM_CLK_OFFSET          0x000
#define PWM_ENA_OFFSET          0x004
#define PWM_DIS_OFFSET          0x008
#define PWM_SR_OFFSET           0x00C

/* Per-channel register offsets (base + 0x200 + channel * 0x20) */
#define PWM_CH_OFFSET           0x200
#define PWM_CH_SIZE             0x020
#define PWM_CMR_OFFSET          0x00   /* Channel Mode Register */
#define PWM_CDTY_OFFSET         0x04   /* Channel Duty Cycle */
#define PWM_CDTYUPD_OFFSET      0x08   /* Channel Duty Cycle Update */
#define PWM_CPRD_OFFSET         0x0C   /* Channel Period */
#define PWM_CPRDUPD_OFFSET      0x10   /* Channel Period Update */

/* CMR Register bits */
#define CMR_CPRE_SHIFT          0
#define CMR_CPRE_MASK           (0xF << CMR_CPRE_SHIFT)
#define CMR_CPRE(n)             ((n) << CMR_CPRE_SHIFT)
#define CMR_CALG                (1 << 8)   /* Center aligned */
#define CMR_CPOL                (1 << 9)   /* Channel polarity */

/* Channel state tracking */
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static bool g_timers_initialized[MAX_IO_TIMERS];
static uint32_t g_timer_period[MAX_IO_TIMERS];

/* Enable PWM peripheral clock */
static void enable_pwm_clock(unsigned timer)
{
    switch (timer) {
    case 0:
        sam_pwm0_enableclk();  /* PID 31 */
        break;
    case 1:
        sam_pwm1_enableclk();  /* PID 60 */
        break;
    }
}

/* Get channel register base address */
static inline uint32_t get_channel_base(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return 0;
    }

    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    uint8_t pwm_ch = timer_io_channels[channel].timer_channel;

    return io_timers[timer_idx].base + PWM_CH_OFFSET + (pwm_ch * PWM_CH_SIZE);
}

/* Register access helpers */
static inline void pwm_putreg(uint32_t addr, uint32_t value)
{
    putreg32(value, addr);
}

static inline uint32_t pwm_getreg(uint32_t addr)
{
    return getreg32(addr);
}

/**
 * Initialize a timer (PWM module)
 */
int io_timer_init_timer(unsigned timer)
{
    if (timer >= MAX_IO_TIMERS) {
        return -EINVAL;
    }

    if (g_timers_initialized[timer]) {
        return OK;
    }

    /* Enable peripheral clock via PMC */
    enable_pwm_clock(timer);

    /* Set default period for 400Hz PWM */
    g_timer_period[timer] = PWM_CLOCK_FREQ / PWM_DEFAULT_RATE;

    g_timers_initialized[timer] = true;

    return OK;
}

/**
 * Initialize a PWM channel
 */
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
                          channel_handler_t channel_handler, void *context)
{
    (void)channel_handler;
    (void)context;

    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return -EINVAL;
    }

    if (mode != IOTimerChanMode_PWMOut && mode != IOTimerChanMode_NotUsed) {
        return -EINVAL;
    }

    uint8_t timer_idx = timer_io_channels[channel].timer_index;

    /* Initialize the timer if needed */
    int ret = io_timer_init_timer(timer_idx);
    if (ret != OK) {
        return ret;
    }

    uint32_t ch_base = get_channel_base(channel);
    if (ch_base == 0) {
        return -EINVAL;
    }

    if (mode == IOTimerChanMode_PWMOut) {
        uint32_t gpio = timer_io_channels[channel].gpio_out;

        /* Configure GPIO for PWM output */
        sam_configgpio(gpio);

        /* Configure channel mode register:
         * - CPRE = 3 (MCK/8)
         * - CPOL = 1 (high polarity - start high)
         * - CALG = 0 (left-aligned)
         */
        uint32_t cmr = CMR_CPRE(PWM_CLOCK_PRESCALER) | CMR_CPOL;
        pwm_putreg(ch_base + PWM_CMR_OFFSET, cmr);

        /* Set period (CPRD) */
        pwm_putreg(ch_base + PWM_CPRD_OFFSET, g_timer_period[timer_idx]);

        /* Set initial duty cycle to 0 */
        pwm_putreg(ch_base + PWM_CDTY_OFFSET, 0);

        /* Enable the channel */
        uint8_t pwm_ch = timer_io_channels[channel].timer_channel;
        pwm_putreg(io_timers[timer_idx].base + PWM_ENA_OFFSET, (1 << pwm_ch));
    }

    g_channel_modes[channel] = mode;

    return OK;
}

/**
 * Set PWM duty cycle (value in microseconds)
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

    /* Convert microseconds to timer ticks */
    uint32_t ticks = (uint32_t)value * PWM_CLOCK_FREQ / 1000000UL;

    /* Clamp to period */
    uint8_t timer_idx = timer_io_channels[channel].timer_index;
    if (ticks > g_timer_period[timer_idx]) {
        ticks = g_timer_period[timer_idx];
    }

    /* Use CDTYUPD for glitch-free update (Harmony CSP pattern) */
    pwm_putreg(ch_base + PWM_CDTYUPD_OFFSET, ticks);

    return OK;
}

/**
 * Get current duty cycle value
 */
uint16_t io_channel_get_ccr(unsigned channel)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) {
        return 0;
    }

    uint32_t ch_base = get_channel_base(channel);
    uint32_t ticks = pwm_getreg(ch_base + PWM_CDTY_OFFSET);

    /* Convert back to microseconds */
    return (uint16_t)(ticks * 1000000UL / PWM_CLOCK_FREQ);
}

/**
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

    /* Calculate new period */
    uint32_t period = PWM_CLOCK_FREQ / rate;
    g_timer_period[timer] = period;

    /* Update CPRDUPD for all channels using this timer (glitch-free) */
    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (timer_io_channels[ch].timer_index == timer &&
            g_channel_modes[ch] == IOTimerChanMode_PWMOut) {
            uint32_t ch_base = get_channel_base(ch);
            pwm_putreg(ch_base + PWM_CPRDUPD_OFFSET, period);
        }
    }

    return OK;
}

/**
 * Enable/disable PWM channels
 */
int io_timer_set_enable(bool state, io_timer_channel_mode_t mode,
                        io_timer_channel_allocation_t masks)
{
    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if ((masks & (1 << ch)) && g_channel_modes[ch] == mode) {
            uint8_t timer_idx = timer_io_channels[ch].timer_index;
            uint8_t pwm_ch = timer_io_channels[ch].timer_channel;
            uint32_t base = io_timers[timer_idx].base;

            if (state) {
                pwm_putreg(base + PWM_ENA_OFFSET, (1 << pwm_ch));
            } else {
                pwm_putreg(base + PWM_DIS_OFFSET, (1 << pwm_ch));
            }
        }
    }

    return OK;
}

/* ... Additional required functions: io_timer_get_group, io_timer_validate_channel_index,
 * io_timer_is_channel_free, io_timer_free_channel, io_timer_get_channel_mode,
 * io_timer_get_mode_channels, io_timer_channel_get_as_pwm_input,
 * io_timer_unallocate_channel, io_timer_set_pwm_rate, io_timer_trigger
 * (copy structure from io_timer_tc.c, adapting for PWMC registers)
 */
```

---

### Step 5: Update timer_config.cpp

**File**: `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`

**Action**: Replace TC configuration with PWMC

```cpp
/**
 * @file timer_config.cpp
 *
 * Configuration data for SAMV71 PWM driver using PWMC (PWM Controller).
 *
 * SAMV71-XULT PWM Output Configuration (Option A - PWMC):
 *   PWM0 CH0 - Motor 4: PB0  (GPIO_PWMC0_H0_2, Peripheral A, EXT1 pin 13)
 *   PWM0 CH1 - Motor 2: PA2  (GPIO_PWMC0_H1_1, Peripheral A, EXT2 pin 9)
 *   PWM0 CH2 - Motor 3: PC19 (GPIO_PWMC0_H2_5, Peripheral B, EXT2 pin 7)
 *   PWM0 CH3 - Motor 1: PA7  (GPIO_PWMC0_H3_3, Peripheral B, Arduino A1)
 *
 * Note: TC timers (TC0/TC1/TC3) retained for HRT, pck6_test, and RC capture.
 */

#include <px4_arch/io_timer_hw_description.h>

/**
 * Timer configuration - single PWM0 module
 */
const io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOPWMTimer(PWM::PWM0),
};

/**
 * PWM channel to GPIO pin mapping (Motor order: 1, 2, 3, 4)
 */
const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    /* Motor 1: PA7 - PWM0_CH3 - Peripheral B */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel3}, {GPIO::PortA, GPIO::Pin7}, PWMCPeripheral::B),
    /* Motor 2: PA2 - PWM0_CH1 - Peripheral A */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel1}, {GPIO::PortA, GPIO::Pin2}, PWMCPeripheral::A),
    /* Motor 3: PC19 - PWM0_CH2 - Peripheral B */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel2}, {GPIO::PortC, GPIO::Pin19}, PWMCPeripheral::B),
    /* Motor 4: PB0 - PWM0_CH0 - Peripheral A */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel0}, {GPIO::PortB, GPIO::Pin0}, PWMCPeripheral::A),
};

const io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

---

### Step 6: Update board_config.h

**File**: `boards/microchip/samv71-xult-clickboards/src/board_config.h`

**Actions**:
1. Remove TC PWM GPIO definitions
2. Remove `GPIO_MB2_RST` (PB0 now used for Motor 4)
3. Update `BOARD_NUM_IO_TIMERS` to 1
4. Update comments

**Remove:**
```c
/* TC-based PWM - REMOVE THESE */
#define GPIO_PWM1_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN15)
#define GPIO_PWM2_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN23)
#define GPIO_PWM3_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN26)

/* Also remove GPIO_MB2_RST - PB0 repurposed for Motor 4 */
#define GPIO_MB2_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOB|GPIO_PIN0)
```

**Update:**
```c
/* PWMC-based PWM outputs - Option A (4 motors on PWM0)
 * GPIO configs handled in timer_config.cpp via initIOPWMChannel()
 *   Motor 1: PA7  (PWM0_CH3) - Arduino A1
 *   Motor 2: PA2  (PWM0_CH1) - EXT2 pin 9
 *   Motor 3: PC19 (PWM0_CH2) - EXT2 pin 7
 *   Motor 4: PB0  (PWM0_CH0) - EXT1 pin 13 (was MB2_RST)
 */
#define DIRECT_PWM_OUTPUT_CHANNELS  4

/* Number of IO timers (1 = single PWM0 module with 4 channels) */
#define BOARD_NUM_IO_TIMERS 1

/* mikroBUS Socket Reset Pins
 * Socket 1: PA19 (RST) - Active
 * Socket 2: PB0 (RST) - REMOVED: PB0 now used for Motor 4 PWM (PWM0_CH0)
 *           mikroBUS Socket 2 functionality is out of scope for this board.
 */
#define GPIO_MB1_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN19)
/* GPIO_MB2_RST removed - PB0 repurposed for Motor 4 PWM output */
```

**Update PX4_GPIO_INIT_LIST:**
```c
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
    GPIO_SPI0_CS_BMP388,      \
    GPIO_MB1_RST,             \
    /* GPIO_MB2_RST removed - PB0 now Motor 4 PWM */ \
    GPIO_EXT1_RST,            \
    GPIO_EXT2_RST,            \
    /* TC PWM pins removed - using PWMC now */ \
    GPIO_BTN_SAFETY,          \
    GPIO_LED_SAFETY,          \
    GPIO_nARMED_INIT,         \
}
```

---

### Step 7: Update defconfig

**File**: `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

**Add (PWMC):**
```
# PWMC for motor PWM
CONFIG_SAMV7_PWM=y
CONFIG_SAMV7_PWM0=y
CONFIG_SAMV7_PWM0_CH0=y
CONFIG_SAMV7_PWM0_CH1=y
CONFIG_SAMV7_PWM0_CH2=y
CONFIG_SAMV7_PWM0_CH3=y
```

**Keep (unchanged):**
```
CONFIG_SAMV7_TC0=y   # HRT (High Resolution Timer)
CONFIG_SAMV7_TC1=y   # RC capture potential (TC5/PC29)
CONFIG_SAMV7_TC3=y   # pck6_test command
```

---

### Step 8: Update CMakeLists.txt

**File**: `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`

**Change:**
```cmake
px4_add_library(arch_io_pins
    io_timer_pwmc.c   # Changed from io_timer_tc.c
    pwm_servo.c
)
target_link_libraries(arch_io_pins PRIVATE drivers_board)
```

---

## 5. Files Summary

| File | Action | Key Changes |
|------|--------|-------------|
| `hw_description.h` | MODIFY | Add `PWM::` namespace, `PWMModule`, `Channel0-3`, `pwmBaseRegister()` |
| `io_timer_hw_description.h` | MODIFY | Add `initIOPWMTimer()`, `initIOPWMChannel()`, `PWMCPeripheral` enum |
| `io_timer.h` | MODIFY | Update comments for PWMC channels 0-3 |
| `io_timer_pwmc.c` | **CREATE** | PWMC driver with `PWM_CLOCK_FREQ` (NOT TC_CLOCK_FREQ) |
| `timer_config.cpp` | MODIFY | Replace TC with PWMC configuration |
| `board_config.h` | MODIFY | Remove TC PWM + `GPIO_MB2_RST`, add PWMC comments, `BOARD_NUM_IO_TIMERS=1` |
| `defconfig` | MODIFY | Add `CONFIG_SAMV7_PWM0*`, **keep** `TC0/TC1/TC3` |
| `CMakeLists.txt` | MODIFY | Replace `io_timer_tc.c` with `io_timer_pwmc.c` |

---

## 6. Testing Plan

### 6.1 Build Test
```bash
make microchip_samv71-xult-clickboards
```

### 6.2 Boot Test
- Verify system boots without errors
- Check `dmesg` for PWM initialization messages

### 6.3 PWM Output Test
Using oscilloscope on:
- PA7 (Motor 1) - Arduino A1
- PA2 (Motor 2) - EXT2 pin 9
- PC19 (Motor 3) - EXT2 pin 7
- PB0 (Motor 4) - EXT1 pin 13

Expected:
- 400 Hz frequency
- Clean square wave
- Duty cycle responsive to commands

### 6.4 PX4 Commands
```bash
# Test PWM output
pwm test

# Test individual actuators
actuator_test set -m 1 -v 0.5  # Motor 1 at 50%
actuator_test set -m 2 -v 0.5  # Motor 2 at 50%
actuator_test set -m 3 -v 0.5  # Motor 3 at 50%
actuator_test set -m 4 -v 0.5  # Motor 4 at 50%
```

---

## 7. Rollback Plan

If issues are encountered:

1. **Revert to safe harbor:**
   ```bash
   git checkout pre-pwmc-checkpoint
   ```

2. **Or restore TC-based implementation:**
   - Revert `CMakeLists.txt` to use `io_timer_tc.c`
   - Restore TC GPIO definitions in `board_config.h`
   - Remove PWMC configs from `defconfig`

---

## 8. Future Considerations

### 8.1 DShot Support
PWMC can be extended for DShot protocol using:
- DMA for precise timing
- Higher clock rates
- Complementary outputs (H/L pairs)

### 8.2 PWM1 Expansion
If more than 4 PWM channels needed:
1. Add `initIOPWMTimer(PWM::PWM1)` to `io_timers[]`
2. Increase `MAX_IO_TIMERS` to 2
3. Add PWM1 channels to `timer_io_channels[]`
4. Enable `CONFIG_SAMV7_PWM1*` in defconfig

---

## Appendix A: Register Reference

### PWMC Base Addresses
- PWM0: `0x40020000` (SAM_PWM0_BASE)
- PWM1: `0x4005C000` (SAM_PWM1_BASE)

### Peripheral IDs
- PWM0: PID 31 (PCER0 bit 31)
- PWM1: PID 60 (PCER1 bit 28)

### Channel Register Layout
```
Base + 0x200 + (ch * 0x20):
  +0x00: CMR    - Channel Mode Register
  +0x04: CDTY   - Duty Cycle
  +0x08: CDTYUPD - Duty Cycle Update (glitch-free)
  +0x0C: CPRD   - Period
  +0x10: CPRDUPD - Period Update (glitch-free)
  +0x14: CCNT   - Counter
  +0x18: DT     - Dead Time
  +0x1C: DTUPD  - Dead Time Update
```

### CMR Prescaler Values
| CPRE | Clock Source |
|------|--------------|
| 0 | MCK |
| 1 | MCK/2 |
| 2 | MCK/4 |
| 3 | MCK/8 (used) |
| 4 | MCK/16 |
| ... | ... |
| 10 | MCK/1024 |
| 11 | CLKA |
| 12 | CLKB |

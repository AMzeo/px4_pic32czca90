# SAMV71 PWMC Implementation Plan

**Created:** December 6, 2025
**Updated:** January 12, 2026
**Status:** PLANNED - Not Yet Implemented
**Priority:** HIGH (Required for motor output / flight capability)

---

## Executive Summary

Replace TC-based PWM (currently 3 channels) with PWMC (dedicated PWM Controller) for 4 motor outputs using verified conflict-free pins. This enables proper ESC control for quadcopter flight.

**Selected Configuration: Pin Set A**
| Motor | PWMC | Pin | Location | NuttX Macro |
|-------|------|-----|----------|-------------|
| 1 | PWM0 CH3 | PA7 | Arduino A1 | `GPIO_PWMC0_H3_3` |
| 2 | PWM0 CH1 | PA2 | EXT2 pin 9 | `GPIO_PWMC0_H1_1` |
| 3 | PWM0 CH2 | PC19 | mikroBUS1 PWM | `GPIO_PWMC0_H2_5` |
| 4 | PWM1 CH1 | PA14 | EXT2 pin 8 | `GPIO_PWMC1_H1_2` |

---

## 1. Background

### 1.1 Previous TC-Based PWM (Current Implementation)

The current implementation uses Timer Counter (TC) for PWM:
- TC0 CH1 → PA15 (TIOA1) - PWM1
- TC1 CH0 → PC23 (TIOA3) - PWM2
- TC1 CH1 → PC26 (TIOA4) - PWM3
- **Only 3 channels** (TC0 CH2/PA26 removed due to SD card conflict)

### 1.2 Why Switch to PWMC?

| Aspect | TC-Based | PWMC |
|--------|----------|------|
| Channels | 3 | 4+ |
| DMA Support | No | Yes |
| DShot Capable | No | Yes |
| Dedicated Peripheral | No (shares with HRT) | Yes |

### 1.3 Previous PWMC Attempt

A previous PWMC implementation was attempted but crashed at boot. The crash was traced to **PWMOut module issues** (MixingOutput, WorkQueue), not the PWMC code itself. These issues have since been fixed, making PWMC viable again.

---

## 2. Pin Mapping

### 2.1 Goal

Implement **4 ESC outputs** using **PWMC** with **standard PWM at 400 Hz** (2.5 ms period) and pulse widths of **1000–2000 µs** (typical multirotor ESC range). Use **one pin per channel** (`PWMH*` only).

### 2.2 Pin Set A (SELECTED) - Avoids PA0, Uses PC19

This set avoids `PA0` (keeps mikroBUS1 `INT` free) and uses `PC19` (mikroBUS1 `PWM`) for one motor output.

| Motor | PWMC | Pin | Physical Location | NuttX Pinmap Macro | Peripheral |
|-------|------|-----|-------------------|--------------------| -----------|
| Motor 1 | PWM0 CH3 | **PA7** (PWMH3) | Arduino A1 | `GPIO_PWMC0_H3_3` | PERIPH B |
| Motor 2 | PWM0 CH1 | **PA2** (PWMH1) | EXT2 pin 9 | `GPIO_PWMC0_H1_1` | PERIPH A |
| Motor 3 | PWM0 CH2 | **PC19** (PWMH2) | mikroBUS1 PWM | `GPIO_PWMC0_H2_5` | PERIPH B |
| Motor 4 | PWM1 CH1 | **PA14** (PWMH1) | EXT2 pin 8 | `GPIO_PWMC1_H1_2` | PERIPH C |

### 2.3 Pin Set B (ALTERNATIVE) - Avoids PA0 AND PC19

This set keeps mikroBUS1 `PWM` (`PC19`) available for future clickboards by moving one output to PWMC1.

| Motor | PWMC | Pin | Physical Location | NuttX Pinmap Macro | Peripheral |
|-------|------|-----|-------------------|--------------------| -----------|
| Motor 1 | PWM0 CH3 | **PA7** (PWMH3) | Arduino A1 | `GPIO_PWMC0_H3_3` | PERIPH B |
| Motor 2 | PWM0 CH1 | **PA2** (PWMH1) | EXT2 pin 9 | `GPIO_PWMC0_H1_1` | PERIPH A |
| Motor 3 | PWM1 CH3 | **PA8** (PWMH3) | Arduino D10 | `GPIO_PWMC1_H3_1` | PERIPH A |
| Motor 4 | PWM1 CH1 | **PA14** (PWMH1) | EXT2 pin 8 | `GPIO_PWMC1_H1_2` | PERIPH C |

### 2.4 Previously Proposed Pin Set (DEPRECATED) - Uses PA0

This was the original 4-output plan but consumes mikroBUS1 `INT` (`PA0`). Kept for history/reference only.

| Motor | PWMC | Pin | Header Location |
|-------|------|-----|-----------------|
| Motor 1 | PWM0 CH0 | **PA0** (PWMH0) | EXT1 pin 7 |
| Motor 2 | PWM0 CH1 | **PA2** (PWMH1) | EXT2 pin 9 |
| Motor 3 | PWM0 CH2 | **PC19** (PWMH2) | mikroBUS1 PWM |
| Motor 4 | PWM1 CH1 | **PA14** (PWMH1) | EXT2 pin 8 |

### 2.5 Current Pin Usage (Conflict Reference)

```
SPI0:     PD20 (MISO), PD21 (MOSI), PD22 (SPCK)
          PD25 (CS - ICM20689), PD27 (CS - BMP388)
          PD28 (DRDY - ICM20689)
I2C0:     PA3 (SDA), PA4 (SCL)
HSMCI0:   PA25 (MCCK), PA26 (DA2), PA27 (DA3), PA28 (CMD), PA29 (DA0), PA30 (DA1)
USB:      Internal
UART:     PB4 (RX), PD28 (TX)
HRT:      TC0 CH0 internal (no external pins)
LED:      PA23
RST Pins: PA5 (EXT1), PA19 (mikroBUS1), PA24 (EXT2), PB0 (mikroBUS2)
```

### 2.6 Conflict Check Results (Pin Set A)

| PWMC Pin | SPI0? | I2C0? | HSMCI? | HRT? | RST? | Status |
|----------|-------|-------|--------|------|------|--------|
| PA7 | No | No | No | N/A | No | ✅ SAFE |
| PA2 | No | No | No | N/A | No | ✅ SAFE |
| PC19 | No | No | No | N/A | No | ✅ SAFE (uses mikroBUS1 PWM) |
| PA14 | No | No | No | N/A | No | ✅ SAFE |

### 2.7 Pins Explicitly AVOIDED

| Pin | Reason | Consequence if Used |
|-----|--------|---------------------|
| **PA26** | HSMCI0 DA2 (SD card data line 2) | SD card write corruption! |
| **PA0** | mikroBUS1 INT | Loses sensor interrupt capability |
| **PA3/PA4** | I2C0 bus (TWIHS0) | All I2C sensors broken |
| **PD20-22** | SPI0 bus | SPI sensors broken |
| **PD25** | ICM20689 CS | IMU fails |
| **PD27** | BMP388 CS | Barometer fails |
| **PD28** | ICM20689 DRDY | IMU data ready broken |
| **PA5/PA19/PA24/PB0** | Reset pins | Click board reset broken |

### 2.8 HRT Pin Clarification

**Important:** HRT (High Resolution Timer) uses TC0 Channel 0 **INTERNALLY ONLY**:
- Uses counter register (CV), compare registers (RA/RC), and interrupt
- Does **NOT** use external TIOA0/TIOB0 pins (PA0/PA1)
- Therefore **PA0 is FREE** for PWMC use (but we avoid it to keep mikroBUS1 INT available)

---

## 3. PWMH vs PWML Explanation

### 3.1 Complementary Outputs

SAMV71 PWMC channels have complementary outputs:
- **PWMH** (High) and **PWML** (Low) are inverted versions of each other
- When PWMH is HIGH, PWML is LOW (and vice versa)
- They share the same frequency and duty cycle
- Designed for H-bridge motor drivers

### 3.2 For ESC Control

**Use only ONE output per motor** (PWMH or PWML, not both):
```
ESC expects: ───┐     ┌───────────────────┐     ┌───
               │     │                   │     │
               └─────┘                   └─────┘
               ← 1-2ms →
               (pulse width = throttle)
```

Standard ESC PWM:
- Frequency: 50-400 Hz (**400 Hz typical for multirotors**)
- Pulse width: 1000 µs (0%) to 2000 µs (100%)
- Arm pulse: ~1000 µs for 2 seconds

---

## 4. Files to Modify

### 4.1 NuttX Configuration

**File:** `nuttx-config/nsh/defconfig`

```diff
# Enable PWMC peripherals
+CONFIG_SAMV7_PWM0=y
+CONFIG_SAMV7_PWM0_CH1=y       # PA2 - Motor 2
+CONFIG_SAMV7_PWM0_CH2=y       # PC19 - Motor 3
+CONFIG_SAMV7_PWM0_CH3=y       # PA7 - Motor 1
+CONFIG_SAMV7_PWM1=y
+CONFIG_SAMV7_PWM1_CH1=y       # PA14 - Motor 4

# Keep TC0 for HRT only
CONFIG_SAMV7_TC0=y
CONFIG_SAMV7_TC0_CH0=y

# Remove unused TC channels (previously used for PWM)
-CONFIG_SAMV7_TC0_CH1=y
-CONFIG_SAMV7_TC1=y
-CONFIG_SAMV7_TC1_CH0=y
-CONFIG_SAMV7_TC1_CH1=y
```

### 4.2 Board Configuration

**File:** `src/board_config.h`

**Remove old TC-based PWM definitions:**
```diff
-/* PWM Timer Configuration using TC */
-#define GPIO_PWM1_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN15)
-#define GPIO_PWM2_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN23)
-#define GPIO_PWM3_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN26)
-#define GPIO_RC_INPUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN29)
```

**Add new PWMC configuration:**
```c
/* PWM Configuration - Using PWMC (NOT Timer Counter)
 *
 * Pin Set A (Selected):
 *   Motor 1: PWM0 CH3 -> PA7 (PWMH3) - Arduino A1
 *   Motor 2: PWM0 CH1 -> PA2 (PWMH1) - EXT2 pin 9
 *   Motor 3: PWM0 CH2 -> PC19 (PWMH2) - mikroBUS1 PWM
 *   Motor 4: PWM1 CH1 -> PA14 (PWMH1) - EXT2 pin 8
 *
 * PA0 kept FREE for mikroBUS1 INT (future sensor interrupts)
 *
 * CRITICAL PIN AVOIDANCE:
 *   - PA26 NOT USED (SD card DA2 conflict - caused corruption!)
 *   - PA0 NOT USED (preserve mikroBUS1 INT capability)
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  4
#define BOARD_NUM_IO_TIMERS         4

/* PWMC GPIO configurations - from samv71_pinmap.h */
#define GPIO_PWM0_CH3_PWMH  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)   /* Motor 1 */
#define GPIO_PWM0_CH1_PWMH  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)   /* Motor 2 */
#define GPIO_PWM0_CH2_PWMH  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)  /* Motor 3 */
#define GPIO_PWM1_CH1_PWMH  (GPIO_PERIPHC | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN14)  /* Motor 4 */
```

**Update GPIO init list:**
```c
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
    GPIO_SPI0_CS_BMP388,      \
    GPIO_MB1_RST,             \
    GPIO_MB2_RST,             \
    GPIO_EXT1_RST,            \
    GPIO_EXT2_RST,            \
    GPIO_PWM0_CH3_PWMH,       \
    GPIO_PWM0_CH1_PWMH,       \
    GPIO_PWM0_CH2_PWMH,       \
    GPIO_PWM1_CH1_PWMH,       \
}
```

### 4.3 Timer Configuration

**File:** `src/timer_config.cpp`

```cpp
/**
 * @file timer_config.cpp
 *
 * SAMV71-XULT PWM Configuration using PWMC (PWM Controller)
 *
 * Pin Set A:
 *   Motor 1: PWM0 CH3 -> PA7 (PWMH3) - Arduino A1
 *   Motor 2: PWM0 CH1 -> PA2 (PWMH1) - EXT2 pin 9
 *   Motor 3: PWM0 CH2 -> PC19 (PWMH2) - mikroBUS1 PWM
 *   Motor 4: PWM1 CH1 -> PA14 (PWMH1) - EXT2 pin 8
 */

#include <px4_arch/io_timer_hw_description.h>

/**
 * IO Timer configuration - maps PWMC channels to PX4 timer abstraction
 *
 * Note: The exact API depends on io_timer_hw_description.h for SAMV7.
 * May need to use initIOPWMC() or similar if available.
 */
const io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::PWM0, 3),   /* PWM0 CH3 - Motor 1 (PA7) */
    initIOTimer(Timer::PWM0, 1),   /* PWM0 CH1 - Motor 2 (PA2) */
    initIOTimer(Timer::PWM0, 2),   /* PWM0 CH2 - Motor 3 (PC19) */
    initIOTimer(Timer::PWM1, 1),   /* PWM1 CH1 - Motor 4 (PA14) */
};

/**
 * Timer channel to GPIO pin mapping
 */
const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel3}, {GPIO::PortA, GPIO::Pin7}),
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel1}, {GPIO::PortA, GPIO::Pin2}),
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel2}, {GPIO::PortC, GPIO::Pin19}),
    initIOTimerChannel(io_timers, {Timer::PWM1, Timer::Channel1}, {GPIO::PortA, GPIO::Pin14}),
};

const io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

### 4.4 IO Timer Backend

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

This file implements PWMC register access. Key functions:

```c
/**
 * Initialize a PWMC timer instance
 * - Enable peripheral clock via PMC
 * - Configure channel for waveform mode
 * - Set default frequency (400 Hz)
 */
int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode);

/**
 * Set PWM frequency (affects all channels on same PWMC instance)
 * - Calculate CPRD from: frequency = MCK / (prescaler * CPRD)
 * - For 400 Hz with MCK=150MHz, prescaler=32: CPRD = 11718
 */
int io_timer_set_rate(unsigned timer, unsigned rate);

/**
 * Set duty cycle (pulse width in timer ticks)
 * - Calculate CDTY from microseconds
 * - 1500 µs at 4.6875 MHz = 7031 ticks
 */
int io_timer_set_ccr(unsigned channel, uint16_t value);

/**
 * Initialize a specific channel
 * - Configure GPIO for PWMC peripheral function
 * - Enable channel output
 */
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
                          channel_handler_t handler, void *context);

/**
 * Get channel group bitmask for a timer
 */
uint32_t io_timer_get_group(unsigned timer);
```

**PWMC Register Programming Reference (SAMV71 Datasheet Chapter 47):**

```c
/* Base addresses */
#define SAM_PWM0_BASE  0x40020000
#define SAM_PWM1_BASE  0x4005C000

/* Key registers per channel (offset from base + 0x200 + ch*0x20) */
#define PWM_CMR_OFFSET   0x00  /* Channel Mode Register */
#define PWM_CDTY_OFFSET  0x04  /* Channel Duty Cycle */
#define PWM_CPRD_OFFSET  0x08  /* Channel Period */

/* Global registers */
#define PWM_CLK_OFFSET   0x00  /* PWM Clock Register */
#define PWM_ENA_OFFSET   0x04  /* PWM Enable Register */
#define PWM_DIS_OFFSET   0x08  /* PWM Disable Register */
#define PWM_SR_OFFSET    0x0C  /* PWM Status Register */

/* Example configuration for 400 Hz PWM */
/*
 * MCK = 150 MHz
 * Prescaler = 32 (CPRE = 5 in CMR)
 * PWM clock = 150 MHz / 32 = 4.6875 MHz
 * Period for 400 Hz = 4687500 / 400 = 11718 ticks
 * Duty for 1500 µs = 1500 * 4.6875 = 7031 ticks
 */
#define PWM_PRESCALER    5      /* CPRE value for /32 */
#define PWM_PERIOD_400HZ 11718  /* CPRD for 400 Hz */
#define PWM_DUTY_1500US  7031   /* CDTY for 1500 µs */
#define PWM_DUTY_1000US  4687   /* CDTY for 1000 µs (min throttle) */
#define PWM_DUTY_2000US  9375   /* CDTY for 2000 µs (max throttle) */
```

### 4.5 Hardware Description Header (if needed)

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

May need to add PWMC-specific types:

```cpp
namespace PWMC {
    enum PWM : uint8_t { PWM0 = 0, PWM1 = 1 };
    enum Channel : uint8_t { Channel0 = 0, Channel1 = 1, Channel2 = 2, Channel3 = 3 };
}

/* Helper to initialize PWMC-based io_timer */
static inline constexpr io_timers_t initIOPWMC(PWMC::PWM pwm, PWMC::Channel channel) {
    return io_timers_t {
        .base = (pwm == PWMC::PWM0) ? SAM_PWM0_BASE : SAM_PWM1_BASE,
        .clock_register = (pwm == PWMC::PWM0) ? SAM_PMC_PWM0 : SAM_PMC_PWM1,
        .vectorno = 0,  /* Not using interrupts for basic PWM */
        .channel = static_cast<uint8_t>(channel),
        .is_pwmc = true,
    };
}
```

### 4.6 Build Configuration

**File:** `default.px4board`

```diff
+CONFIG_DRIVERS_PWM_OUT=y
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`

```cmake
if(CONFIG_DRIVERS_PWM_OUT)
    px4_add_library(arch_io_pins
        io_timer_pwmc.c
        pwm_servo.c
    )
    target_include_directories(arch_io_pins PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
else()
    px4_add_library(arch_io_pins
        io_timer_tc_stub.c
    )
endif()
```

---

## 5. Implementation Steps

### Phase 1: Preparation
- [ ] Backup current TC-based configuration
- [ ] Verify NuttX PWMC driver exists (`sam_pwm.c`)
- [ ] Study io_timer interface requirements
- [ ] Review existing `io_timer_pwmc.c` stub code

### Phase 2: NuttX Config
- [ ] Add CONFIG_SAMV7_PWM0=y and CONFIG_SAMV7_PWM1=y
- [ ] Add CONFIG_SAMV7_PWM0_CH1=y, CH2=y, CH3=y
- [ ] Add CONFIG_SAMV7_PWM1_CH1=y
- [ ] Remove unused TC channel configs

### Phase 3: Board Config
- [ ] Remove old TC-based GPIO_PWM*_OUT definitions
- [ ] Add PWMC GPIO definitions (GPIO_PWM0_CH*_PWMH, GPIO_PWM1_CH1_PWMH)
- [ ] Update DIRECT_PWM_OUTPUT_CHANNELS to 4
- [ ] Update BOARD_NUM_IO_TIMERS to 4
- [ ] Update PX4_GPIO_INIT_LIST with new pins

### Phase 4: IO Timer Backend
- [ ] Review existing io_timer_pwmc.c
- [ ] Implement io_timer_init_timer() with PWMC register programming
- [ ] Implement io_timer_set_rate() for frequency control
- [ ] Implement io_timer_set_ccr() for duty cycle control
- [ ] Implement io_timer_channel_init() for GPIO configuration
- [ ] Implement io_timer_get_group() returning correct bitmask
- [ ] Update CMakeLists.txt

### Phase 5: Timer Config
- [ ] Rewrite timer_config.cpp for PWMC channels
- [ ] Update io_timer_hw_description.h if needed (add PWMC namespace)
- [ ] Verify MAX_IO_TIMERS and MAX_TIMER_IO_CHANNELS are >= 4

### Phase 6: Testing
- [ ] Build and verify no compilation errors
- [ ] Flash and verify boot without crash
- [ ] Test pwm_out driver starts
- [ ] Verify 4 channels reported
- [ ] Test each channel with oscilloscope
- [ ] Run SD card regression tests
- [ ] Verify sensors still work

---

## 6. Testing Plan

### 6.1 Build Test
```bash
make microchip_samv71-xult-clickboards_default

# Check binary size
arm-none-eabi-size build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf
```

### 6.2 Boot Test
```bash
# Flash and connect via USB serial
# Should not crash at boot
# Look for: pwm_out driver loaded successfully
nsh> dmesg | grep -i pwm
```

### 6.3 PWM Output Tests
```bash
nsh> pwm_out start
nsh> pwm_out status      # Should show 4 channels
nsh> pwm test -c 1 -p 1500   # Test Motor 1 (PA7) at 1500 µs
nsh> pwm test -c 2 -p 1500   # Test Motor 2 (PA2)
nsh> pwm test -c 3 -p 1500   # Test Motor 3 (PC19)
nsh> pwm test -c 4 -p 1500   # Test Motor 4 (PA14)
```

### 6.4 Oscilloscope Verification

| Test | Expected Value |
|------|----------------|
| Frequency | 400 Hz (2.5 ms period) |
| Min pulse | 1000 µs |
| Mid pulse | 1500 µs |
| Max pulse | 2000 µs |
| Idle pulse | 900-1000 µs |
| Rise time | < 100 ns |

**Probe Points:**
| Motor | Pin | Header |
|-------|-----|--------|
| 1 | PA7 | Arduino A1 |
| 2 | PA2 | EXT2 pin 9 |
| 3 | PC19 | mikroBUS1 PWM |
| 4 | PA14 | EXT2 pin 8 |

### 6.5 Regression Tests (CRITICAL)

| Test | Command | Must Pass |
|------|---------|-----------|
| SD Card Write | `param save` | No corruption |
| SD Card Read | `param load` | Correct values |
| IMU Data | `listener sensor_accel` | Data flowing |
| Baro Data | `listener sensor_baro` | Data flowing |
| I2C Bus | `i2cdetect -b 1` | No errors |

---

## 7. Rollback Plan

If PWMC implementation fails:

1. Revert to TC-based PWM (backup configuration)
2. Use existing 3-channel TC configuration:
   - PA15 (TIOA1) - Motor 1
   - PC23 (TIOA3) - Motor 2
   - PC26 (TIOA4) - Motor 3
3. Accept 3-motor limitation until fix found

**Backup files before starting:**
```bash
cp src/board_config.h src/board_config.h.tc_backup
cp src/timer_config.cpp src/timer_config.cpp.tc_backup
cp nuttx-config/nsh/defconfig nuttx-config/nsh/defconfig.tc_backup
```

---

## 8. Final Pin Map (Pin Set A)

```
SAMV71-XULT Pin Usage with PWMC
================================

Port A:
  PA0  - FREE (mikroBUS1 INT - preserved for future sensors)
  PA2  - PWMC0 PWMH1 (Motor 2) ← NEW
  PA3  - I2C0 SDA
  PA4  - I2C0 SCL
  PA5  - EXT1 RST
  PA7  - PWMC0 PWMH3 (Motor 1) ← NEW
  PA14 - PWMC1 PWMH1 (Motor 4) ← NEW
  PA15 - (was TC PWM, now free)
  PA19 - mikroBUS1 RST
  PA23 - LED Blue
  PA24 - EXT2 RST
  PA25 - HSMCI0 MCCK
  PA26 - HSMCI0 DA2 ← PROTECTED! (SD card)
  PA27-30 - HSMCI0 (SD card)

Port B:
  PB0  - mikroBUS2 RST

Port C:
  PC19 - PWMC0 PWMH2 (Motor 3) ← NEW
  PC23 - (was TC PWM, now free)
  PC26 - (was TC PWM, now free)

Port D:
  PD20 - SPI0 MISO
  PD21 - SPI0 MOSI
  PD22 - SPI0 SPCK
  PD25 - SPI0 CS (ICM20689)
  PD27 - SPI0 CS (BMP388)
  PD28 - SPI0 DRDY (ICM20689)
```

---

## 9. Physical Header Locations

```
Arduino Headers:
┌─────────────────────────────────────┐
│ A1 (PA7)  ← Motor 1 (PWMC0 CH3)     │
│ D10 (PA8) ← Available (Pin Set B)   │
└─────────────────────────────────────┘

EXT2 Header:
┌─────────────────────────────────────┐
│ Pin 8 (PA14) ← Motor 4 (PWMC1 CH1)  │
│ Pin 9 (PA2)  ← Motor 2 (PWMC0 CH1)  │
│ Pin 10 (PA24) - EXT2 RST            │
│ Pin 15 (PD27) - BMP388 CS           │
└─────────────────────────────────────┘

EXT1 Header:
┌─────────────────────────────────────┐
│ Pin 7 (PA0)  - FREE (was Motor 1)   │
│ Pin 9 (PD28) - ICM20689 DRDY        │
│ Pin 10 (PA5) - EXT1 RST             │
│ Pin 15 (PD25) - ICM20689 CS         │
└─────────────────────────────────────┘

mikroBUS Socket 1:
┌─────────────────────────────────────┐
│ PWM (PC19) ← Motor 3 (PWMC0 CH2)    │
│ INT (PA0)  - FREE for sensors       │
│ RST (PA19) - Click board reset      │
└─────────────────────────────────────┘
```

---

## 10. References

- SAMV71 Datasheet: Chapter 47 (PWM Controller)
- NuttX `samv71_pinmap.h` - Pin multiplexing definitions
- NuttX `sam_pwm.c` - PWMC driver reference
- `PWM_INVESTIGATION_SUMMARY.md` - Previous investigation
- `TASK_DSHOT_PWM_IMPLEMENTATION.md` - DShot/PWM task description
- STM32 `io_timer.c` - Reference implementation pattern

---

## 11. Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-06 | Initial creation with PA0-based pin set |
| 2.0 | 2026-01-12 | Updated to use Pin Set A (PA7, PA2, PC19, PA14), fixed all code examples, corrected SPI pin references, added physical header locations |

---

**Document Status:** Ready for Implementation

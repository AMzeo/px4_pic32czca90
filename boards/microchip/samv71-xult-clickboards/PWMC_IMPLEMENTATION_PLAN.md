# SAMV71 PWMC Implementation Plan

**Created:** December 6, 2025
**Status:** PLANNED - Not Yet Implemented
**Priority:** HIGH (Required for motor output / flight capability)

---

## Executive Summary

Replace TC-based PWM (currently 3 channels) with PWMC (dedicated PWM Controller) for 4 motor outputs using verified conflict-free pins. This enables proper ESC control for quadcopter flight.

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

### 2.1 Selected PWMC Pins

| Motor | PWMC | Pin | Header Location | Verified Safe |
|-------|------|-----|-----------------|---------------|
| Motor 1 | PWM0 CH0 | **PA0** (PWMH0) | EXT1 pin 7 | ✓ |
| Motor 2 | PWM0 CH1 | **PA2** (PWMH1) | EXT2 pin 9 | ✓ |
| Motor 3 | PWM0 CH2 | **PC19** (PWMH2) | mikroBUS1 PWM | ✓ |
| Motor 4 | PWM1 CH1 | **PA14** (PWMH1) | EXT2 pin 8 | ✓ |

### 2.2 Pin Conflict Verification

**Current Pin Usage:**
```
SPI0:     PD20 (MISO), PD21 (MOSI), PD22 (SPCK), PA11 (CS), PA12 (DRDY)
I2C0:     PA3 (SDA), PA4 (SCL)
HSMCI0:   PA25, PA26, PA27, PA28, PA29, PA30 (SD Card)
USB:      Internal
UART:     PB4 (RX), PD28 (TX)
HRT:      TC0 CH0 internal (no external pins)
LED:      PA23
```

**Conflict Check Results:**

| PWMC Pin | SPI0? | I2C0? | HSMCI? | HRT? | Status |
|----------|-------|-------|--------|------|--------|
| PA0 | No | No | No | Internal only | ✓ SAFE |
| PA2 | No | No | No | N/A | ✓ SAFE |
| PC19 | No | No | No | N/A | ✓ SAFE |
| PA14 | No | No | No | N/A | ✓ SAFE |

### 2.3 Pins Explicitly AVOIDED

| Pin | Reason | Consequence if Used |
|-----|--------|---------------------|
| **PA26** | HSMCI0 DA2 (SD card data line 2) | SD card write corruption! |
| **PA12** | ICM20689 DRDY interrupt | IMU data ready broken |
| **PA3/PA4** | I2C0 bus (TWIHS0) | All I2C sensors broken |
| **PD20-22** | SPI0 bus | SPI sensors broken |

### 2.4 HRT Pin Clarification

**Important:** HRT (High Resolution Timer) uses TC0 Channel 0 **INTERNALLY ONLY**:
- Uses counter register (CV), compare registers (RA/RC), and interrupt
- Does **NOT** use external TIOA0/TIOB0 pins (PA0/PA1)
- Therefore **PA0 is FREE** for PWMC use

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
- Frequency: 50-400 Hz (400 Hz typical for multirotors)
- Pulse width: 1000 µs (0%) to 2000 µs (100%)
- Arm pulse: ~1000 µs for 2 seconds

---

## 4. Files to Modify

### 4.1 NuttX Configuration

**File:** `nuttx-config/nsh/defconfig`

```diff
# Enable PWMC peripherals
+CONFIG_SAMV7_PWM0=y
+CONFIG_SAMV7_PWM0_CH0=y
+CONFIG_SAMV7_PWM0_CH1=y
+CONFIG_SAMV7_PWM0_CH2=y
+CONFIG_SAMV7_PWM1=y
+CONFIG_SAMV7_PWM1_CH1=y

# Keep TC0 for HRT only
CONFIG_SAMV7_TC0=y
CONFIG_SAMV7_TC0_CH0=y
-CONFIG_SAMV7_TC0_CH1=y
-CONFIG_SAMV7_TC1_CH0=y
-CONFIG_SAMV7_TC1_CH1=y
```

### 4.2 Board Configuration

**File:** `src/board_config.h`

```c
/* PWM Configuration - Using PWMC (NOT Timer Counter)
 *
 * Motor 1: PWM0 CH0 -> PA0 (PWMH0) - EXT1 pin 7
 * Motor 2: PWM0 CH1 -> PA2 (PWMH1) - EXT2 pin 9
 * Motor 3: PWM0 CH2 -> PC19 (PWMH2) - mikroBUS1 PWM
 * Motor 4: PWM1 CH1 -> PA14 (PWMH1) - EXT2 pin 8
 *
 * CRITICAL PIN AVOIDANCE:
 *   - PA26 NOT USED (SD card DA2 conflict - caused corruption!)
 *   - PA12 NOT USED (ICM20689 DRDY conflict)
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  4

/* PWMC GPIO configurations */
#define GPIO_PWM0_CH0_PWMH  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN0)
#define GPIO_PWM0_CH1_PWMH  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)
#define GPIO_PWM0_CH2_PWMH  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)
#define GPIO_PWM1_CH1_PWMH  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN14)
```

### 4.3 Timer Configuration

**File:** `src/timer_config.cpp`

```cpp
#include <px4_arch/io_timer_hw_description.h>

const io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::PWM0, 0),   /* PWM0 CH0 - Motor 1 */
    initIOTimer(Timer::PWM0, 1),   /* PWM0 CH1 - Motor 2 */
    initIOTimer(Timer::PWM0, 2),   /* PWM0 CH2 - Motor 3 */
    initIOTimer(Timer::PWM1, 1),   /* PWM1 CH1 - Motor 4 */
};

const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel0}, {GPIO::PortA, GPIO::Pin0}),
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel1}, {GPIO::PortA, GPIO::Pin2}),
    initIOTimerChannel(io_timers, {Timer::PWM0, Timer::Channel2}, {GPIO::PortC, GPIO::Pin19}),
    initIOTimerChannel(io_timers, {Timer::PWM1, Timer::Channel1}, {GPIO::PortA, GPIO::Pin14}),
};
```

### 4.4 IO Timer Backend

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

New file implementing PWMC register access:
- `io_timer_init_timer()` - Initialize PWMC peripheral
- `io_timer_set_ccr()` - Set duty cycle
- `io_timer_set_rate()` - Set PWM frequency
- Channel enable/disable functions

### 4.5 Build Configuration

**File:** `default.px4board`

```diff
+CONFIG_DRIVERS_PWM_OUT=y
+CONFIG_BOARD_IO_TIMER_PWMC=y
```

---

## 5. Implementation Steps

### Phase 1: Preparation
- [ ] Backup current TC-based configuration
- [ ] Verify NuttX PWMC driver exists (`sam_pwm.c`)
- [ ] Study io_timer interface requirements

### Phase 2: NuttX Config
- [ ] Enable CONFIG_SAMV7_PWM0 and CONFIG_SAMV7_PWM1
- [ ] Enable required PWM channels
- [ ] Disable unused TC channels

### Phase 3: Board Config
- [ ] Add PWMC GPIO definitions to board_config.h
- [ ] Update DIRECT_PWM_OUTPUT_CHANNELS to 4
- [ ] Update PX4_GPIO_INIT_LIST

### Phase 4: IO Timer Backend
- [ ] Create io_timer_pwmc.c
- [ ] Implement all required io_timer functions
- [ ] Update CMakeLists.txt

### Phase 5: Timer Config
- [ ] Update timer_config.cpp for PWMC
- [ ] Add PWM0/PWM1 to io_timer_hw_description.h if needed

### Phase 6: Testing
- [ ] Build and flash
- [ ] Verify boot without crash
- [ ] Test pwm_out driver
- [ ] Verify with oscilloscope
- [ ] Run SD card regression tests
- [ ] Verify sensors still work

---

## 6. Testing Plan

### 6.1 Boot Test
```bash
# Should not crash at boot
# Look for: pwm_out driver loaded successfully
```

### 6.2 PWM Output Tests
```bash
nsh> pwm_out start
nsh> pwm info           # Should show 4 channels
nsh> pwm test -c 1 -p 1500   # Test Motor 1 at 1500 µs
nsh> pwm test -c 2 -p 1500   # Test Motor 2
nsh> pwm test -c 3 -p 1500   # Test Motor 3
nsh> pwm test -c 4 -p 1500   # Test Motor 4
```

### 6.3 Oscilloscope Verification
| Test | Expected |
|------|----------|
| Frequency | 400 Hz (2.5 ms period) |
| Min pulse | 1000 µs |
| Max pulse | 2000 µs |
| Idle pulse | 900 µs |

### 6.4 Regression Tests (CRITICAL)

| Test | Command | Must Pass |
|------|---------|-----------|
| SD Card Write | `param save` | No corruption |
| SD Card Read | `param load` | Correct values |
| IMU DRDY | `listener sensor_accel` | Data flowing |
| I2C Bus | `i2cdetect -b 0` | No errors |

---

## 7. Rollback Plan

If PWMC implementation fails:

1. Revert to TC-based PWM (backup configuration)
2. Use existing 3-channel TC configuration:
   - PA15 (TIOA1) - Motor 1
   - PC23 (TIOA3) - Motor 2
   - PC26 (TIOA4) - Motor 3
3. Accept 3-motor limitation until fix found

---

## 8. Final Pin Map

```
SAMV71-XULT Pin Usage with PWMC
================================

Port A:
  PA0  - PWMC0 PWMH0 (Motor 1) ← NEW
  PA2  - PWMC0 PWMH1 (Motor 2) ← NEW
  PA3  - I2C0 SDA
  PA4  - I2C0 SCL
  PA11 - SPI0 CS (ICM20689)
  PA12 - SPI0 DRDY (ICM20689) ← Protected!
  PA14 - PWMC1 PWMH1 (Motor 4) ← NEW
  PA15 - (was TC PWM, now free)
  PA23 - LED Blue
  PA25 - HSMCI0 MCCK
  PA26 - HSMCI0 DA2 ← Protected! (SD card)
  PA27-30 - HSMCI0 (SD card)

Port C:
  PC19 - PWMC0 PWMH2 (Motor 3) ← NEW
  PC23 - (was TC PWM, now free)
  PC26 - (was TC PWM, now free)

Port D:
  PD20 - SPI0 MISO
  PD21 - SPI0 MOSI
  PD22 - SPI0 SPCK
```

---

## 9. References

- SAMV71 Datasheet: Chapter 47 (PWM Controller)
- `PWM_INVESTIGATION_SUMMARY.md` - Previous investigation
- `TASK_DSHOT_PWM_IMPLEMENTATION.md` - DShot/PWM task description
- NuttX `sam_pwm.c` - PWMC driver reference
- STM32 `io_timer.c` - Reference implementation

---

## 10. Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-06 | Initial creation with complete implementation plan |

---

**Document Status:** Ready for Implementation

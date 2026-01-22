# PWM Output Verification Test Procedure

**Board:** SAMV71-XULT Xplained Ultra
**Firmware:** PX4 microchip_samv71-xult-clickboards_default
**Date:** January 2026
**Tester:** _______________

---

## 1. Overview

This test verifies the 3-channel TC-based PWM output functionality on the SAMV71-XULT board. The PWM signals are used for ESC/motor control.

**Channels Under Test:**
| Channel | TC Block | Pin | Physical Location |
|---------|----------|-----|-------------------|
| PWM0 | TC0 CH1 | PA15 | See Section 2 |
| PWM1 | TC1 CH0 | PC23 | See Section 2 |
| PWM2 | TC1 CH1 | PC26 | See Section 2 |

---

## 2. Hardware Setup

### 2.1 Required Equipment

- [ ] SAMV71-XULT Xplained Ultra board
- [ ] Oscilloscope (2+ channels preferred)
- [ ] Oscilloscope probes (3x)
- [ ] USB Micro-B cable (for power and serial console)
- [ ] Jumper wires or probe hooks
- [ ] Serial terminal software (picocom, screen, or PuTTY)

### 2.2 Pin Locations on SAMV71-XULT

```
SAMV71-XULT Board Layout (Top View)
====================================

     USB DEBUG        USB TARGET
        [ ]              [ ]

    ┌─────────────────────────────────┐
    │                                 │
    │   ┌─────┐         ┌─────┐      │
    │   │ EXT1│         │ EXT2│      │
    │   │     │         │     │      │
    │   └─────┘         └─────┘      │
    │                                 │
    │                    ETHERNET     │
    │   ┌──────────────┐   [ ]       │
    │   │              │             │
    │   │   SAMV71     │  ┌──────┐   │
    │   │   MCU        │  │SDCARD│   │
    │   │              │  └──────┘   │
    │   └──────────────┘             │
    │                                 │
    │   ┌─────┐  ┌─────┐             │
    │   │ MB1 │  │ MB2 │  mikroBUS   │
    │   └─────┘  └─────┘             │
    │                                 │
    │   ARDUINO HEADERS              │
    │   [....................]       │
    │                                 │
    └─────────────────────────────────┘
```

### 2.3 PWM Pin Locations

#### PA15 (PWM Channel 0) - Arduino Header D14
```
Arduino Header (J503/J504):
Pin 20 = D14 = PA15

    J503                J504
    ┌───┐              ┌───┐
    │ 1 │ IOREF        │ 1 │ D8
    │ 2 │ RESET        │ 2 │ D9
    │...│              │...│
    │   │              │20 │ D14 ← PA15 (PWM0)
    │   │              │21 │ D15
    └───┘              └───┘
```

#### PC23 (PWM Channel 1) - Test Point or Header
```
PC23 is accessible on:
- J506 (Debug header) or
- Direct probe to MCU pin

Check schematic for exact test point location.
Alternative: Use logic analyzer flying lead to MCU.
```

#### PC26 (PWM Channel 2) - Test Point or Header
```
PC26 is accessible on:
- J506 (Debug header) or
- Direct probe to MCU pin

Check schematic for exact test point location.
```

### 2.4 Recommended Probe Points

| Channel | Primary Location | Alternative |
|---------|-----------------|-------------|
| PA15 | Arduino D14 (easiest) | J504 Pin 20 |
| PC23 | Need to identify header | MCU pin direct |
| PC26 | Need to identify header | MCU pin direct |

**Note:** PA15 on Arduino header is the easiest to probe.

---

## 3. Expected PWM Parameters

| Parameter | Expected Value | Tolerance |
|-----------|---------------|-----------|
| Frequency | 400 Hz | ±1% |
| Period | 2.5 ms | ±25 µs |
| Disarmed Pulse | 1000 µs | ±10 µs |
| Min Throttle (0%) | 1000 µs | ±10 µs |
| Mid Throttle (50%) | 1500 µs | ±10 µs |
| Max Throttle (100%) | 2000 µs | ±10 µs |
| 15% Throttle | ~1150 µs | ±20 µs |
| Rise Time | < 100 ns | - |
| Fall Time | < 100 ns | - |
| Voltage High | ~3.3V | ±0.2V |
| Voltage Low | ~0V | < 0.3V |

### PWM Waveform Reference

```
        ┌──────┐                         ┌──────┐
        │      │                         │      │
        │      │                         │      │
────────┘      └─────────────────────────┘      └─────────
        ←─────→
        1-2 ms
        (pulse width = throttle)

        ←────────────────────────────────→
                    2.5 ms (400 Hz period)
```

---

## 4. Test Procedure

### 4.1 Pre-Test Setup

1. **Connect the board:**
   ```
   - Connect USB Micro-B cable to "USB DEBUG" port
   - Board should power on (LEDs may blink)
   ```

2. **Open serial terminal:**
   ```bash
   picocom -b 115200 /dev/ttyACM0
   ```
   Or:
   ```bash
   screen /dev/ttyACM0 115200
   ```

3. **Verify PX4 boot:**
   ```
   - Press Enter in terminal
   - Should see "nsh>" prompt
   ```

4. **Connect oscilloscope probes:**
   ```
   - CH1 probe → PA15 (Arduino D14)
   - CH2 probe → PC23 (if accessible)
   - CH3 probe → PC26 (if accessible)
   - GND clip → Board GND (any GND pin)
   ```

5. **Configure oscilloscope:**
   ```
   - Time base: 500 µs/div or 1 ms/div
   - Voltage: 1 V/div
   - Trigger: CH1, Rising Edge, ~1.5V
   - Coupling: DC
   ```

### 4.2 Test 1: Verify PWM Driver Running

**Command:**
```bash
nsh> pwm_out status
```

**Expected Output:**
```
pwm_out: cycle: X events, XXXus elapsed, XXX.XXus avg, min XXXus max XXXus
Channel Configuration:
Channel 0: value: 1000, disarmed: 1000, min: 1000, max: 2000
Channel 1: value: 1000, disarmed: 1000, min: 1000, max: 2000
Channel 2: value: 1000, disarmed: 1000, min: 1000, max: 2000
Timer 0: rate: 400 channels: X
Timer 2: rate: 400 channels: X
```

**Result:** [ ] PASS  [ ] FAIL

**Notes:** _________________________________

---

### 4.3 Test 2: Disarmed State PWM

**Purpose:** Verify PWM output at disarmed/idle state (1000 µs)

**Procedure:**
1. Ensure system is disarmed (default after boot)
2. Observe oscilloscope

**Measurements:**

| Channel | Pin | Frequency | Pulse Width | Voltage | PASS/FAIL |
|---------|-----|-----------|-------------|---------|-----------|
| PWM0 | PA15 | _____ Hz | _____ µs | _____ V | [ ] |
| PWM1 | PC23 | _____ Hz | _____ µs | _____ V | [ ] |
| PWM2 | PC26 | _____ Hz | _____ µs | _____ V | [ ] |

**Expected:** 400 Hz, ~1000 µs pulse width, 3.3V high level

---

### 4.4 Test 3: Single Motor Test (50% Throttle)

**Purpose:** Verify PWM responds to actuator commands

**Command:**
```bash
nsh> actuator_test set -m 1 -v 0.5
```

**Note:** Motor numbering is 1-based in actuator_test, maps to Channel 0

**Measurements on PA15:**

| Parameter | Expected | Measured | PASS/FAIL |
|-----------|----------|----------|-----------|
| Frequency | 400 Hz | _____ Hz | [ ] |
| Pulse Width | 1500 µs | _____ µs | [ ] |
| Duty Cycle | 60% | _____ % | [ ] |

**Stop Test:**
```bash
nsh> actuator_test set -m 1 -v 0
```
Or press Enter.

---

### 4.5 Test 4: All Motors Iterate Test

**Purpose:** Verify all 3 channels respond

**Command:**
```bash
nsh> actuator_test iterate-motors
```

**Observation:**
- Watch oscilloscope during iteration
- Each motor should pulse at 15% (~1150 µs) for ~1 second

**Results:**

| Motor | Channel | Pin | Signal Observed | PASS/FAIL |
|-------|---------|-----|-----------------|-----------|
| Motor 0 | PWM0 | PA15 | [ ] Yes [ ] No | [ ] |
| Motor 1 | PWM1 | PC23 | [ ] Yes [ ] No | [ ] |
| Motor 2 | PWM2 | PC26 | [ ] Yes [ ] No | [ ] |
| Motor 3-11 | N/A | N/A | Should be no change | [ ] |

---

### 4.6 Test 5: Throttle Range Test

**Purpose:** Verify full throttle range (1000-2000 µs)

**Commands and Expected Results:**

| Command | Expected Pulse (PA15) | Measured | PASS/FAIL |
|---------|----------------------|----------|-----------|
| `actuator_test set -m 1 -v 0` | 1000 µs | _____ µs | [ ] |
| `actuator_test set -m 1 -v 0.25` | 1250 µs | _____ µs | [ ] |
| `actuator_test set -m 1 -v 0.5` | 1500 µs | _____ µs | [ ] |
| `actuator_test set -m 1 -v 0.75` | 1750 µs | _____ µs | [ ] |
| `actuator_test set -m 1 -v 1.0` | 2000 µs | _____ µs | [ ] |

**Stop Test:** `actuator_test set -m 1 -v 0` or press Enter

---

### 4.7 Test 6: Waveform Quality

**Purpose:** Verify clean PWM waveform

**Procedure:**
1. Set motor to 50%: `actuator_test set -m 1 -v 0.5`
2. Zoom in on rising/falling edges
3. Check for overshoot, ringing, or noise

**Measurements:**

| Parameter | Specification | Measured | PASS/FAIL |
|-----------|--------------|----------|-----------|
| Rise Time (10-90%) | < 100 ns | _____ ns | [ ] |
| Fall Time (90-10%) | < 100 ns | _____ ns | [ ] |
| Overshoot | < 10% | _____ % | [ ] |
| Ringing | < 3 cycles | _____ | [ ] |
| Noise (high level) | < 100 mV | _____ mV | [ ] |

---

## 5. Test Summary

### Overall Results

| Test | Description | Result |
|------|-------------|--------|
| Test 1 | PWM Driver Status | [ ] PASS [ ] FAIL |
| Test 2 | Disarmed State PWM | [ ] PASS [ ] FAIL |
| Test 3 | Single Motor 50% | [ ] PASS [ ] FAIL |
| Test 4 | All Motors Iterate | [ ] PASS [ ] FAIL |
| Test 5 | Throttle Range | [ ] PASS [ ] FAIL |
| Test 6 | Waveform Quality | [ ] PASS [ ] FAIL |

### Final Verdict

- [ ] **ALL TESTS PASSED** - PWM baseline verified
- [ ] **SOME TESTS FAILED** - See notes below

### Issues Found

_________________________________________________________________

_________________________________________________________________

_________________________________________________________________

### Oscilloscope Screenshots

Attach or reference oscilloscope captures:
- [ ] Disarmed state waveform
- [ ] 50% throttle waveform
- [ ] Rising edge detail
- [ ] All 3 channels (if captured simultaneously)

---

## 6. Troubleshooting

### No PWM Signal

1. Check `pwm_out status` - driver running?
2. Verify correct pin (PA15 = Arduino D14)
3. Check probe GND connection
4. Try `actuator_test set -m 1 -v 0.5`

### Wrong Frequency

1. Should be 400 Hz (2.5 ms period)
2. Check Timer configuration in `pwm_out status`

### Wrong Pulse Width

1. Verify actuator_test value (-v parameter)
2. 0.0 = 1000 µs, 0.5 = 1500 µs, 1.0 = 2000 µs

### Signal Present But Noisy

1. Check GND connection
2. Use shorter probe ground lead
3. Check for nearby switching noise sources

---

## 7. Sign-Off

**Tested By:** _________________________ **Date:** ___________

**Reviewed By:** _________________________ **Date:** ___________

**Firmware Version:** `git describe --tags` output: _____________

**Build Hash:** ____________________

---

## Appendix A: Quick Reference Commands

```bash
# Check PWM status
pwm_out status

# Test single motor (1-based numbering)
actuator_test set -m 1 -v 0.5    # Motor 1 at 50%
actuator_test set -m 2 -v 0.5    # Motor 2 at 50%
actuator_test set -m 3 -v 0.5    # Motor 3 at 50%

# Stop motor test
actuator_test set -m 1 -v 0

# Iterate all motors
actuator_test iterate-motors

# System info
ver all
uorb top
```

## Appendix B: Pin Reference

| PWM Channel | TC | Pin | Arduino | EXT Header | Notes |
|-------------|-----|-----|---------|------------|-------|
| 0 | TC0 CH1 | PA15 | D14 | - | Easiest to probe |
| 1 | TC1 CH0 | PC23 | - | - | Check schematic |
| 2 | TC1 CH1 | PC26 | - | - | Check schematic |

---

## Appendix C: Adding 4th PWM Channel (TC-Based)

This section documents how to enable a 4th PWM channel using TC2 CH0 (PC5).

### C.1 Proposed 4-Channel Configuration

| Motor | PWM | Pin | TC Channel | Timer Index | Status |
|-------|-----|-----|------------|-------------|--------|
| Motor 1 | PWM0 | PA15 | TC0 CH1 | Timer1 | Current |
| Motor 2 | PWM1 | PC23 | TC1 CH0 | Timer3 | Current |
| Motor 3 | PWM2 | PC26 | TC1 CH1 | Timer4 | Current |
| **Motor 4** | **PWM3** | **PC5** | **TC2 CH0** | **Timer6** | **NEW** |

### C.2 PC5 Pin Information

```
PC5 (TIOA6) - TC2 Channel 0, Output A
- NuttX Macro: GPIO_TC6_TIOA
- Peripheral Function: PERIPH B
- Physical Location: LCD connector D5 (LCD not used)
- Conflict Check: SAFE - No conflicts with current implementation
```

### C.3 Files to Modify

#### File 1: `nuttx-config/nsh/defconfig`

Add TC2 peripheral enable:

```diff
 CONFIG_SAMV7_TC0=y
 CONFIG_SAMV7_TC1=y
+CONFIG_SAMV7_TC2=y
 CONFIG_SAMV7_TC3=y
```

#### File 2: `src/board_config.h`

**Step 1:** Update channel count:

```diff
-#define DIRECT_PWM_OUTPUT_CHANNELS  3
+#define DIRECT_PWM_OUTPUT_CHANNELS  4
```

**Step 2:** Add PWM4 GPIO definition (after existing PWM definitions ~line 149):

```c
#define GPIO_PWM4_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN5)   /* TC6 TIOA - PC5 */
```

Full PWM section should look like:

```c
/* PWM Timer Configuration
 *
 * TC0 CH1 (TC1) - PWM1: PA15 (TIOA1)
 * TC1 CH0 (TC3) - PWM2: PC23 (TIOA3)
 * TC1 CH1 (TC4) - PWM3: PC26 (TIOA4)
 * TC2 CH0 (TC6) - PWM4: PC5 (TIOA6) - NEW
 */
#define GPIO_PWM1_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN15)  /* TC1 TIOA - PA15 */
#define GPIO_PWM2_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN23)  /* TC3 TIOA - PC23 */
#define GPIO_PWM3_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN26)  /* TC4 TIOA - PC26 */
#define GPIO_PWM4_OUT    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN5)   /* TC6 TIOA - PC5 */
```

#### File 3: `src/timer_config.cpp`

**Step 1:** Add Timer6 to io_timers array:

```cpp
const io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::Timer1),  /* TC0 CH1 - PA15 */
    initIOTimer(Timer::Timer3),  /* TC1 CH0 - PC23 */
    initIOTimer(Timer::Timer4),  /* TC1 CH1 - PC26 */
    initIOTimer(Timer::Timer6),  /* TC2 CH0 - PC5 (NEW) */
};
```

**Step 2:** Add channel 3 to timer_io_channels array:

```cpp
const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOTimerChannel(io_timers, {Timer::Timer1, Timer::Channel1}, {GPIO::PortA, GPIO::Pin15}),
    initIOTimerChannel(io_timers, {Timer::Timer3, Timer::Channel1}, {GPIO::PortC, GPIO::Pin23}),
    initIOTimerChannel(io_timers, {Timer::Timer4, Timer::Channel1}, {GPIO::PortC, GPIO::Pin26}),
    initIOTimerChannel(io_timers, {Timer::Timer6, Timer::Channel1}, {GPIO::PortC, GPIO::Pin5}),  /* NEW */
};
```

#### File 4: `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c`

Add TC6 (TC2 CH0) to the PID mapping function (~line 109):

```c
static inline uint32_t get_tc_pid(unsigned timer)
{
    switch (timer) {
    case 0: return SAM_PID_TC1;  /* Timer1 -> TC0 CH1 */
    case 1: return SAM_PID_TC3;  /* Timer3 -> TC1 CH0 */
    case 2: return SAM_PID_TC4;  /* Timer4 -> TC1 CH1 */
    case 3: return SAM_PID_TC6;  /* Timer6 -> TC2 CH0 (NEW) */
    default: return 0;
    }
}
```

**Note:** Verify `SAM_PID_TC6` is defined in NuttX. TC2 CH0 should be PID 29.

#### File 5: `include/px4_arch/io_timer_hw_description.h` (if needed)

Verify Timer6 is defined in the Timer enum. If not, add:

```cpp
namespace Timer {
    enum Timer : uint8_t {
        Timer1 = 0,
        Timer2 = 1,
        Timer3 = 2,
        Timer4 = 3,
        Timer5 = 4,
        Timer6 = 5,  /* TC2 CH0 - ensure this exists */
        // ...
    };
}
```

### C.4 Build and Test

**Build:**
```bash
make microchip_samv71-xult-clickboards_default
```

**Flash:**
```bash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.bin 0x00400000 verify reset exit"
```

**Verify 4 channels:**
```bash
nsh> pwm_out status
# Should show Channel 0, 1, 2, 3
```

**Test 4th channel:**
```bash
nsh> actuator_test set -m 4 -v 0.5
# Probe PC5 - should see 400Hz, 1500µs pulse
```

### C.5 4-Channel Pin Summary

| Channel | Motor | Pin | TC | Location | Probe Point |
|---------|-------|-----|----|----------|-------------|
| 0 | M1 | PA15 | TC0 CH1 | Arduino D14 | Easy |
| 1 | M2 | PC23 | TC1 CH0 | - | Check schematic |
| 2 | M3 | PC26 | TC1 CH1 | - | Check schematic |
| 3 | M4 | PC5 | TC2 CH0 | LCD D5 | Check schematic |

### C.6 Important Notes

1. **No DShot Support:** TC-based PWM does NOT support DShot protocol.
   For DShot, use PWMC implementation instead (see `PWMC_IMPLEMENTATION_PLAN.md`).

2. **PC5 Conflict:** PC5 is the LCD D5 data line. LCD is not enabled in current
   configuration, so this is safe. Do not enable LCD if using PC5 for PWM.

3. **TC2 Clock:** TC2 must be enabled in defconfig for PC5 PWM to work.

4. **PID Numbers:** SAMV7 TC peripheral IDs:
   - TC0 (TC0 CH0) = PID 23
   - TC1 (TC0 CH1) = PID 24
   - TC2 (TC0 CH2) = PID 25
   - TC3 (TC1 CH0) = PID 26
   - TC4 (TC1 CH1) = PID 27
   - TC5 (TC1 CH2) = PID 28
   - TC6 (TC2 CH0) = PID 29
   - TC7 (TC2 CH1) = PID 30
   - TC8 (TC2 CH2) = PID 31

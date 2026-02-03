# SAMV71-XULT PWMC Motor PWM Implementation

## Overview

This document describes the PWMC (PWM Controller) implementation for motor PWM output on the SAMV71-XULT-Clickboards board. The PWMC replaces the previous Timer/Counter (TC) based PWM approach, providing dedicated hardware PWM with glitch-free duty cycle updates.

## Hardware Configuration

### SAMV7 PWMC Architecture

The SAMV71 has 2 PWMC modules:
- **PWM0**: Base address 0x40020000, Peripheral ID 31
- **PWM1**: Base address 0x4005C000, Peripheral ID 60

Each module has 4 independent channels (CH0-CH3) with:
- CPRD: Period register (determines PWM frequency)
- CDTY: Duty cycle register
- CDTYUPD: Duty cycle update register (glitch-free updates)
- CPRDUPD: Period update register (glitch-free updates)

### Clock Configuration

```
MCK (Master Clock) = 150 MHz
CPRE = 3 (MCK/8 prescaler)
PWM Clock = 150 MHz / 8 = 18.75 MHz
For 400 Hz PWM: CPRD = 18,750,000 / 400 = 46,875 ticks
```

### Motor Pin Mapping

| Motor | PWM Channel | GPIO Pin | Peripheral | Header Location |
|-------|-------------|----------|------------|-----------------|
| Motor 1 | PWM0 CH3 | **PC13** | Peripheral B | **EXT2 Pin 4** |
| Motor 2 | PWM0 CH1 | PA2 | Peripheral A | **EXT2 Pin 9** |
| Motor 3 | PWM0 CH2 | PC19 | Peripheral B | **EXT2 Pin 7** (also mikroBUS1 PWM) |
| Motor 4 | PWM0 CH0 | PB0 | Peripheral A | **EXT1 Pin 13** |

### Pin Verification (from SAMV71-XULT board.h)

```
PC13 - EXT2 connector Pin 4 (N/C, available for PWM)
PA2  - EXT2 connector Pin 9
PC19 - EXT2 connector Pin 7 (also mikroBUS1 PWM pin)
PB0  - EXT1 connector Pin 13 (was MB2_RST, repurposed for Motor 4)
```

**CRITICAL FIX (2025):** Motor 1 was moved from PA7 to PC13 because PA7 is XIN32 (32.768 kHz slow crystal input). Using PA7 for PWM would conflict with the RTC when `BOARD_HAVE_SLOWXTAL=1`.

**WARNING:** Do NOT probe EXT1-9 (PD28 = IMU DRDY) or EXT2-3 (PD30 = ADC battery voltage)!

## Files Modified/Created

### 1. Hardware Description (`platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`)

Added PWM namespace with module and channel enums:

```cpp
namespace PWM
{
enum PWMModule {
    PWM0 = 0,
    PWM1 = 1,
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

enum class PWMCPeripheral {
    A = 0,  /* GPIO_PERIPHA */
    B = 1,  /* GPIO_PERIPHB */
};

#ifndef SAM_PWM0_BASE
#define SAM_PWM0_BASE  0x40020000
#endif
#ifndef SAM_PWM1_BASE
#define SAM_PWM1_BASE  0x4005C000
#endif
```

### 2. IO Timer Hardware Description (`platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`)

Added PWMC helper functions:

```cpp
static inline constexpr io_timers_t initIOPWMTimer(PWM::PWMModule module)
{
    io_timers_t ret{};
    ret.base = pwmBaseRegister(module);
    ret.clock_register = 0;
    ret.clock_bit = 0;
    ret.vectorno = 0;
    return ret;
}

static inline constexpr timer_io_channels_t initIOPWMChannel(const io_timers_t io_timers_conf[MAX_IO_TIMERS],
        PWM::PWMChannel pwm_channel, GPIO::GPIOPin pin, PWMCPeripheral periph)
{
    timer_io_channels_t ret{};
    uint32_t gpio_mode = (periph == PWMCPeripheral::A) ? (3 << 21) : (4 << 21);
    uint32_t gpio_cfg = (0 << 16);
    uint32_t gpio_port = ((uint32_t)pin.port << 5);
    uint32_t gpio_pin = (uint32_t)pin.pin;
    ret.gpio_out = gpio_mode | gpio_cfg | gpio_port | gpio_pin;
    ret.gpio_in = 0;
    ret.timer_index = (uint8_t)pwm_channel.module;
    ret.timer_channel = (uint8_t)pwm_channel.channel;
    return ret;
}
```

### 3. PWMC Driver (`platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`)

New driver implementing the io_timer interface for PWMC hardware. Key functions:

- `io_timer_init_timer()` - Initialize PWMC module, enable clock
- `io_timer_channel_init()` - Configure channel for PWM output
- `io_timer_set_ccr()` - Set duty cycle (microseconds to ticks conversion)
- `io_timer_set_rate()` - Set PWM frequency
- `io_timer_set_enable()` - Enable/disable channels
- `io_timer_get_group()` - Get channel bitmask for a timer

### 4. Timer Configuration (`boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`)

Updated to use PWMC:

```cpp
const io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOPWMTimer(PWM::PWM0),
};

const timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel3}, {GPIO::PortC, GPIO::Pin13}, PWMCPeripheral::B),  /* Motor 1 - PC13 (was PA7/XIN32) */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel1}, {GPIO::PortA, GPIO::Pin2},  PWMCPeripheral::A),  /* Motor 2 */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel2}, {GPIO::PortC, GPIO::Pin19}, PWMCPeripheral::B),  /* Motor 3 */
    initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel0}, {GPIO::PortB, GPIO::Pin0},  PWMCPeripheral::A),  /* Motor 4 */
};
```

### 5. Board Configuration (`boards/microchip/samv71-xult-clickboards/src/board_config.h`)

Updated defines:

```cpp
#define DIRECT_PWM_OUTPUT_CHANNELS  4
#define BOARD_NUM_IO_TIMERS         1
```

Removed conflicting GPIO defines (GPIO_PWM1_OUT, GPIO_PWM2_OUT, GPIO_PWM3_OUT, GPIO_MB2_RST).

### 6. CMakeLists.txt (`platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`)

Changed from TC driver to PWMC driver:

```cmake
px4_add_library(arch_io_pins
    io_timer_pwmc.c   # Was: io_timer_tc.c
    pwm_servo.c
)
```

### 7. Build Script Update (`Tools/module_config/output_groups_from_timer_config.py`)

Added SAMV7 PWMC format parsing:

```python
# SAMV7 PWMC format: initIOPWMTimer(PWM::PWM0),
search = re.search('initIOPWMTimer\\(PWM::(PWM[0-9]+)\\)', line, re.IGNORECASE)
if search:
    return search.group(1), 'samv7'

# SAMV7 PWMC channel format: initIOPWMChannel(io_timers, {PWM::PWM0, PWM::Channel3}, ...)
search = re.search('initIOPWMChannel.*PWM::(PWM[0-9]+)', line, re.IGNORECASE)
```

### 8. PWMOut Driver Fix (`src/drivers/pwm_out/PWMOut.cpp`)

Fixed SAMV7 subscription handling:

```cpp
// SAMV7: Disable work queue switch to avoid re-entrancy crash.
// Pass false to skip work queue switch while still setting up
// function assignments and scheduling.
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(false);
#else
    _mixing_output.updateSubscriptions(true);
#endif
```

### 9. NuttX defconfig (`boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`)

Removed NuttX PWMC driver config (we use direct register access):

```
# Removed: CONFIG_SAMV7_PWM0=y and related CONFIG_SAMV7_PWM0_CHx entries
```

---

## Parameter Configuration

### Motor Function Assignment

| Parameter | Value | Description |
|-----------|-------|-------------|
| PWM_MAIN_FUNC1 | 101 | Motor 1 |
| PWM_MAIN_FUNC2 | 102 | Motor 2 |
| PWM_MAIN_FUNC3 | 103 | Motor 3 |
| PWM_MAIN_FUNC4 | 104 | Motor 4 |

### PWM Limits

| Parameter | Default | Description |
|-----------|---------|-------------|
| PWM_MAIN_DIS1-4 | 1000 | Disarmed value (microseconds) |
| PWM_MAIN_MIN1-4 | 1000 | Minimum value (microseconds) |
| PWM_MAIN_MAX1-4 | 2000 | Maximum value (microseconds) |
| PWM_MAIN_FAIL1-4 | 1000 | Failsafe value (microseconds) |

### Timer Rate

| Parameter | Value | Description |
|-----------|-------|-------------|
| PWM_MAIN_TIM0 | 400 | PWM frequency in Hz |

---

## Build Instructions

```bash
# Clean build (recommended for first time)
make clean
make microchip_samv71-xult-clickboards_default

# Incremental build
make microchip_samv71-xult-clickboards_default
```

Expected output:
```
Memory region         Used Size  Region Size  %age Used
       flash:     1345988 B         2 MB     64.18%
        sram:       52604 B       320 KB     16.05%
     nocache:          5 KB        64 KB      7.81%
```

## Flashing Instructions

```bash
# Using OpenOCD with CMSIS-DAP (EDBG on SAMV71-XULT)
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
    -c "adapter speed 4000" \
    -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.bin 0x00400000 verify reset exit"
```

---

## Test Guide

### Prerequisites

1. SAMV71-XULT board with firmware flashed
2. USB cable connected (for NSH console via CDC/ACM)
3. Serial terminal (minicom, screen, or similar) at 115200 baud
4. Oscilloscope for hardware verification

### Console Connection

```bash
# Find the device
ls /dev/ttyACM*

# Connect (example with minicom)
minicom -D /dev/ttyACM0 -b 115200

# Or with screen
screen /dev/ttyACM0 115200
```

### Test Procedure

#### Step 1: Verify System Boot

After reset, you should see boot messages. Press Enter to get NSH prompt:

```
nsh>
```

#### Step 2: Check PWM Driver Status

```
nsh> pwm_out status
```

**Expected Output:**
```
pwm_out: cycle: 1477 events, 41805us elapsed, 28.30us avg, min 13us max 63us 5.258us rms
pwm_out: interval: 1477 events, 49670.45us avg, min 24us max 50133us 3890.754us rms
INFO  [mixer_module] Param prefix: PWM_MAIN
control latency: 0 events, 0us elapsed, 0.00us avg, min 0us max 0us 0.000us rms
Channel Configuration:
Channel 0: func: 101, value: 1000, failsafe: 1000, disarmed: 1000, min: 1000, max: 2000
Channel 1: func: 102, value: 1000, failsafe: 1000, disarmed: 1000, min: 1000, max: 2000
Channel 2: func: 103, value: 1000, failsafe: 1000, disarmed: 1000, min: 1000, max: 2000
Channel 3: func: 104, value: 1000, failsafe: 1000, disarmed: 1000, min: 1000, max: 2000
Timer 0: rate: 400 channels: 0 1 2 3
```

**Verification Points:**
- `cycle: XXXX events` - Should show hundreds/thousands of events (NOT just 1)
- `func: 101, 102, 103, 104` - Motor 1-4 assigned (NOT 0)
- `Timer 0: rate: 400 channels: 0 1 2 3` - All 4 channels at 400Hz

#### Step 3: Verify Actuator Outputs Topic

```
nsh> listener actuator_outputs
```

**Expected Output:**
```
TOPIC: actuator_outputs
 actuator_outputs
    timestamp: 157288141 (0.033794 seconds ago)
    noutputs: 4
    output: [1000.00000, 1000.00000, 1000.00000, 1000.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000]
```

**Verification Points:**
- `timestamp: X.XXX seconds ago` - Should be < 1 second (fresh, updating)
- `noutputs: 4` - All 4 channels active
- `output: [1000, 1000, 1000, 1000, ...]` - Disarmed PWM values

#### Step 4: Check Control Allocator Status

```
nsh> control_allocator status
```

**Expected Output:**
```
control_allocator: running
Effectiveness..: Roll 1.0  Pitch 1.0  Yaw 1.0  Thrust 1.0
Configured Motors: 4
Configured Servos: 0
```

#### Step 5: Check Actuator Motors Topic

```
nsh> listener actuator_motors
```

**Expected Output:**
```
TOPIC: actuator_motors
 actuator_motors
    timestamp: XXXXXXXX (X.XXX seconds ago)
    control: [0.00000, 0.00000, 0.00000, 0.00000, nan, nan, nan, nan, nan, nan, nan, nan]
```

**Verification Points:**
- First 4 values should be 0.0 (idle/disarmed)
- Values 5-12 should be `nan` (unused)

#### Step 6: Verify Parameter Values

```
nsh> param show PWM_MAIN*
```

**Expected Output (key parameters):**
```
PWM_MAIN_FUNC1: 101
PWM_MAIN_FUNC2: 102
PWM_MAIN_FUNC3: 103
PWM_MAIN_FUNC4: 104
PWM_MAIN_TIM0: 400
PWM_MAIN_DIS1: 1000
PWM_MAIN_DIS2: 1000
PWM_MAIN_DIS3: 1000
PWM_MAIN_DIS4: 1000
PWM_MAIN_MIN1: 1000
PWM_MAIN_MIN2: 1000
PWM_MAIN_MIN3: 1000
PWM_MAIN_MIN4: 1000
PWM_MAIN_MAX1: 2000
PWM_MAIN_MAX2: 2000
PWM_MAIN_MAX3: 2000
PWM_MAIN_MAX4: 2000
```

#### Step 7: Set Parameters (if not already set)

```
nsh> param set PWM_MAIN_FUNC1 101
nsh> param set PWM_MAIN_FUNC2 102
nsh> param set PWM_MAIN_FUNC3 103
nsh> param set PWM_MAIN_FUNC4 104
nsh> param save
nsh> reboot
```

---

## Hardware Verification with Oscilloscope

### Test Points

| Motor | Pin | Header | Expected Signal |
|-------|-----|--------|-----------------|
| Motor 1 | PA7 | **Arduino A1** | 400Hz, 1000us pulse |
| Motor 2 | PA2 | **EXT2 Pin 9** | 400Hz, 1000us pulse |
| Motor 3 | PC19 | **EXT2 Pin 7** | 400Hz, 1000us pulse |
| Motor 4 | PB0 | **EXT1 Pin 13** | 400Hz, 1000us pulse |

**WARNING:** Do NOT probe:
- EXT1 Pin 9 (PD28) - This is IMU DRDY, not PA2!
- EXT2 Pin 3 (PD30) - This is ADC battery voltage, not PB0!

### Expected Waveform Characteristics

**Disarmed State (default):**
- Frequency: 400 Hz (2.5 ms period)
- Pulse width: 1000 us (1 ms)
- Duty cycle: 40%
- Amplitude: 0V to 3.3V

### Actuator Test Commands

Use these commands to test PWM output range:

```bash
# Test Motor 1 at minimum (1000us)
nsh> actuator_test set -m 1 -v 0.0

# Test Motor 1 at mid-range (1500us)
nsh> actuator_test set -m 1 -v 0.5

# Test Motor 1 at maximum (2000us)
nsh> actuator_test set -m 1 -v 1.0

# Test Motor 2
nsh> actuator_test set -m 2 -v 0.5

# Test Motor 3
nsh> actuator_test set -m 3 -v 0.5

# Test Motor 4
nsh> actuator_test set -m 4 -v 0.5

# Reset all to disarmed
nsh> actuator_test reset
```

### Expected Pulse Widths

| Command Value | Expected Pulse Width |
|---------------|---------------------|
| -v 0.0 | 1000 us (MIN) |
| -v 0.25 | 1250 us |
| -v 0.5 | 1500 us (MID) |
| -v 0.75 | 1750 us |
| -v 1.0 | 2000 us (MAX) |

---

## Troubleshooting

### Issue: `func: 0` shown in pwm_out status

**Cause:** Parameters not set or not saved.

**Solution:**
```
nsh> param set PWM_MAIN_FUNC1 101
nsh> param set PWM_MAIN_FUNC2 102
nsh> param set PWM_MAIN_FUNC3 103
nsh> param set PWM_MAIN_FUNC4 104
nsh> param save
nsh> reboot
```

### Issue: `cycle: 1 events` (driver not cycling)

**Cause:** Old firmware without the updateSubscriptions fix.

**Solution:** Rebuild with latest code that includes the fix in PWMOut.cpp:
```cpp
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(false);
#else
    _mixing_output.updateSubscriptions(true);
#endif
```

### Issue: `actuator_outputs` timestamp is stale (hundreds of seconds ago)

**Cause:** Same as above - driver not cycling.

**Solution:** Rebuild and reflash firmware.

### Issue: No PWM signal on oscilloscope

**Possible Causes:**
1. Wrong pin probed - verify header pin mapping
2. GPIO not configured - check timer_config.cpp
3. PWMC clock not enabled - check io_timer_pwmc.c

**Debug Commands:**
```
nsh> pwm_out status
nsh> listener actuator_outputs
```

### Issue: Build error about unparsed timer

**Cause:** output_groups_from_timer_config.py doesn't recognize PWMC format.

**Solution:** Update the Python script to parse SAMV7 PWMC format (already done).

### Issue: GPIO conflict with NuttX PWM driver

**Cause:** CONFIG_SAMV7_PWM0=y in defconfig enables NuttX PWM driver which configures GPIOs.

**Solution:** Remove CONFIG_SAMV7_PWM0 and related entries from defconfig. PX4 uses direct register access.

---

## Test Report Template

```
SAMV71-XULT PWMC Test Report
============================
Date: _______________
Tester: _______________
Firmware Version: _______________

Software Tests:
[ ] pwm_out status shows cycle > 1000 events
[ ] pwm_out status shows func: 101, 102, 103, 104
[ ] pwm_out status shows Timer 0: rate: 400
[ ] actuator_outputs timestamp < 1 second ago
[ ] actuator_outputs noutputs: 4
[ ] actuator_outputs output: [1000, 1000, 1000, 1000, ...]
[ ] control_allocator shows 4 motors configured

Hardware Tests (Oscilloscope):
[ ] Motor 1 (PA7): _____ Hz, _____ us pulse width
[ ] Motor 2 (PA2): _____ Hz, _____ us pulse width
[ ] Motor 3 (PC19): _____ Hz, _____ us pulse width
[ ] Motor 4 (PB0): _____ Hz, _____ us pulse width

Actuator Test Results:
[ ] actuator_test set -m 1 -v 0.0 -> _____ us measured
[ ] actuator_test set -m 1 -v 0.5 -> _____ us measured
[ ] actuator_test set -m 1 -v 1.0 -> _____ us measured
[ ] actuator_test set -m 2 -v 0.5 -> _____ us measured
[ ] actuator_test set -m 3 -v 0.5 -> _____ us measured
[ ] actuator_test set -m 4 -v 0.5 -> _____ us measured

Notes:
_________________________________
_________________________________
_________________________________

Result: [ ] PASS  [ ] FAIL
```

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-01-XX | 1.0 | Initial PWMC implementation |
| 2025-01-XX | 1.1 | Fixed updateSubscriptions issue for SAMV7 |

---

## References

- SAMV71 Datasheet: Section 49 (PWM Controller)
- SAMV71-XULT Schematic: Pin mapping verification
- PX4 io_timer interface: platforms/nuttx/src/px4/common/include/px4_platform/io_timer.h

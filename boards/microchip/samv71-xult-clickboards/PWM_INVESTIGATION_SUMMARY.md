# SAMV71 PWM Implementation Summary & Investigation

## Date: November 29, 2024

---

## 1. Build Changes Made

### 1.1 Files Modified

#### `boards/microchip/samv71-xult-clickboards/default.px4board`
```diff
- CONFIG_DRIVERS_PWM_OUT=y
- CONFIG_SAMV7_USE_PWM_BACKEND_PWMC=y
+ # CONFIG_DRIVERS_PWM_OUT is not set
+ # CONFIG_SAMV7_USE_PWM_BACKEND_PWMC is not set
```
**Status:** PWM_OUT disabled due to crash

#### `boards/microchip/samv71-xult-clickboards/src/board_config.h`
```diff
- #define DIRECT_PWM_OUTPUT_CHANNELS  6
+ #define DIRECT_PWM_OUTPUT_CHANNELS  4
```
**Reason:** Must match MAX_TIMER_IO_CHANNELS (4) to prevent buffer overflow

#### `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`
- Added debug code with hardware access disabled
- Simplified `validate_channel()` to remove timer_io_channels access
- Simplified `io_timer_get_group()` to return hardcoded 0xF for timer 0
- Multiple iterations of debugging code (currently has debug stubs)

#### `platforms/nuttx/src/px4/microchip/samv7/io_pins/pwm_servo.c`
- Converted all functions to no-ops for debugging
- Removed all io_timer function calls

#### `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`
```cmake
if(CONFIG_DRIVERS_PWM_OUT)
  px4_add_library(arch_io_pins
    io_timer_pwmc.c
    pwm_servo.c
  )
else()
  px4_add_library(arch_io_pins
    io_timer_tc_stub.c
  )
endif()
```

---

## 2. PWM Crash Investigation Summary

### 2.1 Symptoms
- System resets in a loop during boot
- Crash occurs at "nsh: dshot" / "pwm_out start" in startup sequence
- Only happens when CONFIG_DRIVERS_PWM_OUT=y

### 2.2 Investigation Steps

| Step | Test | Result |
|------|------|--------|
| 1 | Disable PWM_OUT entirely | **BOOTS OK** |
| 2 | Enable PWM_OUT with full io_timer code | Crash |
| 3 | Enable PWM_OUT, disable HW register access | Crash |
| 4 | Enable PWM_OUT, disable timer_io_channels access | Crash |
| 5 | Enable PWM_OUT, disable io_timers array access | Crash |
| 6 | Enable PWM_OUT, make pwm_servo.c complete no-op | **CRASH** |

### 2.3 Root Cause Analysis

The crash is **NOT** in our code:
- `io_timer_pwmc.c` - Ruled out (no-op still crashes)
- `pwm_servo.c` - Ruled out (no-op still crashes)
- `timer_config.cpp` - Ruled out (arrays not accessed)

The crash is in **PWMOut module itself** (`src/drivers/pwm_out/PWMOut.cpp`):
- Possibly in `OutputModuleInterface` base class constructor
- Possibly in workqueue initialization (`px4::wq_configurations::hp_default`)
- Possibly in `MixingOutput` initialization
- Possibly in static constructor/initialization

### 2.4 Bugs Found & Fixed

1. **DIRECT_PWM_OUTPUT_CHANNELS mismatch**
   - Was: 6, Should be: 4 (matching MAX_TIMER_IO_CHANNELS)
   - Caused buffer overflow in `g_channel_modes[]` array

2. **io_timer_get_group() wrong return value**
   - Was returning timer index (0) instead of channel bitmask (0xF)
   - Fixed to return proper bitmask

---

## 3. TC-Based IO Approach Recommendation

### 3.1 Why TC Instead of PWMC?

| Aspect | PWMC | TC (Timer/Counter) |
|--------|------|---------------------|
| Channels | 4 (PWMC0) | 9 (TC0-TC2, 3 channels each) |
| Flexibility | PWM only | PWM, Capture, Compare |
| PX4 Compatibility | Custom backend needed | Standard io_timer pattern |
| NuttX Support | Limited | Full support via sam_tc.c |

### 3.2 SAMV71 Timer/Counter Resources (Complete Pin Map)

| TC Block/Ch | TIOA Pin | TIOB Pin | TCLK Pin | Peripheral |
|-------------|----------|----------|----------|------------|
| **TC0 ch0** | **PA0** (TIOA0) | PA1 (TIOB0) | PA4 (TCLK0) | Periph C |
| **TC0 ch1** | **PA15** (TIOA1) | PA16 (TIOB1) | PA28 (TCLK1) | Periph B |
| **TC0 ch2** | **PA26** (TIOA2) | PA27 (TIOB2) | PA29 (TCLK2) | Periph B |
| **TC1 ch0** | **PC23** (TIOA3) | PC24 (TIOB3) | PC25 (TCLK3) | - |
| **TC1 ch1** | **PC26** (TIOA4) | PC27 (TIOB4) | PC28 (TCLK4) | - |
| **TC1 ch2** | **PC29** (TIOA5) | PC30 (TIOB5) | PC31 (TCLK5) | - |
| **TC2 ch0** | **PC5** (TIOA6) | PC6 (TIOB6) | PC7 (TCLK6) | - |
| **TC2 ch1** | **PC8** (TIOA7) | PC9 (TIOB7) | PC10 (TCLK7) | - |
| **TC2 ch2** | **PC11** (TIOA8) | PC12 (TIOB8) | PC14 (TCLK8) | - |
| **TC3 ch0** | **PE0** (TIOA9) | PE1 (TIOB9) | PE2 (TCLK9) | - |
| **TC3 ch1** | **PE3** (TIOA10) | PE4 (TIOB10) | PE5 (TCLK10) | - |
| **TC3 ch2** | ~~PD21~~ (TIOA11) | ~~PD22~~ (TIOB11) | PD24 (TCLK11) | **CONFLICT** |

### 3.3 Pin Conflicts to Avoid

**CRITICAL CONFLICTS:**
- **PD21 (TIOA11)** - Used by SPI0 MOSI - **DO NOT USE**
- **PD22 (TIOB11)** - Used by SPI0 SPCK - **DO NOT USE**
- **PA4 (TCLK0)** - Used by I2C0/TWIHS0 TWCK0 - **DO NOT USE**

**Current Sensor Pin Usage:**
- SPI0: PD20 (MISO), PD21 (MOSI), PD22 (SPCK), PA11 (CS), PA12 (DRDY)
- I2C0/TWIHS0: PA3 (TWD0), PA4 (TWCK0)

### 3.4 Recommended TC Pin Mapping for PWM Outputs

Based on available pins without conflicts:

| PWM Output | TC Channel | TIOA Pin | Notes |
|------------|------------|----------|-------|
| PWM1 | TC0_CH0 | **PA0** | Safe - no conflicts |
| PWM2 | TC0_CH1 | **PA15** | Safe - no conflicts |
| PWM3 | TC0_CH2 | **PA26** | Safe - no conflicts |
| PWM4 | TC1_CH0 | **PC23** | Safe - no conflicts |
| PWM5 | TC1_CH1 | **PC26** | Safe - no conflicts |
| PWM6 | TC1_CH2 | **PC29** | Safe - no conflicts |

**Alternative/Additional Channels (if needed):**
| PWM Output | TC Channel | TIOA Pin | Notes |
|------------|------------|----------|-------|
| PWM7 | TC2_CH0 | **PC5** | Safe |
| PWM8 | TC2_CH1 | **PC8** | Safe |
| PWM9 | TC2_CH2 | **PC11** | Safe |
| PWM10 | TC3_CH0 | **PE0** | Safe (Port E) |
| PWM11 | TC3_CH1 | **PE3** | Safe (Port E) |

**DO NOT USE:** TC3_CH2 (PD21/PD22 conflict with SPI0)

### 3.5 NuttX defconfig Changes Needed for TC

```
# Enable Timer/Counter blocks
CONFIG_SAMV7_TC0=y
CONFIG_SAMV7_TC0_CH0=y
CONFIG_SAMV7_TC0_CH1=y
CONFIG_SAMV7_TC0_CH2=y

CONFIG_SAMV7_TC1=y
CONFIG_SAMV7_TC1_CH0=y
CONFIG_SAMV7_TC1_CH1=y
CONFIG_SAMV7_TC1_CH2=y

# Optional - for more PWM channels
CONFIG_SAMV7_TC2=y
CONFIG_SAMV7_TC2_CH0=y
CONFIG_SAMV7_TC2_CH1=y
CONFIG_SAMV7_TC2_CH2=y

CONFIG_SAMV7_TC3=y
CONFIG_SAMV7_TC3_CH0=y
CONFIG_SAMV7_TC3_CH1=y
# CONFIG_SAMV7_TC3_CH2 is not set  # Conflicts with SPI0!
```

### 3.6 Implementation Steps for TC-Based PWM

1. **Create new io_timer_tc.c** (not stub)
   - Implement TC waveform mode for PWM generation
   - Use NuttX sam_tc.h APIs
   - Reference: `nuttx/arch/arm/src/samv7/sam_tc.c`

2. **Update timer_config.cpp**
   - Define TC-based io_timers[] array
   - Map timer_io_channels[] to TC TIOA outputs

3. **Update board_config.h**
   - Define GPIO pins for TC outputs
   - Set DIRECT_PWM_OUTPUT_CHANNELS to match

4. **Update CMakeLists.txt**
   - Use io_timer_tc.c instead of io_timer_pwmc.c

5. **Test without PWMOut first**
   - Verify TC registers can be accessed
   - Verify GPIO configuration works
   - Then enable PWM_OUT

---

## 4. Current Baseline Build

### 4.1 Build Configuration
- PWM_OUT: **Disabled**
- Flash usage: 1,294,296 bytes (61.72%)
- SRAM usage: 30,708 bytes (7.81%)

### 4.2 Working Features
- USB CDC/ACM
- SD Card (with PIO mode)
- SPI sensors (ICM20689, ICM45686)
- I2C sensors (AK09916, BMM150, DPS310, BMP388, BMI088)
- MAVLink
- Commander
- EKF2
- All MC control modules

### 4.3 Disabled/Pending Features
- PWM_OUT (crash investigation needed)
- DSHOT (not implemented)
- TX DMA for SD (causes issues)
- QSPI (scaffolding only)
- ADC (not implemented)

---

## 5. Files to Restore/Modify for TC Approach

### Keep As-Is:
- `io_timer.h` - Already has proper declarations
- `io_timer_hw_description.h` - Timer namespace and helpers

### Need New Implementation:
- `io_timer_tc.c` - Full TC waveform implementation (replace stub)

### Need Modification:
- `timer_config.cpp` - Change to TC-based configuration
- `board_config.h` - Update PWM pin definitions
- `defconfig` - Enable TC peripherals
- `CMakeLists.txt` - Point to io_timer_tc.c

---

## 6. TC Waveform Mode Reference

For PWM generation using TC in waveform mode:

```c
// TC Channel Mode Register settings for PWM
// TCCLKS = MCK/128 or appropriate divider
// WAVE = 1 (waveform mode)
// WAVSEL = 2 (UP mode with automatic trigger on RC compare)
// ACPA = 1 (RA compare: set TIOA)
// ACPC = 2 (RC compare: clear TIOA)

// Period = RC value
// Duty cycle = RA value (0 to RC)
```

Key registers per channel:
- CMR: Channel Mode Register
- RA: Register A (duty cycle threshold)
- RC: Register C (period)
- CCR: Channel Control Register (enable/disable)

---

## 7. Next Steps

1. Decide on TC pin mapping (avoid sensor conflicts)
2. Enable TC peripherals in NuttX defconfig
3. Implement io_timer_tc.c with waveform mode
4. Update timer_config.cpp for TC channels
5. Test TC register access without PWMOut
6. Enable PWMOut and test full integration
7. If PWMOut still crashes, investigate workqueue/MixingOutput issues

---

## 8. Reference Documents

- SAMV71 Datasheet: Timer/Counter section
- NuttX sam_tc.c implementation
- PX4 io_timer implementations for other platforms (STM32, etc.)
- SAMV71-XULT schematic for pin availability

# SAMV7 Holybro 500 Drone - Team Development Plan

**Goal:** Get a working drone flying on Holybro 500 frame with SAMV71 + Click board sensors
**Team Size:** 4 developers (You + Dev A + Dev B + Dev C)
**Tools:** All developers have Claude CLI access
**Current State:** 75% production ready, HITL verified

---

## CRITICAL PATH TO FIRST FLIGHT

```
Week 1: Parallel Development (All 4 devs working simultaneously)
        │
        ├── You: ADC + Power Module Integration
        ├── Dev A: Sensor Hardware + Real Sensor Mode
        ├── Dev B: PWM/Motor Configuration + ESC Setup
        └── Dev C: Safety Systems + Watchdog + Preflight
        │
Week 2: Integration & Testing
        │
        ├── Hardware Integration (All)
        ├── Sensor Calibration (Dev A)
        ├── Motor Testing (Dev B)
        └── Safety Validation (Dev C)
        │
Week 3: Flight Testing
        │
        └── Progressive Flight Tests (All)
```

---

## DEVELOPER ASSIGNMENTS

---

# YOU (Lead) - Power & System Integration

## Primary Tasks: ADC Driver + Battery Monitoring + System Integration

### Task Y1: Implement ADC Driver [CRITICAL]
**Priority:** P0 - Blocks flight testing
**Estimated Time:** 2-3 days
**Dependencies:** None

**Objective:** Implement PX4 ADC driver for SAMV7 AFEC peripheral

**Files to Create:**
```
platforms/nuttx/src/px4/microchip/samv7/adc/
├── adc.cpp          (~350 lines)
└── CMakeLists.txt
```

**Files to Modify:**
```
platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt  # Add adc subdirectory
boards/microchip/samv71-xult-clickboards/src/board_config.h  # ADC channel mapping
boards/microchip/samv71-xult-clickboards/default.px4board    # Enable ADC driver
```

**Implementation Steps:**
1. Study STM32 ADC implementation: `platforms/nuttx/src/px4/stm32_common/adc/adc.cpp`
2. Study NuttX SAMV7 AFEC driver: `nuttx/arch/arm/src/samv7/sam_afec.c`
3. Create ADC initialization function
4. Implement channel read functions
5. Map ADC channels for:
   - Battery voltage (with voltage divider calculation)
   - Battery current (if current sensor available)
   - Board version detection (optional)
6. Test with `adc test` command

**Claude CLI Prompt for You:**
```
I need to implement an ADC driver for SAMV7 (Microchip SAM V71) for PX4.
The NuttX AFEC driver exists at nuttx/arch/arm/src/samv7/sam_afec.c.
Reference the STM32 implementation at platforms/nuttx/src/px4/stm32_common/adc/adc.cpp.
Create platforms/nuttx/src/px4/microchip/samv7/adc/adc.cpp that:
1. Initializes AFEC0
2. Reads battery voltage channel
3. Integrates with PX4 power module
Help me implement this step by step.
```

**Acceptance Criteria:**
- [ ] `adc test` shows voltage readings
- [ ] Battery voltage appears in QGroundControl
- [ ] Low battery warnings work

---

### Task Y2: Configure Power Module [HIGH]
**Priority:** P1
**Estimated Time:** 1 day
**Dependencies:** Y1 (ADC Driver)

**Objective:** Configure battery monitoring for Holybro 500

**Parameters to Set:**
```bash
# Battery configuration
param set BAT1_V_DIV <calculated_divider>      # Voltage divider ratio
param set BAT1_A_PER_V <amps_per_volt>         # Current sensor calibration
param set BAT1_CAPACITY <mah>                   # Battery capacity
param set BAT1_N_CELLS 4                        # 4S LiPo typical
param set BAT1_V_EMPTY 3.5                      # Empty cell voltage
param set BAT1_V_CHARGED 4.2                    # Full cell voltage

# Low battery thresholds
param set BAT_LOW_THR 0.15                      # 15% warning
param set BAT_CRIT_THR 0.07                     # 7% critical
param set BAT_EMERGEN_THR 0.03                  # 3% emergency land
```

**Hardware Setup:**
- Identify power module voltage/current sense pins
- Connect to SAMV71 ADC inputs (document which pins)
- Measure actual voltage divider ratio

---

### Task Y3: System Integration Oversight [ONGOING]
**Priority:** P1
**Estimated Time:** Ongoing

**Responsibilities:**
- Code review for all team members
- Merge coordination
- Build verification after merges
- Final integration testing
- Document any blocking issues

**Daily Sync Points:**
- Morning: Check overnight build status
- Midday: Quick sync on blockers
- Evening: Merge window (all tested code)

---

### Task Y4: SD Card Write Investigation [MEDIUM]
**Priority:** P2 (if time permits)
**Estimated Time:** 2-3 days investigation

**Objective:** Debug SD card write hang for reliable logging

**Investigation Steps:**
1. Enable HSMCI debug logging in NuttX
2. Reproduce hang with continuous write test
3. Analyze PIO vs DMA write path
4. Check buffer management in `sam_hsmci.c`

**Note:** Flight testing can proceed with logging disabled if needed. Mark as lower priority than ADC.

---

# DEVELOPER A - Sensors & Calibration

## Primary Tasks: Real Sensor Integration + Calibration

### Task A1: Hardware Sensor Verification [CRITICAL]
**Priority:** P0 - Must complete first
**Estimated Time:** 1 day
**Dependencies:** Physical hardware access

**Objective:** Verify all Click board sensors are detected and working

**Hardware Checklist:**
```
□ ICM-20689 IMU Click (SPI) - MIKROE-4044
  - Connect to mikroBUS socket on SPI0
  - CS: PA11, DRDY: PA12 (already configured)

□ Magnetometer Click (I2C) - Choose ONE:
  - AK09916 (Compass 4 Click - MIKROE-4231) at 0x0C
  - BMM150 (GeoMagnetic Click - MIKROE-2935) at 0x10

□ Barometer Click (I2C) - Choose ONE:
  - DPS310 (Pressure 3 Click - MIKROE-2293) at 0x77
  - BMP388 (Pressure 5 Click - MIKROE-3566) at 0x76

□ Optional: Second IMU for redundancy
  - BMI088 (13DOF Click - MIKROE-3775) at 0x18/0x68
  - ICM-45686 (6DOF IMU 27 Click - MIKROE-6514) on SPI
```

**Verification Commands:**
```bash
# Scan I2C bus
i2cdetect 0

# Check each sensor
icm20689 status
ak09916 status      # or bmm150 status
dps310 status       # or bmp388 status

# Verify data flow
listener sensor_accel
listener sensor_gyro
listener sensor_mag
listener sensor_baro
```

---

### Task A2: Enable Real Sensor Mode [CRITICAL]
**Priority:** P0
**Estimated Time:** 2 hours
**Dependencies:** None (code change)

**Objective:** Change board defaults from HITL to real sensor mode

**Option 1: Modify rc.board_defaults (Recommended)**

Edit `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`:
```bash
# Change these lines:
# FROM:
param set-default SYS_HAS_MAG 0
param set-default SYS_HAS_BARO 0

# TO:
param set-default SYS_HAS_MAG 1
param set-default SYS_HAS_BARO 1

# Also change default airframe:
# FROM:
param set-default SYS_AUTOSTART 4001

# Keep as 4001 for real flight (not 1001 which is HITL)
```

**Option 2: Create Separate Config Files**
```
init/rc.board_defaults_hitl    # HITL mode settings
init/rc.board_defaults_real    # Real sensor settings
```

**Option 3: Runtime Parameter Change**
```bash
# On device (temporary, for testing)
param set SYS_HAS_MAG 1
param set SYS_HAS_BARO 1
param set SYS_AUTOSTART 4001
param save
reboot
```

---

### Task A3: ICM45686 SPI Configuration [HIGH - if using this sensor]
**Priority:** P1 (only if using ICM45686)
**Estimated Time:** 2 hours
**Dependencies:** Hardware - need second mikroBUS socket

**Objective:** Add second IMU to SPI bus configuration

**Files to Modify:**

`board_config.h` - Add GPIO definitions:
```c
// Add after ICM20689 definitions:
#define GPIO_SPI0_CS_ICM45686    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOX|GPIO_PINY)  // TODO: Pick GPIO
#define GPIO_SPI0_DRDY_ICM45686  (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_PORT_PIOX|GPIO_PINZ)   // TODO: Pick GPIO

// Update init list:
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
    GPIO_SPI0_CS_ICM45686,    \   // Add
    GPIO_SPI0_DRDY_ICM45686,  \   // Add
    GPIO_PWM1_OUT,            \
    GPIO_PWM2_OUT,            \
    GPIO_PWM3_OUT,            \
}
```

`spi.cpp` - Add device to bus:
```cpp
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    {
        .devices = {
            make_spidev(DRV_IMU_DEVTYPE_ICM20689, GPIO_SPI0_CS_ICM20689, GPIO_SPI0_DRDY_ICM20689),
            make_spidev(DRV_IMU_DEVTYPE_ICM45686, GPIO_SPI0_CS_ICM45686, GPIO_SPI0_DRDY_ICM45686),  // Add
        },
        // ... rest unchanged
    },
};
```

---

### Task A4: Sensor Calibration [CRITICAL]
**Priority:** P0 - Required before flight
**Estimated Time:** 1-2 hours per calibration type
**Dependencies:** A1, A2 complete

**Objective:** Calibrate all sensors for accurate flight

**Calibration Sequence:**
```bash
# 1. Gyroscope Calibration (keep drone still)
commander calibrate gyro

# 2. Accelerometer Calibration (6-position)
commander calibrate accel

# 3. Magnetometer Calibration (rotate in all axes)
commander calibrate mag

# 4. Level Horizon Calibration
commander calibrate level

# 5. Verify calibration
listener sensor_combined
ekf2 status
```

**QGroundControl Calibration:**
- Use QGC's guided calibration wizard
- More user-friendly than CLI
- Provides visual feedback

**Calibration Parameters to Verify:**
```bash
param show CAL_*          # All calibration params
param show EKF2_*         # EKF configuration
```

---

### Task A5: EKF2 Configuration [HIGH]
**Priority:** P1
**Estimated Time:** 4 hours
**Dependencies:** A4 complete

**Objective:** Configure EKF2 for optimal sensor fusion

**Key Parameters:**
```bash
# IMU selection
param set EKF2_IMU_ID <device_id>        # Primary IMU device ID

# Magnetometer
param set EKF2_MAG_TYPE 1                 # Automatic selection
param set EKF2_MAG_DELAY 0                # Mag delay compensation

# Barometer
param set EKF2_BARO_DELAY 0               # Baro delay compensation
param set EKF2_BARO_NOISE 3.5             # Baro noise (default)

# GPS (if using)
param set EKF2_GPS_DELAY 110              # GPS delay
param set EKF2_GPS_V_NOISE 0.3            # GPS velocity noise

# Height source
param set EKF2_HGT_REF 1                  # Baro as height reference
```

**Verification:**
```bash
ekf2 status
listener estimator_status
listener vehicle_local_position
listener vehicle_global_position   # If GPS connected
```

---

### Task A6: Sensor Documentation [MEDIUM]
**Priority:** P2
**Estimated Time:** 2 hours

**Objective:** Document final sensor configuration

**Create:** `boards/microchip/samv71-xult-clickboards/SENSOR_CONFIG.md`

Contents:
- Which Click boards are installed
- I2C addresses verified
- Calibration values recorded
- Any sensor-specific notes

---

**Claude CLI Prompt for Dev A:**
```
I'm setting up real sensors for a SAMV7 PX4 drone using Click board sensors.
Current board config is in boards/microchip/samv71-xult-clickboards/.
The board defaults are set for HITL mode (SYS_HAS_MAG=0, SYS_HAS_BARO=0).

Help me:
1. Change rc.board_defaults for real sensor mode
2. Verify I2C sensors are detected (AK09916, DPS310)
3. Configure the SPI IMU (ICM20689)
4. Set up sensor calibration procedure
5. Configure EKF2 for these sensors

The sensor driver commands are in rc.board_sensors.
```

---

# DEVELOPER B - Motors & PWM

## Primary Tasks: PWM Configuration + ESC Setup + Motor Testing

### Task B1: Holybro 500 Hardware Documentation [CRITICAL]
**Priority:** P0 - Do first
**Estimated Time:** 2 hours
**Dependencies:** Physical hardware access

**Objective:** Document the Holybro 500 frame wiring

**Information to Gather:**
```
□ ESC Type: _____________ (BLHeli_S, BLHeli_32, etc.)
□ ESC Protocol: _________ (PWM, OneShot, DShot - likely PWM for now)
□ Motor KV: _____________
□ Propeller Size: ________
□ Battery: ______________ (cells, mAh)
□ Frame Configuration: X or + (likely X)

□ Motor Positions (looking down, front = forward):
  Motor 1: Front-Right, CCW
  Motor 2: Back-Left, CCW
  Motor 3: Front-Left, CW
  Motor 4: Back-Right, CW
  (Verify against PX4 motor map!)

□ PWM Signal Wiring:
  PWM1 (PA15) -> Motor ___
  PWM2 (PC23) -> Motor ___
  PWM3 (PC26) -> Motor ___
  PWM4 -> NOT AVAILABLE (only 3 channels!)
```

**CRITICAL ISSUE:** SAMV7 only has 3 PWM channels configured!
- Quadcopter needs 4 motors
- Options:
  1. Add 4th PWM channel (requires finding available TC)
  2. Use I2C ESC (e.g., BLHeli_32 with I2C)
  3. Use CAN ESC (requires UAVCAN setup)

---

### Task B2: Add 4th PWM Channel [CRITICAL - BLOCKING]
**Priority:** P0 - MUST FIX for quadcopter
**Estimated Time:** 1-2 days
**Dependencies:** None

**Objective:** Configure 4th PWM output for quadcopter

**Current PWM Configuration:**
```
TC0 CH0 (TC0) - HRT (cannot use)
TC0 CH1 (TC1) - PWM1: PA15 ✓
TC0 CH2 (TC2) - BLOCKED (SD card DA2 conflict)
TC1 CH0 (TC3) - PWM2: PC23 ✓
TC1 CH1 (TC4) - PWM3: PC26 ✓
TC1 CH2 (TC5) - Reserved for RC input
TC2 CH0 (TC6) - Available?
TC2 CH1 (TC7) - Available?
TC2 CH2 (TC8) - Available?
TC3 exists on SAMV71
```

**Investigation Required:**
1. Check SAMV71 datasheet for TC2/TC3 pin assignments
2. Find GPIO that can route to TC2/TC3 TIOA output
3. Verify no conflicts with other peripherals

**Files to Modify:**

`nuttx-config/nsh/defconfig`:
```
# Add TC2 if available
CONFIG_SAMV7_TC2=y    # Enable Timer Counter block 2
```

`board_config.h`:
```c
// Add 4th PWM output
#define GPIO_PWM4_OUT    (GPIO_PERIPHX | GPIO_CFG_DEFAULT | GPIO_PORT_PIOX | GPIO_PINY)
#define DIRECT_PWM_OUTPUT_CHANNELS  4   // Change from 3 to 4
#define BOARD_NUM_IO_TIMERS 4           // Change from 3 to 4
```

`timer_config.cpp`:
```cpp
// Add 4th timer configuration
```

**Alternative: Use Different Motor Count**
- Tricopter (3 motors) - works with current config
- Hexacopter Y6 (6 motors, 3 pairs) - might work with 3 PWM

---

### Task B3: ESC Configuration [HIGH]
**Priority:** P1
**Estimated Time:** 4 hours
**Dependencies:** B2 complete (4 PWM channels)

**Objective:** Configure ESC parameters

**ESC Parameters:**
```bash
# PWM output configuration
param set PWM_MAIN_RATE 400              # PWM frequency (400Hz typical)
param set PWM_MAIN_MIN 1000              # Minimum pulse width
param set PWM_MAIN_MAX 2000              # Maximum pulse width
param set PWM_MAIN_DISARM 900            # Disarmed pulse width

# Or if using HIL_ACT prefix (check which is active):
param set HIL_ACT_RATE 400
param set HIL_ACT_MIN1 1000
param set HIL_ACT_MAX1 2000
# ... etc for each channel
```

**Motor Assignment:**
```bash
# Quadcopter X configuration (airframe 4001)
param set PWM_MAIN_FUNC1 101    # Motor 1
param set PWM_MAIN_FUNC2 102    # Motor 2
param set PWM_MAIN_FUNC3 103    # Motor 3
param set PWM_MAIN_FUNC4 104    # Motor 4
```

---

### Task B4: Motor Testing [CRITICAL]
**Priority:** P0 - Safety critical
**Estimated Time:** 2 hours
**Dependencies:** B2, B3 complete

**Objective:** Verify all motors spin correctly

**SAFETY WARNING:**
```
⚠️ REMOVE ALL PROPELLERS FOR MOTOR TESTING ⚠️
⚠️ SECURE DRONE TO TEST STAND ⚠️
⚠️ HAVE KILL SWITCH READY ⚠️
```

**Motor Test Sequence:**
```bash
# Method 1: actuator_test command
actuator_test set -m 1 -v 0.1    # Motor 1 at 10%
actuator_test set -m 1 -v 0      # Stop motor 1
actuator_test set -m 2 -v 0.1    # Motor 2 at 10%
# ... test each motor

# Method 2: commander motor test
commander motor_test -m 1 -v 0.1
commander motor_test -m 2 -v 0.1
commander motor_test -m 3 -v 0.1
commander motor_test -m 4 -v 0.1
```

**Verification Checklist:**
```
□ Motor 1 spins CCW (looking down)
□ Motor 2 spins CCW
□ Motor 3 spins CW
□ Motor 4 spins CW
□ All motors respond to throttle commands
□ No abnormal sounds or vibrations
□ ESC calibration completed (if needed)
```

---

### Task B5: ESC Calibration [HIGH]
**Priority:** P1
**Estimated Time:** 30 minutes
**Dependencies:** B3, B4 complete

**Objective:** Calibrate ESC throttle range

**ESC Calibration Procedure:**
```bash
# Set calibration mode
param set PWM_MAIN_MIN 1000
param set PWM_MAIN_MAX 2000

# Option 1: All-at-once calibration
# 1. Disconnect battery
# 2. Set PWM to max:
param set PWM_MAIN_DISARM 2000
# 3. Connect battery (hear ESC beeps)
# 4. After beeps, set to min:
param set PWM_MAIN_DISARM 1000
# 5. Wait for confirmation beeps
# 6. Reset disarm value:
param set PWM_MAIN_DISARM 900

# Option 2: Use ESC's built-in calibration (varies by brand)
```

---

### Task B6: Airframe Configuration [HIGH]
**Priority:** P1
**Estimated Time:** 2 hours
**Dependencies:** B4 complete

**Objective:** Configure correct airframe parameters

**For Holybro 500 (Generic Quadcopter X):**
```bash
param set SYS_AUTOSTART 4001    # Generic Quad X

# Or create custom airframe for SAMV71:
# See: ROMFS/px4fmu_common/init.d/airframes/
```

**Key Airframe Parameters:**
```bash
# Motor mixing
param set CA_ROTOR_COUNT 4
param set CA_ROTOR0_PX 0.1515    # Motor positions
param set CA_ROTOR0_PY 0.245
# ... (use defaults from 4001 or tune)

# PID tuning (start with defaults, tune later)
param set MC_ROLLRATE_P 0.15
param set MC_PITCHRATE_P 0.15
param set MC_YAWRATE_P 0.2
```

---

### Task B7: RC Input Configuration [HIGH]
**Priority:** P1
**Estimated Time:** 2 hours
**Dependencies:** Hardware - RC receiver

**Objective:** Configure RC receiver input

**RC Options:**
1. **SBUS** (preferred) - Connect to UART
2. **PPM** - Single wire, connect to capture pin
3. **PWM** - Multiple wires (not recommended)

**Configuration:**
```bash
# For SBUS on UART4:
param set RC_PORT_CONFIG 300      # Or appropriate port number

# RC channel mapping
param set RC_MAP_THROTTLE 3
param set RC_MAP_ROLL 1
param set RC_MAP_PITCH 2
param set RC_MAP_YAW 4
param set RC_MAP_ARM_SW <channel>  # Arm switch channel
```

---

**Claude CLI Prompt for Dev B:**
```
I'm configuring motors for a SAMV7 PX4 drone using a Holybro 500 frame (quadcopter X).
Current config only has 3 PWM channels, but I need 4 for a quadcopter.

Current PWM pins:
- PWM1: PA15 (TC1)
- PWM2: PC23 (TC3)
- PWM3: PC26 (TC4)

Board files are in boards/microchip/samv71-xult-clickboards/.
Timer config is in src/timer_config.cpp.

Help me:
1. Find an available Timer Counter for 4th PWM channel on SAMV71
2. Add the 4th PWM configuration
3. Set up motor testing safely
4. Configure ESC parameters for standard PWM ESCs
```

---

# DEVELOPER C - Safety & Preflight

## Primary Tasks: Watchdog + Safety Systems + Preflight Checks

### Task C1: Implement Watchdog Timer [CRITICAL]
**Priority:** P0 - System reliability
**Estimated Time:** 1-2 days
**Dependencies:** None

**Objective:** Add hardware watchdog for fail-safe operation

**Files to Create:**
```
platforms/nuttx/src/px4/microchip/samv7/watchdog/
├── watchdog.c       (~150 lines)
└── CMakeLists.txt
```

**Implementation Reference:**
- STM32: `platforms/nuttx/src/px4/stm32/stm32f7/watchdog/`
- NuttX SAMV7: `nuttx/arch/arm/src/samv7/sam_wdt.c`

**SAMV7 Watchdog Features:**
- 256-second maximum timeout (at 32kHz)
- 3.9ms minimum timeout
- Can trigger interrupt or system reset
- 12-bit reload counter

**Implementation Steps:**
1. Initialize WDT with appropriate timeout (e.g., 10 seconds)
2. Create watchdog kick function
3. Integrate with PX4 main loop
4. Test watchdog reset (intentionally hang system)

**Claude CLI Prompt for Dev C:**
```
I need to implement a hardware watchdog timer for SAMV7 PX4.
The NuttX driver is at nuttx/arch/arm/src/samv7/sam_wdt.c.
Reference STM32 watchdog at platforms/nuttx/src/px4/stm32/stm32f7/watchdog/.

Create platforms/nuttx/src/px4/microchip/samv7/watchdog/watchdog.c that:
1. Initializes SAMV7 WDT with 10-second timeout
2. Provides a kick function called from main loop
3. Resets system if watchdog expires

Help me implement this.
```

---

### Task C2: Arming Checks Configuration [CRITICAL]
**Priority:** P0
**Estimated Time:** 4 hours
**Dependencies:** Sensors and motors working

**Objective:** Configure comprehensive preflight checks

**Arming Check Parameters:**
```bash
# Enable all checks for production
param set COM_ARM_CHK_ESCS 1     # ESC check
param set COM_ARM_IMU_ACC 0.7    # Accel consistency
param set COM_ARM_IMU_GYR 0.25   # Gyro consistency
param set COM_ARM_MAG_ANG 45     # Mag vs GPS heading

# For initial testing, you may temporarily disable some:
param set COM_ARM_WO_GPS 1       # Allow arm without GPS (indoor testing)
param set CBRK_SUPPLY_CHK 0      # Re-enable power check (was 894281 for HITL)
```

**Required Checks Before Arming:**
```
□ Sensors calibrated
□ EKF healthy (ekf2 status)
□ Battery voltage in range
□ RC connected and configured
□ Motors tested
□ Propellers secured
□ Flight mode valid
```

---

### Task C3: Failsafe Configuration [CRITICAL]
**Priority:** P0
**Estimated Time:** 4 hours
**Dependencies:** None

**Objective:** Configure all failsafe behaviors

**Failsafe Parameters:**
```bash
# RC Loss Failsafe
param set COM_RC_LOSS_T 0.5      # RC loss timeout (seconds)
param set NAV_RCL_ACT 2          # Action: 0=disabled, 1=loiter, 2=RTL, 3=land

# Battery Failsafe
param set BAT_LOW_THR 0.15       # Low battery warning (15%)
param set BAT_CRIT_THR 0.07      # Critical battery (7%)
param set BAT_EMERGEN_THR 0.03   # Emergency (3%)
param set COM_LOW_BAT_ACT 2      # Action: 0=none, 1=warning, 2=RTL, 3=land

# Data Link Loss
param set COM_DL_LOSS_T 10       # Link loss timeout
param set NAV_DLL_ACT 0          # Action: 0=disabled, 1=loiter, 2=RTL

# Geofence (optional but recommended)
param set GF_ACTION 1            # Action on breach
param set GF_MAX_HOR_DIST 200    # Max horizontal distance (m)
param set GF_MAX_VER_DIST 100    # Max vertical distance (m)

# Flight termination (emergency only)
param set CBRK_FLIGHTTERM 0      # Enable flight termination (was 121212 for HITL)
```

---

### Task C4: Kill Switch Configuration [CRITICAL]
**Priority:** P0 - Safety critical
**Estimated Time:** 1 hour
**Dependencies:** RC input working

**Objective:** Configure hardware kill switch

**Kill Switch Setup:**
```bash
# Map kill switch to RC channel
param set RC_MAP_KILL_SW <channel>    # Usually channel 5 or 6
param set COM_KILL_DISARM 5           # Disarm after 5 seconds of kill

# Or use arm switch as kill (toggle to disarm)
param set RC_MAP_ARM_SW <channel>
```

**Test Kill Switch:**
1. Arm drone (props off!)
2. Engage kill switch
3. Verify immediate motor stop
4. Verify disarm after timeout

---

### Task C5: Preflight Checklist System [HIGH]
**Priority:** P1
**Estimated Time:** 4 hours
**Dependencies:** None

**Objective:** Create comprehensive preflight procedure

**Create:** `boards/microchip/samv71-xult-clickboards/PREFLIGHT_CHECKLIST.md`

**Contents:**
```markdown
# SAMV71 Drone Preflight Checklist

## Before Every Flight

### Ground Checks (Props OFF)
□ Battery charged and secure
□ All connections tight
□ Propellers undamaged
□ Frame integrity
□ SD card inserted

### Power-On Checks
□ Boot completes without errors
□ LED shows normal status
□ QGC connects successfully
□ sensor_status shows all sensors
□ ekf2 status shows healthy

### Sensor Verification
□ listener sensor_accel - data flowing
□ listener sensor_gyro - data flowing
□ listener sensor_baro - reasonable altitude
□ listener sensor_mag - reasonable values
□ listener vehicle_attitude - stable when still

### Motor Checks (Props OFF)
□ actuator_test each motor responds
□ Correct spin direction verified
□ No abnormal sounds

### Final Checks
□ RC transmitter on and bound
□ RC failsafe configured
□ GPS lock (if required)
□ Flight area clear
□ Weather acceptable

### Arming
□ commander arm
□ Motors respond to throttle
□ Ready for takeoff
```

---

### Task C6: Flight Logging Verification [HIGH]
**Priority:** P1
**Estimated Time:** 2 hours
**Dependencies:** SD card working

**Objective:** Verify flight logs are captured

**Logging Configuration:**
```bash
# Enable logging
param set SDLOG_MODE 0          # Log on arm
param set SDLOG_PROFILE 1       # Default profile

# Or for debugging:
param set SDLOG_MODE -1         # Log from boot
```

**Log Verification:**
```bash
# Check log directory
ls /fs/microsd/log/

# Check log file growing during flight
# Use QGC to download and analyze logs
```

**Known Issue:** SD write hang may affect logging. Test thoroughly before relying on logs.

---

### Task C7: Emergency Procedures Documentation [HIGH]
**Priority:** P1
**Estimated Time:** 2 hours
**Dependencies:** None

**Objective:** Document emergency procedures

**Create:** `boards/microchip/samv71-xult-clickboards/EMERGENCY_PROCEDURES.md`

**Contents:**
- Loss of RC: What to expect, RTL behavior
- Low battery: Warning sequences, auto-land
- GPS loss: Altitude hold behavior
- Motor failure: Identification, response
- Flyaway: Kill switch, geofence
- Crash recovery: Data preservation, inspection

---

**Claude CLI Prompt for Dev C:**
```
I'm implementing safety systems for a SAMV7 PX4 drone.
Help me:
1. Create a watchdog timer driver for SAMV7 using nuttx/arch/arm/src/samv7/sam_wdt.c
2. Configure comprehensive arming checks for a quadcopter
3. Set up failsafe parameters (RC loss, battery, data link)
4. Configure a hardware kill switch on an RC channel
5. Create a preflight checklist document

The drone will fly with real sensors (IMU, mag, baro) on a Holybro 500 frame.
Board files are in boards/microchip/samv71-xult-clickboards/.
```

---

# SHARED TASKS (All Developers)

## Integration Phase (Week 2)

### Task S1: Hardware Assembly
**Assigned:** All (physical assembly day)
**Estimated Time:** 1 day

**Assembly Checklist:**
```
□ Mount SAMV71 board to Holybro 500 frame
□ Connect ESCs to motors
□ Connect PWM outputs to ESCs
□ Connect power module to battery and SAMV71
□ Mount Click board sensors (vibration isolated if possible)
□ Connect RC receiver
□ Connect GPS (if using)
□ Route and secure all wiring
□ Attach propellers (ONLY when ready for flight test)
```

---

### Task S2: System Integration Testing
**Assigned:** All
**Estimated Time:** 1 day

**Integration Tests:**
```
□ Full boot with all components connected
□ All sensors reporting data
□ All motors responding to commands
□ Battery voltage reading correctly
□ RC input working
□ QGC showing all telemetry
□ Arm/disarm cycle works
□ Kill switch works
□ Failsafes trigger correctly
```

---

### Task S3: Flight Testing (Week 3)
**Assigned:** All (designated pilot)
**Estimated Time:** Multiple sessions

**Progressive Flight Test Plan:**

**Test 1: Tethered/Constrained**
```
□ Drone tethered to ground
□ Low throttle hover attempt
□ Verify stability
□ Verify motor response
```

**Test 2: Low Hover**
```
□ Untethered, 0.5m hover
□ Stabilization check
□ 30-second hover
□ Controlled landing
```

**Test 3: Basic Maneuvering**
```
□ 2m hover
□ Gentle pitch/roll
□ Yaw rotation
□ Position hold test
```

**Test 4: Extended Flight**
```
□ 5-minute flight
□ Various altitudes
□ Position mode testing
□ RTL test
□ Battery failsafe test (simulated)
```

---

## SCHEDULE OVERVIEW

```
┌─────────────────────────────────────────────────────────────────────┐
│                           WEEK 1                                     │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────────┤
│    YOU      │   DEV A     │   DEV B     │   DEV C     │    ALL      │
├─────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
│ Y1: ADC     │ A1: Sensor  │ B1: Doc HW  │ C1: Watchdog│             │
│ Driver      │ Verify      │ Config      │             │             │
│ (2-3 days)  │ (1 day)     │ (2 hrs)     │ (1-2 days)  │             │
├─────────────┼─────────────┼─────────────┼─────────────┤             │
│ Y2: Power   │ A2: Real    │ B2: 4th PWM │ C2: Arming  │             │
│ Module      │ Sensor Mode │ Channel     │ Checks      │             │
│ (1 day)     │ (2 hrs)     │ (1-2 days)  │ (4 hrs)     │             │
├─────────────┼─────────────┼─────────────┼─────────────┤             │
│             │ A3: ICM45686│ B3: ESC     │ C3: Failsafe│             │
│             │ (optional)  │ Config      │ (4 hrs)     │             │
│             │ (2 hrs)     │ (4 hrs)     │             │             │
├─────────────┼─────────────┼─────────────┼─────────────┤             │
│ Y3: Code    │ A4: Calibra-│ B4: Motor   │ C4: Kill    │             │
│ Review      │ tion        │ Testing     │ Switch      │             │
│ (ongoing)   │ (1-2 hrs)   │ (2 hrs)     │ (1 hr)      │             │
└─────────────┴─────────────┴─────────────┴─────────────┴─────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                           WEEK 2                                     │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────────┤
│    YOU      │   DEV A     │   DEV B     │   DEV C     │    ALL      │
├─────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
│ Integration │ A5: EKF2    │ B5: ESC     │ C5: Preflight│ S1: HW     │
│ Oversight   │ Config      │ Calibration │ Checklist    │ Assembly   │
│             │ (4 hrs)     │ (30 min)    │ (4 hrs)      │ (1 day)    │
├─────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
│ Y4: SD Fix  │ A6: Sensor  │ B6: Airframe│ C6: Logging │ S2: System │
│ (if time)   │ Docs        │ Config      │ Verify      │ Test       │
│             │ (2 hrs)     │ (2 hrs)     │ (2 hrs)     │ (1 day)    │
├─────────────┼─────────────┼─────────────┼─────────────┼─────────────┤
│             │             │ B7: RC      │ C7: Emergency│            │
│             │             │ Config      │ Docs         │            │
│             │             │ (2 hrs)     │ (2 hrs)      │            │
└─────────────┴─────────────┴─────────────┴─────────────┴─────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                           WEEK 3                                     │
├─────────────────────────────────────────────────────────────────────┤
│                    S3: FLIGHT TESTING (ALL)                         │
├─────────────────────────────────────────────────────────────────────┤
│ Day 1: Tethered tests                                               │
│ Day 2: Low hover tests                                              │
│ Day 3: Basic maneuvering                                            │
│ Day 4: Extended flight tests                                        │
│ Day 5: Documentation and tuning                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## COMMUNICATION PLAN

### Daily Standups
- **Time:** Start of each work session
- **Format:** Quick update from each dev
- **Topics:** Progress, blockers, needs from others

### Code Integration
- **Branch Strategy:** Feature branches, merge to main after review
- **Review Required:** At least one other dev reviews before merge
- **Build Verification:** Must build successfully before merge

### Shared Resources
- Claude CLI prompts provided above for each task
- All documentation in `boards/microchip/samv71-xult-clickboards/`
- Hardware shared access schedule if only one board

---

## BLOCKING DEPENDENCIES

```
B2 (4th PWM) ──BLOCKS──> B4 (Motor Test) ──BLOCKS──> Flight Test
                              │
A2 (Real Sensor Mode) ────────┤
                              │
Y1 (ADC Driver) ──────────────┘

A4 (Calibration) ──BLOCKS──> Flight Test

C1 (Watchdog) ──RECOMMENDED BEFORE──> Flight Test
```

**Critical Path:** B2 → B4 → Flight Test
**Most likely bottleneck:** 4th PWM channel configuration

---

## SUCCESS CRITERIA

### Milestone 1: End of Week 1
- [ ] ADC driver working (YOU)
- [ ] Real sensors detected and calibrated (Dev A)
- [ ] 4 PWM channels working (Dev B)
- [ ] Watchdog implemented (Dev C)

### Milestone 2: End of Week 2
- [ ] Drone fully assembled
- [ ] All systems integrated
- [ ] Ground tests pass
- [ ] Ready for flight

### Milestone 3: End of Week 3
- [ ] Successful hover flight
- [ ] Basic maneuvers work
- [ ] Safety systems verified
- [ ] Flight logs captured

---

**Document Created:** 2025-12-06
**Author:** Claude Code

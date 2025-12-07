# SAMV7 PX4 Production Development Plan - Version 2

**Date:** 2025-12-07
**Project Goal:** Production-ready PX4 flight controller on Microchip SAMV71
**Target:** Holybro 500 drone frame - tethered test to full autonomous flight
**Roadmap:** SAMV71 → PIC32CZ70 → PIC32CZ90

---

## Team Structure

| Team Member | Location | Availability | Hardware Access | Primary Role |
|-------------|----------|--------------|-----------------|--------------|
| **Bhanu** | India | 1 week (then travel) | Click boards, SAMV71, Holybro 500 | Core platform development (ADC, PWM, IO) |
| **Syed** | India | 3 weeks | Click boards, SAMV71, scope | Sensor development & validation |
| **Vivek** | India | Ongoing | SAMV71, scope (NO Click boards) | HITL testing & hardware validation |
| **Vignesh** | India | Starting | Setting up | Environment setup, learning |
| **Ajit & Team** | India | On-call | Hardware tools | Custom FC PCB, mechanical integration |
| **US Team** | USA | Later | Replicating setup | Mirror validation, parallel testing |

---

## Part 1: Platform Comparison - Production Gap Analysis

### SAMV7 vs STM32 FMU-v6x (Pixhawk 6X) - Feature Matrix

| Category | Feature | FMU-v6x | SAMV7 | Gap | Priority |
|----------|---------|---------|-------|-----|----------|
| **ADC** | Battery Voltage | ✅ ADC1 CH0 | ❌ Placeholder | CRITICAL | P0 |
| | Battery Current | ✅ ADC1 CH1 | ❌ Placeholder | CRITICAL | P0 |
| | 3.3V Sensor Rail | ✅ 4 channels | ❌ Missing | HIGH | P1 |
| | 5V/6.6V Rails | ✅ Monitored | ❌ Missing | MEDIUM | P2 |
| | HW Version Detect | ✅ ADC3 | ❌ Missing | LOW | P3 |
| **PWM** | Output Channels | ✅ 9 channels | ⚠️ 3 channels | CRITICAL | P0 |
| | DShot Support | ✅ Full | ❌ Missing | HIGH | P1 |
| | Tone Alarm | ✅ Timer 14 | ❌ Missing | MEDIUM | P2 |
| | Input Capture | ✅ Timer 1 | ❌ Missing | MEDIUM | P2 |
| **Safety** | Safety Button | ✅ GPIO | ❌ Missing | HIGH | P1 |
| | Safety LED | ✅ GPIO | ❌ Missing | HIGH | P1 |
| | Armed State LED | ✅ Mapped | ⚠️ Single LED | MEDIUM | P2 |
| **Power** | Dual Brick Input | ✅ 2 inputs | ❌ Missing | HIGH | P1 |
| | Overcurrent Detect | ✅ GPIO | ❌ Missing | MEDIUM | P2 |
| | Rail Enable Control | ✅ Multiple | ❌ Missing | LOW | P3 |
| **Storage** | SD Card | ✅ SDIO | ✅ HSMCI | DONE | - |
| | MTD/EEPROM | ✅ I2C EEPROM | ❌ Missing | HIGH | P1 |
| | Parameter Backup | ✅ Flash | ❌ SD only | MEDIUM | P2 |
| **USB** | VBUS Detection | ✅ GPIO | ⚠️ Stubbed | LOW | P3 |
| | Composite Device | ✅ Full | ⚠️ CDC only | LOW | P3 |
| **CAN** | CAN Bus | ✅ Dual CAN | ❌ Missing | MEDIUM | P2 |
| | UAVCAN | ✅ Supported | ❌ Missing | MEDIUM | P2 |
| **Bootloader** | USB Bootload | ✅ Full | ❌ Missing | HIGH | P1 |
| | Serial Bootload | ✅ 1.5Mbps | ❌ Missing | HIGH | P1 |
| | Board Type ID | ✅ Type 53 | ❌ Missing | HIGH | P1 |
| **Sensors** | Multiple IMU | ✅ 3 IMU sets | ⚠️ 1 IMU | MEDIUM | P2 |
| | Redundancy | ✅ Full | ⚠️ Limited | MEDIUM | P2 |
| **Init** | Board Init | ⚠️ Minimal | ✅ Comprehensive | DONE | - |
| | HRT Setup | ✅ Timer 8 | ✅ TC0/PCK6 | DONE | - |
| | DMA Allocator | ✅ 5120 bytes | ✅ 5120 bytes | DONE | - |

### SAMV7 vs NXP iMXRT (mr-tropic) - Feature Matrix

| Category | Feature | NXP iMXRT | SAMV7 | Notes |
|----------|---------|-----------|-------|-------|
| **Architecture** | Core | Cortex-M7 | Cortex-M7 | Same core |
| | Clock | 600 MHz | 300 MHz | NXP faster |
| | RAM | 1 MB | 384 KB | NXP more |
| **PWM** | Channels | 8 | 3 | NXP uses FlexPWM |
| **ADC** | Channels | Multiple | Placeholder | Both need work |
| **Ethernet** | Support | ✅ Yes | ❌ No | NXP advantage |
| **Bootloader** | Size | 73 KB | ❌ None | NXP production |
| **HW Versioning** | Variants | 3 SPI configs | ❌ None | NXP sophisticated |

### What's WORKING on SAMV7 (Green List)

```
✅ HRT Timer (TC0, PCK6 1MHz)
✅ System Clock (300MHz CPU, 150MHz MCK)
✅ GPIO with interrupts
✅ MPU/DMA nocache region
✅ Board reset handling
✅ CPUID/UUID identification
✅ Console buffer
✅ Crash dump framework
✅ UART (UART0, UART2, UART4, USART1)
✅ USB CDC/ACM (High-Speed)
✅ SPI with DMA (SPI0)
✅ I2C (TWIHS0)
✅ MAVLink telemetry
✅ SD Card (HSMCI0) - reads reliable
✅ Parameter storage to SD
✅ ROMFS/etc startup scripts
✅ Flight logging (ULog format)
✅ MixingOutput (runtime init fix)
✅ EKF2 with 6 fusion algorithms
✅ Multicopter control stack
✅ Commander and arming
✅ RC input processing
✅ 7 Click board sensor drivers
✅ HITL simulation verified
```

### What's MISSING on SAMV7 (Red List - Priority Order)

```
❌ P0 - CRITICAL (Blocks Flight):
   - ADC driver for battery monitoring
   - 4th PWM channel for quadcopter
   - Watchdog timer

❌ P1 - HIGH (Production Required):
   - DShot ESC protocol
   - MTD/EEPROM parameter backup
   - Bootloader (USB + Serial)
   - Safety switch (button + LED)

❌ P2 - MEDIUM (Enhanced Features):
   - Tone alarm/buzzer
   - CAN bus / UAVCAN
   - PWM input capture
   - Multi-IMU redundancy
   - Power rail monitoring

❌ P3 - LOW (Nice to Have):
   - USB VBUS detection
   - Hardware version detection
   - Ethernet (N/A on XULT)
```

---

## Part 2: Development Team Assignments

### Timeline Overview

```
Week 1 (Dec 7-14): Core Development Sprint
├── Bhanu: ADC + 4th PWM + Core IO (INTENSIVE - last week before travel)
├── Syed: Sensor drivers + I2C validation
├── Vivek: HITL pipeline fix + validation setup
└── Vignesh: Environment setup + documentation review

Week 2-3 (Dec 14-28): Bhanu Traveling
├── Syed: Sensor integration + EKF2 tuning + calibration
├── Vivek: Hardware validation + scope testing + HITL
└── Vignesh: Join development (basic tasks)

Week 4+ (Dec 28+): Integration & Flight
├── All: Integration testing
├── Ajit: Hardware modifications begin
└── US Team: Joins for parallel validation
```

---

## BHANU (Lead) - Core Platform Development

### Availability: 1 WEEK INTENSIVE (Dec 7-14)

**Focus:** Core infrastructure that others depend on

### Task B1: ADC Driver Implementation [CRITICAL - Day 1-2]
**Priority:** P0 - Blocks battery monitoring
**Estimated Time:** 2 days

**Reference Implementation:**
- STM32: `platforms/nuttx/src/px4/stm32_common/adc/adc.cpp`
- TII PolarFire: `/media/bhanu1234/Development/PX4-PolarFire-Icicle/boards/mpfs/icicle/src/board_config.h`
- NuttX AFEC: `nuttx/arch/arm/src/samv7/sam_afec.c`

**Files to Create:**
```
platforms/nuttx/src/px4/microchip/samv7/adc/
├── adc.cpp          (~400 lines)
└── CMakeLists.txt
```

**Files to Modify:**
```
platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt
boards/microchip/samv71-xult-clickboards/src/board_config.h
boards/microchip/samv71-xult-clickboards/default.px4board
```

**Implementation from TII PolarFire (copy pattern):**
```c
// Add to board_config.h:
#define ADC_BATTERY_VOLTAGE_CHANNEL     0   // AFEC0 channel
#define ADC_BATTERY_CURRENT_CHANNEL     1   // AFEC0 channel
#define ADC_CHANNELS                    ((1 << 0) | (1 << 1))

// Battery calibration
#define BOARD_ADC_BRICK_VALID           1
#define BOARD_NUMBER_BRICKS             1
```

**Validation (Syed/Vivek with scope):**
- Measure actual ADC pin voltage
- Compare with `adc test` output
- Verify battery_status topic

---

### Task B2: 4th PWM Channel [CRITICAL - Day 2-3]
**Priority:** P0 - Blocks quadcopter motor control
**Estimated Time:** 1.5 days

**Current State:**
```
TC0 CH0 - HRT (USED)
TC0 CH1 - PWM1: PA15 ✓
TC0 CH2 - BLOCKED (SD card DA2)
TC1 CH0 - PWM2: PC23 ✓
TC1 CH1 - PWM3: PC26 ✓
TC1 CH2 - Reserved RC input
TC2 - AVAILABLE for PWM4
```

**Solution: Use TC2 for 4th PWM**

**Files to Modify:**
```
boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig
  + CONFIG_SAMV7_TC2=y

boards/microchip/samv71-xult-clickboards/src/board_config.h
  + #define GPIO_PWM4_OUT (GPIO_PERIPHB | GPIO_PORT_PIOX | GPIO_PINY)
  + #define DIRECT_PWM_OUTPUT_CHANNELS 4
  + #define BOARD_NUM_IO_TIMERS 4

boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
  + Add TC2 channel configuration
```

**Validation (Vivek with scope):**
- Verify PWM4 signal on pin
- Check frequency (50Hz default, 400Hz for ESC)
- Verify all 4 motors respond

---

### Task B3: Watchdog Timer [CRITICAL - Day 3]
**Priority:** P0 - System reliability
**Estimated Time:** 0.5 days

**Reference:**
- NuttX: `nuttx/arch/arm/src/samv7/sam_wdt.c`
- STM32: `platforms/nuttx/src/px4/stm32/stm32f7/watchdog/`

**Files to Create:**
```
platforms/nuttx/src/px4/microchip/samv7/watchdog/
├── watchdog.c       (~150 lines)
└── CMakeLists.txt
```

**Implementation:**
- 10-second timeout
- Kick from main loop
- Reset on expiry

---

### Task B4: GPIO/IO Pin Mapping Documentation [Day 3-4]
**Priority:** P1 - Required for hardware team
**Estimated Time:** 1 day

**Deliverable:** Complete GPIO mapping document for Ajit's team

**Create:** `boards/microchip/samv71-xult-clickboards/GPIO_PINOUT.md`

Contents:
- All used GPIO pins with function
- Available GPIO pins for expansion
- Conflict matrix (what can't be used together)
- mikroBUS socket pinout
- Arduino header pinout
- Recommended pins for custom FC

---

### Task B5: MTD/EEPROM Parameter Storage [Day 4-5]
**Priority:** P1 - Production requirement
**Estimated Time:** 1.5 days

**Reference:**
- TII PolarFire: `/media/bhanu1234/Development/PX4-PolarFire-Icicle/boards/mpfs/icicle/src/mtd.cpp`

**Hardware Required:**
- I2C EEPROM (AT24C04/08/16) on Click board or external

**Files to Create:**
```
boards/microchip/samv71-xult-clickboards/src/mtd.cpp
```

**Implementation:**
```cpp
// From TII PolarFire pattern:
static const px4_mtd_entry_t eeprom = {
    .device = &i2c0,
    .npart = 1,
    .partd = {
        {
            .type = MTD_PARAMETERS,
            .path = "/fs/mtd_params",
            .nblocks = 256  // 8KB
        }
    }
};
```

---

### Task B6: Code Handoff Documentation [Day 5]
**Priority:** P0 - Critical for continuity
**Estimated Time:** 0.5 days

**Create:** `HANDOFF_NOTES.md` with:
- What was completed
- What's in progress
- Known issues
- Next steps for Syed/Vivek
- Build/test commands

---

### Bhanu Week 1 Schedule

| Day | Morning | Afternoon | Evening |
|-----|---------|-----------|---------|
| Day 1 (Sat) | ADC driver design | ADC implementation | ADC testing |
| Day 2 (Sun) | ADC completion | 4th PWM research | PWM implementation |
| Day 3 (Mon) | 4th PWM testing | Watchdog impl | Watchdog test |
| Day 4 (Tue) | GPIO documentation | MTD/EEPROM design | MTD impl start |
| Day 5 (Wed) | MTD completion | Integration test | Handoff docs |

---

## SYED - Sensor Development & Validation

### Availability: 3 WEEKS (Dec 7-28)
### Hardware: Click boards, SAMV71, oscilloscope

**Focus:** All sensor code, calibration, and hardware validation

### Week 1 Tasks (Parallel with Bhanu)

#### Task S1: I2C Sensor Bus Validation [Day 1-2]
**Priority:** P0
**Dependencies:** None

**Objective:** Verify all I2C sensors are detected and communicating

**Commands:**
```bash
i2cdetect 0                    # Scan I2C bus
ak09916 status                 # Magnetometer
dps310 status                  # Barometer
bmp388 status                  # Barometer alternate
bmm150 status                  # Magnetometer alternate
bmi088_i2c -A status           # Accel
bmi088_i2c -G status           # Gyro
```

**Validation with Scope:**
- Probe SDA/SCL lines
- Verify I2C timing (100kHz or 400kHz)
- Check for bus errors/NACK

**Deliverable:** I2C validation report with oscilloscope captures

---

#### Task S2: SPI IMU Validation [Day 2-3]
**Priority:** P0
**Dependencies:** None

**Objective:** Verify ICM-20689 SPI communication

**Commands:**
```bash
icm20689 status
listener sensor_accel
listener sensor_gyro
```

**Validation with Scope:**
- Probe MOSI, MISO, SCK, CS
- Verify SPI timing
- Check DRDY interrupt

**Deliverable:** SPI validation report with oscilloscope captures

---

#### Task S3: Real Sensor Mode Enable [Day 3]
**Priority:** P0
**Dependencies:** S1, S2

**Objective:** Switch from HITL to real sensor mode

**Modify `rc.board_defaults`:**
```bash
# Change from HITL:
param set-default SYS_HAS_MAG 1    # Was 0
param set-default SYS_HAS_BARO 1   # Was 0
param set-default SYS_AUTOSTART 4001
```

**Validation:**
- All sensor topics publishing real data
- EKF2 status shows sensors detected
- No "sensor timeout" errors

---

#### Task S4: ICM45686 Configuration (If Using) [Day 4]
**Priority:** P1
**Dependencies:** Bhanu's GPIO doc (B4)

**Objective:** Add second IMU to SPI bus

**Files to Modify:**
- `board_config.h` - Add GPIO definitions
- `spi.cpp` - Add device to bus
- `rc.board_sensors` - Start driver

---

#### Task S5: Sensor Data Quality Validation [Day 5-7]
**Priority:** P0
**Dependencies:** S3

**Validation Tests:**
1. **Noise Analysis:**
   - Record 1 minute of sensor data
   - Analyze with Python/MATLAB
   - Compare with FMU-v6x baseline

2. **Vibration Test:**
   - Mount on vibration table
   - Measure sensor response
   - Check for aliasing

3. **Temperature Stability:**
   - Heat/cool board
   - Monitor sensor drift

**Deliverable:** Sensor quality report comparing to Pixhawk baseline

---

### Week 2 Tasks (Bhanu Traveling)

#### Task S6: Sensor Calibration [Day 8-10]
**Priority:** P0
**Dependencies:** S5

**Calibration Sequence:**
```bash
commander calibrate gyro       # Keep still
commander calibrate accel      # 6-position
commander calibrate mag        # Rotate all axes
commander calibrate level      # Level surface
```

**Record All Calibration Parameters:**
```bash
param show CAL_* > calibration_params.txt
```

---

#### Task S7: EKF2 Configuration & Tuning [Day 10-12]
**Priority:** P0
**Dependencies:** S6

**Key Parameters:**
```bash
# IMU selection
param set EKF2_IMU_ID <device_id>

# Fusion settings
param set EKF2_AID_MASK <value>
param set EKF2_HGT_REF 1          # Baro for height

# Noise parameters (tune based on S5 results)
param set EKF2_ACC_NOISE <value>
param set EKF2_GYR_NOISE <value>
param set EKF2_BARO_NOISE <value>
param set EKF2_MAG_NOISE <value>
```

**Validation:**
- `ekf2 status` shows healthy
- `listener estimator_status` no warnings
- Position hold test (if motors ready)

---

#### Task S8: Multi-Sensor Fusion Validation [Day 12-14]
**Priority:** P1
**Dependencies:** S7

**Test Cases:**
1. Single IMU operation
2. Dual IMU operation (if ICM45686 added)
3. Baro failover (DPS310 ↔ BMP388)
4. Mag failover (AK09916 ↔ BMM150)

**Deliverable:** Sensor redundancy test report

---

### Week 3 Tasks

#### Task S9: Integration with Motor System [Day 15-17]
**Priority:** P0
**Dependencies:** Vivek's motor tests

**Objective:** Verify sensor→EKF→controller→motor loop

**Tests:**
1. Attitude response to sensor input
2. Rate loop performance
3. Control latency measurement

---

#### Task S10: Pre-Flight Sensor Validation [Day 18-21]
**Priority:** P0
**Dependencies:** All sensor tasks complete

**Create:** Pre-flight sensor checklist
**Validate:** All sensors within specification
**Document:** Final sensor configuration

---

## VIVEK - HITL & Hardware Validation

### Availability: Ongoing
### Hardware: SAMV71, oscilloscope (NO Click boards)

**Focus:** HITL testing, hardware validation, scope measurements

### Week 1 Tasks

#### Task V1: HITL Sensor Processing Fix [Day 1-3]
**Priority:** P0 - CRITICAL
**Dependencies:** None

**Issue:** `sensor_baro` exists but `vehicle_air_data` never published

**Investigation:**
1. Read: `HITL_SENSOR_PROCESSING_ISSUE.md`
2. Trace sensor data flow in `src/modules/sensors/`
3. Check `vehicle_air_data/` processing
4. Debug why simulation baro not reaching EKF

**Files to Investigate:**
```
src/modules/sensors/sensors.cpp
src/modules/sensors/vehicle_air_data/
src/modules/ekf2/
```

**Commands for Debug:**
```bash
listener sensor_baro           # Raw baro data
listener vehicle_air_data      # Processed data (should not be "never published")
ekf2 status                    # Check baro_device_id
sensors status                 # Check baro processing
```

**Deliverable:** Fix or detailed root cause analysis

---

#### Task V2: HITL Full Validation [Day 3-5]
**Priority:** P0
**Dependencies:** V1

**Objective:** Complete HITL flight simulation

**Test Sequence:**
1. Start jMAVSim on PC
2. Connect to SAMV71 via USB
3. Arm and attempt hover
4. Verify no failsafe triggers
5. Complete simulated flight

**Validation Criteria:**
- Motors show func: 101-104 (already fixed)
- EKF gets position estimate
- Commander allows arming
- Stable simulated hover

---

#### Task V3: ADC Validation with Scope [Day 5-6]
**Priority:** P0
**Dependencies:** Bhanu completes B1

**Objective:** Hardware validate ADC implementation

**Scope Measurements:**
1. Measure actual voltage on ADC pin
2. Compare with `adc test` reading
3. Calculate and verify scaling factor
4. Check for noise/ripple

**Deliverable:** ADC validation report with scope captures

---

#### Task V4: PWM Validation with Scope [Day 6-7]
**Priority:** P0
**Dependencies:** Bhanu completes B2

**Objective:** Validate all 4 PWM channels

**Scope Measurements for Each Channel:**
1. PWM frequency (50Hz → 400Hz test)
2. Duty cycle accuracy (1000µs → 2000µs)
3. Rise/fall time
4. Channel-to-channel timing

**Test Commands:**
```bash
pwm_out status
actuator_test set -m 1 -v 0.1
actuator_test set -m 1 -v 0.5
actuator_test set -m 1 -v 0.9
```

**Deliverable:** PWM validation report with scope captures

---

### Week 2 Tasks

#### Task V5: Watchdog Validation [Day 8]
**Priority:** P0
**Dependencies:** Bhanu completes B3

**Test Cases:**
1. Normal operation - verify no reset
2. Intentional hang - verify reset triggers
3. Measure actual timeout

---

#### Task V6: Communication Timing Analysis [Day 9-10]
**Priority:** P1

**Scope Measurements:**
1. MAVLink message timing on USB
2. I2C transaction timing
3. SPI transaction timing
4. Control loop timing

---

#### Task V7: ESC/Motor Testing (with Holybro 500) [Day 11-14]
**Priority:** P0
**Dependencies:** V4 complete, Holybro 500 available

**Safety:** PROPS OFF!

**Test Sequence:**
1. Connect SAMV71 PWM to Holybro ESCs
2. Verify ESC arm/disarm
3. Test motor spin direction
4. Verify motor mapping (1-4)
5. Test throttle response

**Validation:**
- All 4 motors spin correct direction
- Motor mapping matches PX4 convention
- No oscillation or instability

---

### Week 3 Tasks

#### Task V8: Integration Testing [Day 15-18]
**Priority:** P0

**Full System Tests:**
1. Boot → sensor init → EKF → control → motor
2. RC input → control response
3. Failsafe trigger → motor shutdown
4. Arming sequence validation

---

#### Task V9: Tethered Test Preparation [Day 19-21]
**Priority:** P0
**Dependencies:** All previous tasks

**Pre-Tethered Checklist:**
- All sensors calibrated
- Motors tested
- Failsafes configured
- Kill switch verified
- Tether attached
- Safety perimeter established

---

## VIGNESH - Environment Setup & Learning

### Availability: Starting (Getting Setup)

**Focus:** Get development environment ready, learn codebase

### Week 1 Tasks

#### Task G1: Development Environment Setup [Day 1-3]
**Priority:** P0

**Setup Steps:**
1. Clone repository
2. Install toolchain (arm-none-eabi-gcc)
3. Install dependencies (NuttX, Python)
4. Build firmware successfully
5. Flash to SAMV71 board

**Validation:** Successful build and flash

---

#### Task G2: Documentation Review [Day 3-5]
**Priority:** P1

**Read and Understand:**
1. `README.md` - Project overview
2. `PRODUCTION_READINESS.md` - Gap analysis
3. `TEAM_DEVELOPMENT_PLAN_V2.md` - This document
4. `UPDATESUBSCRIPTIONS_DEBUG.md` - Recent fix
5. PX4 Developer Guide (online)

---

#### Task G3: Basic PX4 Commands [Day 5-7]
**Priority:** P1

**Learn Commands:**
```bash
# Status commands
top
dmesg
sensors status
ekf2 status
pwm_out status

# Listener commands
listener sensor_accel
listener vehicle_attitude

# Parameter commands
param show *
param set <name> <value>
param save
```

---

### Week 2+ Tasks

#### Task G4: Join Development [Day 8+]
**Priority:** P1

**Possible Assignments:**
- Documentation updates
- Test script creation
- Bug verification
- Code review assistance

---

## AJIT & TEAM - Hardware Integration

### Availability: On-call (starts when tethered test ready)

**Focus:** Mechanical integration, custom FC design

### Pre-Tethered Phase (Current)

#### Task A1: Review GPIO Pinout [When B4 Complete]
**Priority:** P1

**Review:** GPIO_PINOUT.md from Bhanu
**Plan:** Custom FC connector design

---

#### Task A2: Holybro 500 Integration Planning
**Priority:** P1

**Plan:**
1. SAMV71 mounting position
2. Click board sensor placement
3. Wiring harness design
4. Vibration isolation

---

### Post-Tethered Phase

#### Task A3: Click Board Mounting
**Priority:** P0

**Work:**
1. Design sensor mount for Click boards
2. Vibration isolation for IMU
3. Secure wiring

---

#### Task A4: Custom FC PCB Design (Parallel Track)
**Priority:** P1

**Pixhawk-Compatible FC Design:**
1. SAMV71 as MCU
2. Integrated sensors (no Click boards)
3. Standard Pixhawk connector pinout
4. Production-ready layout

---

## US TEAM - Parallel Validation

### Availability: Later (setting up)

**Focus:** Mirror India setup, parallel validation

### Current Phase

#### Task US1: Setup Replication
**Priority:** P1

**Replicate:**
1. Same hardware (SAMV71-XULT, Click boards)
2. Same toolchain version
3. Same firmware build
4. Same test procedures

---

### Future Phase

#### Task US2: Parallel Testing
**Priority:** P1

**When Ready:**
1. Run same test suite as India
2. Report any differences
3. Validate fixes independently

---

## Holybro 500 Reference & Benchmarking

### Purpose
- Reference flight controller for comparison
- Known-good baseline for sensor/motor behavior
- Target frame for SAMV71 integration

### Comparison Tests

| Test | Holybro FC | SAMV71 | Pass Criteria |
|------|------------|--------|---------------|
| Sensor noise floor | Measure | Measure | Within 2x |
| PWM accuracy | Measure | Measure | Within 1% |
| Control loop latency | Measure | Measure | Within 2x |
| Power consumption | Measure | Measure | Document |
| HITL responsiveness | Test | Test | Equivalent |

---

## Platform Roadmap: SAMV71 → PIC32CZ70 → PIC32CZ90

### Phase 1: SAMV71 (Current - Alpha)
**Goal:** Prove PX4 on Microchip silicon
**Target:** Tethered test → Basic flight

### Phase 2: PIC32CZ CA70 (Next)
**Goal:** Cost-optimized production
**Benefits:**
- 60% cost reduction
- Pin-compatible migration path
- Production volumes

### Phase 3: PIC32CZ CA80/CA90 (Future)
**Goal:** Next-gen flagship
**Benefits:**
- 1MB RAM + TCM
- 8MB Flash
- Gigabit Ethernet
- HSM (CA90) for security

### Migration Path
```
SAMV71 Code Base
      │
      ├──► PIC32CZ CA70 (minimal changes)
      │         │
      │         └──► PIC32CZ CA80 (peripheral updates)
      │                   │
      │                   └──► PIC32CZ CA90 (+ HSM security)
      │
      └──► Custom FC (Ajit) based on learnings
```

---

## Weekly Milestones

### Week 1 (Dec 7-14) - Core Development
| Day | Bhanu | Syed | Vivek | Vignesh |
|-----|-------|------|-------|---------|
| 1 | ADC design | I2C scan | HITL issue | Setup |
| 2 | ADC impl | SPI test | HITL debug | Setup |
| 3 | PWM4 | Real sensor | HITL fix | Docs |
| 4 | Watchdog | ICM45686 | ADC scope | Docs |
| 5 | MTD | Data quality | PWM scope | Commands |
| 6 | Handoff | Calibration | Motor test | Learn |
| 7 | Travel | Integration | Integration | Practice |

### Week 2 (Dec 14-21) - Bhanu Traveling
| Focus | Syed | Vivek | Vignesh |
|-------|------|-------|---------|
| Day 8-10 | Calibration | Watchdog test | Join dev |
| Day 11-14 | EKF tuning | ESC/Motor test | Assist |

### Week 3 (Dec 21-28) - Integration
| Focus | All Team |
|-------|----------|
| Day 15-17 | System integration |
| Day 18-21 | Pre-tethered validation |

### Week 4+ - Flight Testing
| Phase | Activity |
|-------|----------|
| Tethered | Low hover, stability check |
| Untethered | Basic flight, position hold |
| Advanced | Autonomous, stress test |

---

## Communication Plan

### Daily Standups
- **Time:** 10:00 AM IST
- **Duration:** 15 minutes
- **Format:** What done, what next, blockers

### Code Integration
- **Branch:** `samv7-custom`
- **Review:** Bhanu reviews (async when traveling)
- **Merge:** After review + build pass

### Documentation
- All findings in `boards/microchip/samv71-xult-clickboards/`
- Update status in `FEATURE_STATUS.md`
- Scope captures in `validation/` folder

### US Team Sync
- **Frequency:** Weekly
- **Day:** Friday evening IST / Friday morning US
- **Focus:** Progress sync, issue sharing

---

## Success Criteria

### Phase 1 Complete (Week 3)
- [ ] ADC working - battery voltage reads correctly
- [ ] 4 PWM channels working
- [ ] Watchdog implemented
- [ ] All sensors calibrated
- [ ] HITL flies successfully
- [ ] Motors tested (props off)

### Phase 2 Complete (Week 4)
- [ ] Tethered hover achieved
- [ ] No failsafe triggers
- [ ] Stable attitude hold
- [ ] Ajit begins hardware integration

### Phase 3 Complete (Week 6+)
- [ ] Untethered flight
- [ ] Position hold working
- [ ] RTL tested
- [ ] Ready for custom FC design

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| 4th PWM pin conflict | Medium | High | Check SAMV71 datasheet early |
| ADC noise too high | Medium | Medium | Add filtering, compare with Holybro |
| HITL issue not fixable | Low | High | Prioritize real sensor testing |
| SD card hang persists | Medium | Medium | Keep logging short, test often |
| Bhanu blocked during travel | Medium | High | Complete handoff docs |
| US team setup delayed | High | Low | India proceeds independently |

---

## Document History

| Date | Version | Update |
|------|---------|--------|
| 2025-12-06 | V1 | Initial 4-developer plan |
| 2025-12-07 | V2 | Refined with specific team roles, platform comparison |

---

**Last Updated:** 2025-12-07
**Author:** Claude Code (comprehensive analysis)

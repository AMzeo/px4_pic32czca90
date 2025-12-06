# SAMV71-XULT-ClickBoards Feature Status

**Last Updated:** December 6, 2025
**Flash Usage:** 63.69% (1,335,772 bytes of 2MB)
**HITL Status:** VERIFIED WORKING

---

## VERIFIED WORKING (Tested December 6, 2025)

### HITL (Hardware-In-The-Loop) Simulation

| Component | Status | Details |
|-----------|--------|---------|
| jMAVSim Connection | ✓ Working | Via USB MAVLink (/dev/ttyACM0) |
| HIL Sensor Data | ✓ Working | sensor_accel, sensor_gyro receiving simulation data |
| Commander Arming | ✓ Working | `commander arm -f` succeeds |
| Flight Logging | ✓ Working | Logs written to SD card (464KB test file) |
| Parameter Storage | ✓ Working | Params saved/loaded from /fs/microsd/params |

**Test Results:**
```
nsh> listener sensor_accel
device_id: 1310988 (Type: 0x14, SIMULATION:1 (0x01))
x: -0.04834, y: 0.01916, z: -9.85557  (gravity correct!)

nsh> commander arm -f
INFO [commander] Armed by internal command
INFO [logger] Opened full log file: /fs/microsd/log/2025-12-06/11_19_59.ulg
```

### Core Systems Verified

| System | Status | Test Method |
|--------|--------|-------------|
| SD Card Read/Write | ✓ Working | param save/load, flight logs |
| USB CDC/ACM | ✓ Working | Debug console + MAVLink |
| MAVLink Protocol | ✓ Working | jMAVSim bidirectional comm |
| uORB Messaging | ✓ Working | listener command shows topics |
| Parameter System | ✓ Working | 279 bytes imported at boot |
| Logger Module | ✓ Working | No stack warnings after fix |

---

## ENABLED FEATURES

### Core Flight Modules

| Module | Config | Status | Notes |
|--------|--------|--------|-------|
| Commander | `CONFIG_MODULES_COMMANDER=y` | ✓ Enabled | Flight state management |
| EKF2 | `CONFIG_MODULES_EKF2=y` | ✓ Enabled | State estimation |
| Control Allocator | `CONFIG_MODULES_CONTROL_ALLOCATOR=y` | ✓ Enabled | Motor mixing |
| Navigator | `CONFIG_MODULES_NAVIGATOR=y` | ✓ Enabled | Mission execution |
| Flight Mode Manager | `CONFIG_MODULES_FLIGHT_MODE_MANAGER=y` | ✓ Enabled | Mode transitions |
| Land Detector | `CONFIG_MODULES_LAND_DETECTOR=y` | ✓ Enabled | Landing detection |
| Logger | `CONFIG_MODULES_LOGGER=y` | ✓ Enabled | Flight logging |
| MAVLink | `CONFIG_MODULES_MAVLINK=y` | ✓ Enabled | GCS communication |
| Sensors | `CONFIG_MODULES_SENSORS=y` | ✓ Enabled | Sensor management |
| Battery Status | `CONFIG_MODULES_BATTERY_STATUS=y` | ✓ Enabled | Battery monitoring |
| Dataman | `CONFIG_MODULES_DATAMAN=y` | ✓ Enabled | Persistent storage |
| Events | `CONFIG_MODULES_EVENTS=y` | ✓ Enabled | Event handling |
| Load Monitor | `CONFIG_MODULES_LOAD_MON=y` | ✓ Enabled | CPU monitoring |
| Manual Control | `CONFIG_MODULES_MANUAL_CONTROL=y` | ✓ Enabled | RC input handling |
| RC Update | `CONFIG_MODULES_RC_UPDATE=y` | ✓ Enabled | RC processing |

### Multicopter Control

| Module | Config | Status | Notes |
|--------|--------|--------|-------|
| MC Attitude Control | `CONFIG_MODULES_MC_ATT_CONTROL=y` | ✓ Enabled | Attitude controller |
| MC Position Control | `CONFIG_MODULES_MC_POS_CONTROL=y` | ✓ Enabled | Position controller |
| MC Rate Control | `CONFIG_MODULES_MC_RATE_CONTROL=y` | ✓ Enabled | Rate controller |
| MC Hover Thrust Est | `CONFIG_MODULES_MC_HOVER_THRUST_ESTIMATOR=y` | ✓ Enabled | Hover performance |
| MC Autotune | `CONFIG_MODULES_MC_AUTOTUNE_ATTITUDE_CONTROL=y` | ✓ Enabled | Auto-tuning |

### Calibration & Estimation

| Module | Config | Status | Notes |
|--------|--------|--------|-------|
| Gyro Calibration | `CONFIG_MODULES_GYRO_CALIBRATION=y` | ✓ Enabled | Gyro cal |
| Mag Bias Estimator | `CONFIG_MODULES_MAG_BIAS_ESTIMATOR=y` | ✓ Enabled | Mag cal |

### EKF2 Advanced Features (Double-Precision FPU)

| Feature | Config | Status | Notes |
|---------|--------|--------|-------|
| Aux Global Position | `CONFIG_EKF2_AUX_GLOBAL_POSITION=y` | ✓ Enabled | |
| Aux Velocity | `CONFIG_EKF2_AUXVEL=y` | ✓ Enabled | |
| Drag Fusion | `CONFIG_EKF2_DRAG_FUSION=y` | ✓ Enabled | |
| External Vision | `CONFIG_EKF2_EXTERNAL_VISION=y` | ✓ Enabled | |
| GNSS Yaw | `CONFIG_EKF2_GNSS_YAW=y` | ✓ Enabled | |
| Range Finder | `CONFIG_EKF2_RANGE_FINDER=y` | ✓ Enabled | |

### Sensor Drivers

| Driver | Config | Status | Click Board |
|--------|--------|--------|-------------|
| ICM-20689 (SPI) | `CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y` | ✓ Enabled | MIKROE-4044 |
| ICM-45686 (SPI) | `CONFIG_DRIVERS_IMU_INVENSENSE_ICM45686=y` | ✓ Enabled | MIKROE-6514 |
| AK09916 (I2C) | `CONFIG_DRIVERS_MAGNETOMETER_AKM_AK09916=y` | ✓ Enabled | MIKROE-4231 |
| BMM150 (I2C) | `CONFIG_DRIVERS_MAGNETOMETER_BOSCH_BMM150=y` | ✓ Enabled | MIKROE-2935 |
| DPS310 (I2C) | `CONFIG_DRIVERS_BAROMETER_DPS310=y` | ✓ Enabled | MIKROE-2293 |
| BMP388 (I2C) | `CONFIG_DRIVERS_BAROMETER_BMP388=y` | ✓ Enabled | MIKROE-3566 |
| BMI088 I2C | `CONFIG_DRIVERS_IMU_BOSCH_BMI088_I2C=y` | ✓ Enabled | MIKROE-3775 |
| GPS | `CONFIG_DRIVERS_GPS=y` | ✓ Enabled | Any NMEA/UBX |
| RC Input | `CONFIG_DRIVERS_RC_INPUT=y` | ✓ Enabled | PPM/SBUS |

### System Commands

| Command | Config | Status | Usage |
|---------|--------|--------|-------|
| param | `CONFIG_SYSTEMCMDS_PARAM=y` | ✓ Enabled | Parameter management |
| top | `CONFIG_SYSTEMCMDS_TOP=y` | ✓ Enabled | Process monitor |
| ver | `CONFIG_SYSTEMCMDS_VER=y` | ✓ Enabled | Version info |
| dmesg | `CONFIG_SYSTEMCMDS_DMESG=y` | ✓ Enabled | Kernel messages |
| tests | `CONFIG_SYSTEMCMDS_TESTS=y` | ✓ Enabled | Test suite |
| nshterm | `CONFIG_SYSTEMCMDS_NSHTERM=y` | ✓ Enabled | NSH terminal |
| tune_control | `CONFIG_SYSTEMCMDS_TUNE_CONTROL=y` | ✓ Enabled | Buzzer tunes |
| bsondump | `CONFIG_SYSTEMCMDS_BSONDUMP=y` | ✓ Enabled | BSON debug |
| littlefs_mount | `CONFIG_SYSTEMCMDS_LITTLEFS_MOUNT=y` | ✓ Enabled | LittleFS |
| mft | `CONFIG_SYSTEMCMDS_MFT=y` | ✓ Enabled | Manufacturing test |
| **listener** | `CONFIG_SYSTEMCMDS_TOPIC_LISTENER=y` | ✓ **NEW** | uORB debug |
| **perf** | `CONFIG_SYSTEMCMDS_PERF=y` | ✓ **NEW** | Performance |
| **uorb** | `CONFIG_SYSTEMCMDS_UORB=y` | ✓ **NEW** | uORB status |
| **actuator_test** | `CONFIG_SYSTEMCMDS_ACTUATOR_TEST=y` | ✓ **NEW** | Motor test |
| **reboot** | `CONFIG_SYSTEMCMDS_REBOOT=y` | ✓ **NEW** | System reboot |
| **i2cdetect** | `CONFIG_SYSTEMCMDS_I2CDETECT=y` | ✓ **NEW** | I2C scan |
| **led_control** | `CONFIG_SYSTEMCMDS_LED_CONTROL=y` | ✓ **NEW** | LED control |
| **work_queue** | `CONFIG_SYSTEMCMDS_WORK_QUEUE=y` | ✓ **NEW** | WQ debug |
| **system_time** | `CONFIG_SYSTEMCMDS_SYSTEM_TIME=y` | ✓ **NEW** | Time sync |
| **mtd** | `CONFIG_SYSTEMCMDS_MTD=y` | ✓ **NEW** | Flash mgmt |

### Simulation

| Feature | Config | Status | Notes |
|---------|--------|--------|-------|
| PWM Out Sim | `CONFIG_MODULES_SIMULATION_PWM_OUT_SIM=y` | ✓ Enabled | HITL support |

### PWM Output

| Feature | Config | Status | Notes |
|---------|--------|--------|-------|
| PWM Out | `CONFIG_DRIVERS_PWM_OUT=y` | ✓ Enabled | 3 channels (TC-based) |

### Other

| Feature | Config | Status | Notes |
|---------|--------|--------|-------|
| CDC/ACM Autostart | `CONFIG_DRIVERS_CDCACM_AUTOSTART=y` | ✓ Enabled | USB serial |
| Auto Follow Target | `CONFIG_MODULES_FLIGHT_MODE_MANAGER_TASKS_AUTO_FOLLOW_TARGET=y` | ✓ Enabled | Follow mode |
| Logger Stack | `CONFIG_LOGGER_STACK_SIZE=4500` | ✓ **FIXED** | Was 3700, increased |
| HP WQ Stack | `CONFIG_WQ_HP_DEFAULT_STACKSIZE=4096` | ✓ Tuned | Work queue stack |

---

## NOT ENABLED (Potential Future Additions)

### Medium Priority - Should Consider

| Feature | Config | Reason Not Enabled | Priority |
|---------|--------|-------------------|----------|
| DShot | `CONFIG_DRIVERS_DSHOT` | Needs PWMC implementation | MEDIUM |
| Safety Button | `CONFIG_DRIVERS_SAFETY_BUTTON` | Hardware dependent | MEDIUM |
| Tone Alarm | `CONFIG_DRIVERS_TONE_ALARM` | Needs buzzer hardware | MEDIUM |
| PWM Input | `CONFIG_DRIVERS_PWM_INPUT` | RC failsafe redundancy | MEDIUM |
| Temperature Compensation | `CONFIG_MODULES_TEMPERATURE_COMPENSATION` | Sensor accuracy | MEDIUM |
| Hardfault Stream | `CONFIG_MODULES_HARDFAULT_STREAM` | Crash diagnostics | MEDIUM |

### Low Priority - Optional

| Feature | Config | Reason Not Enabled |
|---------|--------|-------------------|
| GPIO Command | `CONFIG_SYSTEMCMDS_GPIO` | STM32-specific code |
| Hardfault Log | `CONFIG_SYSTEMCMDS_HARDFAULT_LOG` | Needs PROGMEM infrastructure |
| Network Manager | `CONFIG_SYSTEMCMDS_NETMAN` | No Ethernet on board |
| I2C Launcher | `CONFIG_SYSTEMCMDS_I2C_LAUNCHER` | Not needed |
| UAVCAN | `CONFIG_DRIVERS_UAVCAN` | No CAN hardware |
| Camera Trigger | `CONFIG_DRIVERS_CAMERA_TRIGGER` | No camera support |
| Gimbal | `CONFIG_MODULES_GIMBAL` | No gimbal hardware |
| Board ADC | `CONFIG_DRIVERS_ADC_BOARD_ADC` | Not implemented for SAMV7 |

### Not Applicable - Fixed Wing / VTOL

| Feature | Config | Reason |
|---------|--------|--------|
| FW Attitude Control | `CONFIG_MODULES_FW_ATT_CONTROL` | Multicopter only |
| FW Rate Control | `CONFIG_MODULES_FW_RATE_CONTROL` | Multicopter only |
| FW Mode Manager | `CONFIG_MODULES_FW_MODE_MANAGER` | Multicopter only |
| VTOL Control | `CONFIG_MODULES_VTOL_ATT_CONTROL` | Multicopter only |
| Airspeed Selector | `CONFIG_MODULES_AIRSPEED_SELECTOR` | Multicopter only |
| EKF2 Sideslip | `CONFIG_EKF2_SIDESLIP` | Fixed-wing only |

---

## FIXES APPLIED

| Issue | Fix | Date |
|-------|-----|------|
| Logger low on stack (292 bytes) | Increased `CONFIG_LOGGER_STACK_SIZE` from 3700 to 4500 | 2025-12-06 |
| SYS_AUTOSTART override on boot | Changed `param set` to `param set-default` in rc.board_defaults | 2025-12-06 |
| BMI088 driver syntax | Added `-A -G -X` flags in rc.board_sensors | 2025-12-06 |
| PA26 SD card conflict | Removed from PWM (FIX #46) | 2025-12-06 |

---

## SERIAL PORT MAPPING

| Port | Device | Usage |
|------|--------|-------|
| GPS1 | /dev/ttyS2 | GPS |
| TEL1 | /dev/ttyACM0 | MAVLink (USB) |
| TEL2 | /dev/ttyS1 | Telemetry |
| RC | /dev/ttyS4 | RC Input |

---

## MEMORY USAGE

| Region | Used | Total | Percentage |
|--------|------|-------|------------|
| Flash | 1,335,772 B | 2 MB | 63.69% |
| SRAM | 52,444 B | 320 KB | 16.00% |
| NoCache | 5 KB | 64 KB | 7.81% |

---

## NEXT STEPS

1. **PWMC Implementation** - Enable 4-channel PWM using dedicated PWMC peripheral
2. **DShot Support** - Digital ESC protocol for better motor control
3. **ADC Implementation** - Battery voltage/current sensing
4. **Temperature Compensation** - Improve sensor accuracy

---

## DOCUMENT HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-06 | Initial creation with full feature audit |
| 1.1 | 2025-12-06 | Added HITL verification results - jMAVSim working! |

# SAMV7 HITL Sensor Processing Issue

**Issue:** HITL simulation sensor data not being processed into EKF-consumable topics
**Status:** OPEN - Investigation required
**Priority:** High - Blocks HITL flight testing
**Related Fix:** updateSubscriptions() fix documented in UPDATESUBSCRIPTIONS_DEBUG.md

---

## Problem Summary

When running HITL (Hardware-In-The-Loop) simulation with jMAVSim on SAMV71:
- Simulation sensor data arrives correctly at raw sensor topics (`sensor_baro`, `sensor_accel`, `sensor_gyro`)
- Motor functions are correctly assigned (func: 101-104) after updateSubscriptions fix
- BUT `vehicle_air_data` is "never published"
- EKF shows `baro_device_id: 0` - no barometer seen
- EKF cannot compute position estimate (`local position: 0`)
- Commander triggers failsafe due to missing position estimate
- Drone auto-disarms after arming

---

## Symptoms

### 1. sensor_baro has valid simulation data
```
nsh> listener sensor_baro
TOPIC: sensor_baro
 timestamp: 12345678
 device_id: SIMULATION:1
 pressure: 101325.0
 temperature: 25.0
```

### 2. vehicle_air_data never published
```
nsh> listener vehicle_air_data
WARN  [listener] never published
```

### 3. EKF status shows no baro
```
nsh> ekf2 status
...
baro_device_id: 0
local position: 0
...
```

### 4. sensors module status shows no baro processing
```
nsh> sensors status
...
baro: (no entries or not processing simulation data)
...
```

### 5. Failsafe loop during HITL
```
Commander: armed
... (brief moment)
Commander: failsafe triggered - position estimate invalid
Commander: disarmed
```

---

## Expected Behavior

1. `sensor_baro` data from simulation should be processed by sensors module
2. sensors module should publish `vehicle_air_data`
3. EKF2 should see `baro_device_id: SIMULATION:1`
4. EKF2 should compute valid position estimate
5. HITL flight should work without failsafe

---

## Root Cause Hypothesis

The sensors module may not be configured to process simulation sensor data on SAMV7. Possible causes:

1. **Sensor voter not recognizing simulation devices**: The sensor_baro topic exists but the sensors module voter may not be subscribed to it or may be filtering out simulation device IDs.

2. **Missing sensor_selection configuration**: The system may need explicit configuration to use simulation sensors.

3. **Different sensor pipeline for HITL**: On SITL, there's a dedicated path for simulation sensors. On hardware HITL, this path may not be active.

4. **Priority/device ID mismatch**: Simulation sensors may have device IDs that don't match expected hardware sensor IDs.

5. **Module startup order**: The sensors module may start before simulation data is available and not re-subscribe.

---

## Investigation Plan

### Phase 1: Understand sensor data flow
1. Trace how `sensor_baro` gets to `vehicle_air_data` on working platforms (SITL)
2. Check sensors module code for simulation sensor handling
3. Verify sensor_selection topic configuration

### Phase 2: Check SAMV7-specific configuration
1. Check if HITL-specific modules are started in rcS/defaults
2. Verify SYS_HITL parameter effects on sensor processing
3. Check board-specific sensor configuration

### Phase 3: Compare with working HITL implementations
1. Compare with Pixhawk HITL setup
2. Identify any missing modules or configuration

---

## Files to Investigate

| File | Purpose |
|------|---------|
| `src/modules/sensors/sensors.cpp` | Main sensor processing module |
| `src/modules/sensors/vehicle_air_data/` | Baro to vehicle_air_data processing |
| `src/modules/ekf2/` | EKF sensor subscription |
| `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` | Board startup config |
| `ROMFS/px4fmu_common/init.d/rcS` | Main startup script |
| `src/modules/simulation/sensor_baro_sim/` | Simulation baro source |

---

## Related Parameters

```bash
# HITL mode parameters
param show SYS_HITL
param show SYS_AUTOSTART

# Sensor selection
param show CAL_BARO*
param show EKF2_BARO*
param show SENS_BOARD_*
```

---

## Commands for Debugging

```bash
# Check simulation sensor topics
listener sensor_baro
listener sensor_accel
listener sensor_gyro
listener sensor_mag

# Check processed sensor topics
listener vehicle_air_data
listener vehicle_attitude
listener vehicle_local_position

# Check EKF status
ekf2 status

# Check sensors module
sensors status

# Check what modules are running
top
```

---

## Workaround (None Yet)

No workaround currently available. HITL requires proper sensor processing.

---

## Dependencies

This issue is separate from but related to:
- **updateSubscriptions() fix** (SOLVED) - Motor functions now work
- **SPI sensor drivers** - Real sensors may interfere with simulation sensors

---

## Document History

| Date | Update |
|------|--------|
| 2025-12-06 | Initial issue creation during HITL testing |

---

**Last Updated:** 2025-12-06
**Author:** Claude Code (debugging session)

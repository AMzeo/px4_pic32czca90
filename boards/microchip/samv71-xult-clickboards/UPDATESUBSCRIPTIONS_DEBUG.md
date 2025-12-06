# SAMV7 updateSubscriptions() Debug & Fix Log

**Issue:** `pwm_out_sim` HITL mode not working - motors show `func: 0` instead of `func: 101-104`
**Root Cause:** Static initialization of `all_function_providers[]` array fails on SAMV7 - array reads as all zeros
**Status:** ✅ FIXED - Runtime initialization implemented and verified

---

## Problem Summary

When running HITL (Hardware-In-The-Loop) simulation with jMAVSim on SAMV71:
- Sensor data flows correctly (sensor_accel, sensor_gyro show SIMULATION:1)
- Commander arming works with `-f` flag
- BUT `pwm_out_sim status` shows `func: 0` for all channels instead of `func: 101-104`
- Takeoff fails, failsafe activates, auto-disarms

**Root cause chain:**
1. `updateSubscriptions()` was skipped on SAMV7 (workaround in PWMSim.cpp)
2. When re-enabled, system crashes/reboots
3. Crash caused by `all_function_providers[]` array reading ALL ZEROS
4. Static initialization of complex structs with function pointers fails on SAMV7

---

## Solution

**Runtime initialization** of the `all_function_providers[]` array instead of static initialization.

### Key Changes:

1. **`src/lib/mixer_module/mixer_module.cpp`:**
   - Added default constructor to `FunctionProvider` struct
   - Changed array from static-initialized to runtime-initialized
   - Added `init_function_providers()` function called at start of `updateSubscriptions()`

2. **`src/modules/simulation/pwm_out_sim/PWMSim.cpp`:**
   - Removed all SAMV7 workarounds
   - `updateSubscriptions(true)` now called unconditionally

3. **`src/modules/simulation/pwm_out_sim/PWMSim.hpp`:**
   - Changed `SAMV7_PWMSIM_TEST_MODE` from 5 to 0 (full mode)

---

## Files Modified

| File | Change |
|------|--------|
| `src/lib/mixer_module/mixer_module.cpp` | Runtime init of all_function_providers[] |
| `src/modules/simulation/pwm_out_sim/PWMSim.cpp` | Removed SAMV7 workarounds |
| `src/modules/simulation/pwm_out_sim/PWMSim.hpp` | Mode 0 for all platforms |

---

## Implementation Details

### Before (Static Initialization - BROKEN on SAMV7):
```cpp
static const FunctionProvider all_function_providers[] = {
    {OutputFunction::Constant_Min, &FunctionConstantMin::allocate},
    {OutputFunction::Constant_Max, &FunctionConstantMax::allocate},
    {OutputFunction::Motor1, OutputFunction::MotorMax, &FunctionMotors::allocate},
    // ...
};
```

### After (Runtime Initialization - WORKS):
```cpp
// Array declared without initializer (BSS, zeroed at startup)
static FunctionProvider all_function_providers[12];
static bool providers_initialized = false;

// Runtime initialization function
static void init_function_providers()
{
    if (providers_initialized) {
        return;
    }

    all_function_providers[0] = FunctionProvider(OutputFunction::Constant_Min, &FunctionConstantMin::allocate);
    all_function_providers[1] = FunctionProvider(OutputFunction::Constant_Max, &FunctionConstantMax::allocate);
    all_function_providers[2] = FunctionProvider(OutputFunction::Motor1, OutputFunction::MotorMax, &FunctionMotors::allocate);
    all_function_providers[3] = FunctionProvider(OutputFunction::Servo1, OutputFunction::ServoMax, &FunctionServos::allocate);
    all_function_providers[4] = FunctionProvider(OutputFunction::Peripheral_via_Actuator_Set1, OutputFunction::Peripheral_via_Actuator_Set6, &FunctionActuatorSet::allocate);
    all_function_providers[5] = FunctionProvider(OutputFunction::Landing_Gear, &FunctionLandingGear::allocate);
    all_function_providers[6] = FunctionProvider(OutputFunction::Landing_Gear_Wheel, &FunctionLandingGearWheel::allocate);
    all_function_providers[7] = FunctionProvider(OutputFunction::Parachute, &FunctionParachute::allocate);
    all_function_providers[8] = FunctionProvider(OutputFunction::Gripper, &FunctionGripper::allocate);
    all_function_providers[9] = FunctionProvider(OutputFunction::RC_Roll, OutputFunction::RC_AUXMax, &FunctionManualRC::allocate);
    all_function_providers[10] = FunctionProvider(OutputFunction::Gimbal_Roll, OutputFunction::Gimbal_Yaw, &FunctionGimbal::allocate);
    all_function_providers[11] = FunctionProvider(OutputFunction::IC_Engine_Ignition, OutputFunction::IC_Engine_Starter, &FunctionICEControl::allocate);

    providers_initialized = true;
}

// Called at start of updateSubscriptions()
bool MixingOutput::updateSubscriptions(bool allow_wq_switch)
{
    init_function_providers();  // Ensure array is initialized
    // ... rest of function
}
```

---

## Verified Results

```
nsh> pwm_out_sim start
nsh> pwm_out_sim status
pwm_out_sim: cycle: 0 events, 0us elapsed, 0.00us avg, min 0us max 0us 0.000us rms
INFO  [mixer_module] Param prefix: HIL_ACT
INFO  [mixer_module] Switched to rate_ctrl work queue
Channel Configuration:
Channel 0: func: 101, value: 900, failsafe: 600, disarmed: 900, min: 1000, max: 2000
Channel 1: func: 102, value: 900, failsafe: 600, disarmed: 900, min: 1000, max: 2000
Channel 2: func: 103, value: 900, failsafe: 600, disarmed: 900, min: 1000, max: 2000
Channel 3: func: 104, value: 900, failsafe: 600, disarmed: 900, min: 1000, max: 2000
```

**Key indicators of success:**
- `func: 101-104` instead of `func: 0`
- `Switched to rate_ctrl work queue` - work queue switch works (was crashing before)
- No crash/reboot

---

## Fix History

| Fix # | Approach | Result |
|-------|----------|--------|
| 1 | Remove `const` qualifier | FAILED - still zeros |
| 2 | Add `volatile` qualifier | FAILED - symbol in BSS |
| 3 | `__attribute__((section(".data")))` | FAILED - ELF still zeros |
| 4 | Correct 3-arg struct initializers | FAILED - ELF still zeros |
| 5 | Combined section + initializers + used | FAILED - static init broken |
| 6 | **Runtime initialization** | ✅ SUCCESS |

---

## Why This Affects SAMV7 But Not STM32

The exact root cause is unclear, but potential factors:

1. **Compiler/Linker Behavior:** Static initialization of complex structs with function pointers may be handled differently
2. **C Runtime Initialization:** SAMV7 NuttX startup may not properly copy `.data` section
3. **Memory Layout:** Different flash/SRAM addresses and MPU configuration
4. **D-Cache:** SAMV7 uses `CONFIG_ARMV7M_DCACHE=y` which could affect initialization

The runtime initialization approach works reliably because it explicitly assigns values after the C runtime is fully initialized.

---

## HITL Setup Instructions

To use HITL mode on SAMV7:

```bash
# On the board (nsh console):
param set SYS_AUTOSTART 1001
param save
reboot

# After reboot, verify:
pwm_out_sim status
# Should show func: 101, 102, 103, 104 for channels 0-3
```

---

## Commands for Testing

```bash
# Build
make microchip_samv71-xult-clickboards_default

# Flash
openocd -f interface/cmsis-dap.cfg -c "adapter speed 1000" -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"

# Serial console tests (run on device)
pwm_out_sim status          # Check func values
param show HIL_ACT*         # Verify HIL parameters
listener actuator_motors    # Check motor commands
commander arm -f            # Force arm for testing
```

---

## Related Issues

| Issue | Status | Document |
|-------|--------|----------|
| HITL Sensor Processing | OPEN | [HITL_SENSOR_PROCESSING_ISSUE.md](HITL_SENSOR_PROCESSING_ISSUE.md) |

**Note:** After fixing updateSubscriptions(), HITL testing revealed a new issue where simulation sensor data (`sensor_baro`) is not being processed into EKF-consumable topics (`vehicle_air_data`). This prevents position estimation and causes failsafe during HITL flight attempts.

---

## Document History

| Date | Update |
|------|--------|
| 2025-12-06 | Initial creation - documenting debug journey |
| 2025-12-06 | Fix attempts #1-5 - all failed |
| 2025-12-06 | Fix #6 (runtime initialization) - SUCCESS |
| 2025-12-06 | Removed all debug prints, cleaned up code |
| 2025-12-06 | Final documentation and git commit |
| 2025-12-06 | Added link to HITL sensor processing issue |

---

**Last Updated:** 2025-12-06
**Author:** Claude Code (debugging session)

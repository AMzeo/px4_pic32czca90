# SAMV71-XULT PX4 Flight Controller Port

**Status:** Prototype Working — Production Buildout In Progress | **Last Updated:** 2026-02-08

PX4 Autopilot port for the Microchip SAMV71-XULT evaluation board with Click sensor boards.

---

## Production Plan

> **[PRODUCTION_PLAN.md](PRODUCTION_PLAN.md)** — Full implementation plan covering IO Timer Allocation API, DShot output, Input Capture, Production Hardening, and Custom PCB phases.

---

## Quick Links

| Document | Description |
|----------|-------------|
| **[Production Plan](PRODUCTION_PLAN.md)** | **Active implementation roadmap (Phases 0–7)** |
| [Production Delta vs FMUv6X](PRODUCTION_DELTA_FMUv6X.md) | Gap analysis against FMUv6X reference |
| [Feature Status](FEATURE_STATUS.md) | Current feature implementation status |

---

## Hardware

- **MCU:** ATSAMV71Q21B (Cortex-M7, 300MHz, 2MB Flash, 384KB SRAM)
- **Board:** SAMV71-XULT Evaluation Kit
- **Sensors:** MikroElektronika Click boards via mikroBUS sockets

### Supported Click Board Sensors

| Sensor | Type | Interface | Click Board |
|--------|------|-----------|-------------|
| ICM-20689 | IMU (6-DOF) | SPI | MIKROE-4044 |
| ICM-45686 | IMU (6-DOF) | SPI | MIKROE-6514 |
| AK09916 | Magnetometer | I2C | MIKROE-4231 |
| BMM150 | Magnetometer | I2C | MIKROE-2935 |
| DPS310 | Barometer | I2C | MIKROE-2293 |
| BMP388 | Barometer | I2C | MIKROE-3566 |
| BMI088 | IMU (6-DOF) | I2C | MIKROE-3775 |

---

## Current Capabilities

### Working
- Full PX4 stack boot and operation
- 4-channel PWMC PWM output (50–400Hz, dynamic prescaler)
- IMU, Barometer, Magnetometer sensors
- GPS serial input
- Battery ADC monitoring
- Safety button
- USB CDC/ACM console and MAVLink
- SD card storage (params, logs)
- RC serial input
- HRT (high-resolution timer)
- EKF2 state estimation
- Multicopter flight control stack

### In Progress (Production Plan)
- IO Timer Allocation API (Phase 1)
- DShot ESC protocol (Phase 2)
- TC-based Input Capture for RC PPM (Phase 3)
- Production hardening — fault protection, watchdog (Phase 4)

### Planned (Custom PCB)
- 8-channel PWM (Phase 5)
- Extended ADC & power monitoring (Phase 6)
- HW versioning & FRAM storage (Phase 7)

---

## Building

```bash
# Configure and build
make microchip_samv71-xult-clickboards_default

# Flash via OpenOCD (EDBG debugger)
openocd -f interface/cmsis-dap.cfg -c "adapter speed 1000" \
  -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

---

## Console Access

```bash
# USB CDC/ACM serial
screen /dev/ttyACM0 115200

# Or via picocom
picocom -b 115200 /dev/ttyACM0
```

---

## Documentation Index

### Planning & Status
| Document | Purpose |
|----------|---------|
| **[Production Plan](PRODUCTION_PLAN.md)** | **Active roadmap: Phases 0–7** |
| [Production Delta vs FMUv6X](PRODUCTION_DELTA_FMUv6X.md) | Gap analysis against FMUv6X reference |
| [Feature Status](FEATURE_STATUS.md) | Detailed feature implementation status |
| [Master Implementation Plan](MASTER_IMPLEMENTATION_PLAN_20251129.md) | Original porting plan |

### Technical Implementation
| Document | Purpose |
|----------|---------|
| [SAMV71 Master Porting Guide](SAMV71_MASTER_PORTING_GUIDE.md) | Platform porting reference |
| [Implementation Tracker](SAMV71_IMPLEMENTATION_TRACKER.md) | Progress tracking |
| [PWM Investigation](PWM_INVESTIGATION_SUMMARY.md) | Timer/PWM implementation notes |
| [PWMC Implementation Plan](PWMC_IMPLEMENTATION_PLAN.md) | DShot/advanced PWM plan |

### Debugging & Issues
| Document | Purpose |
|----------|---------|
| [updateSubscriptions Debug](UPDATESUBSCRIPTIONS_DEBUG.md) | Runtime init fix for MixingOutput |
| [HITL Sensor Issue](HITL_SENSOR_PROCESSING_ISSUE.md) | Simulation sensor pipeline issue |
| [Console Buffer Debug](TASK_CONSOLE_BUFFER_DEBUG.md) | Console buffer fix notes |
| [Logger Debug](TASK_LOGGER_DEBUG_ENABLE.md) | Flight logger investigation |

### Testing & Validation
| Document | Purpose |
|----------|---------|
| [Click Board Validation](CLICK_BOARD_VALIDATION_GUIDE.md) | Sensor testing guide |
| [HITL Testing](TASK_HITL_TESTING.md) | Hardware-in-the-loop testing |

---

## Key Parameters

```bash
# For HITL mode
param set SYS_AUTOSTART 1001
param set SYS_HAS_MAG 0
param set SYS_HAS_BARO 0

# For real sensors
param set SYS_AUTOSTART 4001
param set SYS_HAS_MAG 1
param set SYS_HAS_BARO 1
```

---

## Flash/RAM Usage

```
Flash: 1,335,772 / 2,097,152 bytes (63.69%)
RAM:      52,444 /   393,216 bytes (13.34%)
```

---

## Contributing

This is an active development port. See the **[Production Plan](PRODUCTION_PLAN.md)** for the full implementation roadmap and current priorities.

---

## License

BSD 3-Clause License - See repository root LICENSE file.

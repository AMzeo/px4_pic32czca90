# PX4 Drone Autopilot

[![Releases](https://img.shields.io/github/release/PX4/PX4-Autopilot.svg)](https://github.com/PX4/PX4-Autopilot/releases) [![DOI](https://zenodo.org/badge/22634/PX4/PX4-Autopilot.svg)](https://zenodo.org/badge/latestdoi/22634/PX4/PX4-Autopilot)

[![Build Targets](https://github.com/PX4/PX4-Autopilot/actions/workflows/build_all_targets.yml/badge.svg?branch=main)](https://github.com/PX4/PX4-Autopilot/actions/workflows/build_all_targets.yml) [![SITL Tests](https://github.com/PX4/PX4-Autopilot/workflows/SITL%20Tests/badge.svg?branch=master)](https://github.com/PX4/PX4-Autopilot/actions?query=workflow%3A%22SITL+Tests%22)

[![Discord Shield](https://discordapp.com/api/guilds/1022170275984457759/widget.png?style=shield)](https://discord.gg/dronecode)

This repository holds the [PX4](http://px4.io) flight control solution for drones, with the main applications located in the [src/modules](https://github.com/PX4/PX4-Autopilot/tree/main/src/modules) directory. It also contains the PX4 Drone Middleware Platform, which provides drivers and middleware to run drones.

PX4 is highly portable, OS-independent and supports Linux, NuttX and MacOS out of the box.

---

## Custom Board Support: ATSAMV71-XULT

This repository includes **custom PX4 implementations** for the **Microchip ATSAMV71-XULT development board** with comprehensive fixes and documentation.

---

## 🚀 Microchip PX4 Platform Roadmap

**Vision:** Build the most advanced, secure, and capable flight controller platform on Microchip silicon.

```
PHASE 1 (NOW)      PHASE 2           PHASE 3           PHASE 4
SAMV71             PIC32CZ CA70      PIC32CZ CA80      CA90 + HSM
┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐
│ 300 MHz  │ ───▶ │ 300 MHz  │ ───▶ │ 300 MHz  │ ───▶ │ 300 MHz  │
│ 384KB RAM│      │ 512KB RAM│      │ 1MB + TCM│      │ 1MB + HSM│
│ 2MB Flash│      │ 60% Cost↓│      │ 8MB Flash│      │ Secure   │
│ AEC-Q100 │      │ Pin Compat│      │ Gigabit  │      │ Boot     │
└──────────┘      └──────────┘      └──────────┘      └──────────┘
```

### 📋 Strategic Documents

| Document | Description |
|----------|-------------|
| **[MICROCHIP_PX4_ROADMAP.md](MICROCHIP_PX4_ROADMAP.md)** | Complete 4-phase development plan with timelines |
| **[SAMV71_HARDWARE_ADVANTAGES.md](SAMV71_HARDWARE_ADVANTAGES.md)** | SAMV71 vs STM32H753II (FMU-v6X) comparison |
| **[PIC32CZ_CA80_ANALYSIS.md](PIC32CZ_CA80_ANALYSIS.md)** | Next-gen PIC32CZ CA80/CA90 analysis |
| **[SAMV71_TIER1_BATTLE_PLAN.md](SAMV71_TIER1_BATTLE_PLAN.md)** | Phase 1 implementation checklist |

### 🏆 Why Microchip Wins

| Capability | Our Platform | Pixhawk (STM32) |
|------------|--------------|-----------------|
| **Flash** | 8 MB (CA80) | 2 MB max |
| **Security** | HSM (CA90) | None |
| **Ethernet** | Gigabit | 10/100 |
| **Real-time** | 256KB TCM | No TCM |
| **Supply** | Industrial | Shortage-prone |

---

### 🌿 Branch Structure

This repository maintains two branches for SAMV71-XULT development:

#### **`main`** - Original/Baseline Implementation
Standard PX4 baseline for SAMV71 with basic hardware support.
- **Status:** Reference implementation
- **Use Case:** Starting point, comparison baseline
- **Documentation:** [boards/microchip/samv71-xult-clickboards/PORTING_NOTES.md](boards/microchip/samv71-xult-clickboards/PORTING_NOTES.md)

#### **`samv7-custom`** - Enhanced Implementation ⭐ **Recommended**
Fully working implementation with critical bug fixes and comprehensive documentation.

**✅ Major Fixes Implemented:**
- **Parameter Storage Fix** - Manual construction pattern fixes C++ static initialization bug
- **SD Card Integration** - Complete FAT32 support with parameter persistence
- **USB CDC/ACM Implementation** - High-speed USB MAVLink telemetry on TARGET USB port
- **HRT Implementation** - Reliable TC0 timer configuration
- **DMA Cache Coherency** - Proper memory management for SDIO and USB
- **Safe Mode Configuration** - Stable baseline for incremental development

**✅ Current Status (December 6, 2025):**
- **Production Readiness:** 75%
- Flash: 1,335 KB / 2 MB (63.69%)
- SRAM: 52 KB / 384 KB (13.34%)
- Parameter set/save/persistence: ✅ Working
- SD card I/O: ✅ Working
- USB CDC/ACM MAVLink: ✅ Working
- HITL Simulation: ✅ **Verified** (jMAVSim tested)
- Click Board Sensors: ✅ 7 drivers enabled
- Flight Control Stack: ✅ Full MC control (EKF2, commander, etc.)
- MixingOutput Fix: ✅ Runtime init fix applied

**📋 Production & Team Planning (NEW):**
| Document | Description |
|----------|-------------|
| **[Board README](boards/microchip/samv71-xult-clickboards/README.md)** | Board overview & documentation index |
| **[PRODUCTION_READINESS.md](boards/microchip/samv71-xult-clickboards/PRODUCTION_READINESS.md)** | 75% ready - gap analysis & roadmap |
| **[TEAM_DEVELOPMENT_PLAN.md](boards/microchip/samv71-xult-clickboards/TEAM_DEVELOPMENT_PLAN.md)** | 4-developer parallel work breakdown |
| **[HITL_SENSOR_PROCESSING_ISSUE.md](boards/microchip/samv71-xult-clickboards/HITL_SENSOR_PROCESSING_ISSUE.md)** | Open issue: simulation sensor pipeline |

**📚 Complete Documentation Suite (30+ documents):**
- **[DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)** - Central navigation hub for all documentation
- **[CURRENT_STATUS.md](CURRENT_STATUS.md)** - Latest system state, features, and issues
- **[README_SAMV7.md](README_SAMV7.md)** - Branch overview and quick start guide
- **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - Critical parameter storage fix details
- **[SAMV71_USB_CDC_IMPLEMENTATION.md](SAMV71_USB_CDC_IMPLEMENTATION.md)** - USB CDC/ACM MAVLink implementation guide
- **[SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md](SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md)** - Incremental service re-enablement plan
- **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - Reference baseline configuration

### Hardware Configuration
- **MCU:** ATSAM V71Q21 (ARM Cortex-M7 @ 300MHz)
- **Flash:** 2MB
- **SRAM:** 384KB
- **USB:** High-speed CDC/ACM (480 Mbps) for MAVLink telemetry
- **SD Card:** FAT32 support (tested with 16GB)
- **Sensors:** I2C bus ready for ICM20689 IMU, magnetometer, barometer

### 🚀 Quick Start (samv7-custom branch)

```bash
# Clone and checkout enhanced branch
git clone https://github.com/YOUR_USERNAME/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private
git checkout samv7-custom

# Update submodules
git submodule update --init --recursive

# Build firmware
make microchip_samv71-xult-clickboards_default

# Flash via OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

### 📖 Documentation Quick Links

**For New Users:**
1. Start with **[README_SAMV7.md](README_SAMV7.md)** for branch overview
2. Check **[CURRENT_STATUS.md](CURRENT_STATUS.md)** for latest features/issues
3. Use **[DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md)** to navigate all docs

**For Developers:**
- **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - Understanding the critical fix
- **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Complete development journey
- **[DOCS_DMESG_HRT_ENABLE.md](DOCS_DMESG_HRT_ENABLE.md)** - Debugging tools setup

**Technical Deep Dives:**
- **[SD_CARD_INTEGRATION_SUMMARY.md](SD_CARD_INTEGRATION_SUMMARY.md)** - SD card implementation
- **[HRT_IMPLEMENTATION_SUMMARY.md](HRT_IMPLEMENTATION_SUMMARY.md)** - Timer implementation
- **[CACHE_COHERENCY_GUIDE.md](CACHE_COHERENCY_GUIDE.md)** - DMA memory management

### 🎯 Recommended Workflow

1. **Use `samv7-custom` branch** for active development
2. **Reference `main` branch** only for comparison with upstream
3. **Start with safe-mode** configuration for testing
4. **Follow incremental approach** for enabling services
5. **Check documentation** before making changes

### 🔧 Switching Between Branches

```bash
# Switch to enhanced version
git checkout samv7-custom
git submodule update --init --recursive
make clean && make microchip_samv71-xult-clickboards_default

# Switch to original (for reference)
git checkout main
git submodule update --init --recursive
make clean && make microchip_samv71-xult-clickboards_default
```

---

* Official Website: http://px4.io (License: BSD 3-clause, [LICENSE](https://github.com/PX4/PX4-Autopilot/blob/main/LICENSE))
* [Supported airframes](https://docs.px4.io/main/en/airframes/airframe_reference.html) ([portfolio](https://px4.io/ecosystem/commercial-systems/)):
  * [Multicopters](https://docs.px4.io/main/en/frames_multicopter/)
  * [Fixed wing](https://docs.px4.io/main/en/frames_plane/)
  * [VTOL](https://docs.px4.io/main/en/frames_vtol/)
  * [Autogyro](https://docs.px4.io/main/en/frames_autogyro/)
  * [Rover](https://docs.px4.io/main/en/frames_rover/)
  * many more experimental types (Blimps, Boats, Submarines, High Altitude Balloons, Spacecraft, etc)
* Releases: [Downloads](https://github.com/PX4/PX4-Autopilot/releases)

## Releases

Release notes and supporting information for PX4 releases can be found on the [Developer Guide](https://docs.px4.io/main/en/releases/).

## Building a PX4 based drone, rover, boat or robot

The [PX4 User Guide](https://docs.px4.io/main/en/) explains how to assemble [supported vehicles](https://docs.px4.io/main/en/airframes/airframe_reference.html) and fly drones with PX4. See the [forum and chat](https://docs.px4.io/main/en/#getting-help) if you need help!


## Changing Code and Contributing

This [Developer Guide](https://docs.px4.io/main/en/development/development.html) is for software developers who want to modify the flight stack and middleware (e.g. to add new flight modes), hardware integrators who want to support new flight controller boards and peripherals, and anyone who wants to get PX4 working on a new (unsupported) airframe/vehicle.

Developers should read the [Guide for Contributions](https://docs.px4.io/main/en/contribute/).
See the [forum and chat](https://docs.px4.io/main/en/#getting-help) if you need help!


## Weekly Dev Call

The PX4 Dev Team syncs up on a [weekly dev call](https://docs.px4.io/main/en/contribute/).

> **Note** The dev call is open to all interested developers (not just the core dev team). This is a great opportunity to meet the team and contribute to the ongoing development of the platform. It includes a QA session for newcomers. All regular calls are listed in the [Dronecode calendar](https://www.dronecode.org/calendar/).


## Maintenance Team

See the latest list of maintainers on [MAINTAINERS](MAINTAINERS.md) file at the root of the project.

For the latest stats on contributors please see the latest stats for the Dronecode ecosystem in our project dashboard under [LFX Insights](https://insights.lfx.linuxfoundation.org/foundation/dronecode). For information on how to update your profile and affiliations please see the following support link on how to [Complete Your LFX Profile](https://docs.linuxfoundation.org/lfx/my-profile/complete-your-lfx-profile). Dronecode publishes a yearly snapshot of contributions and achievements on its [website under the Reports section](https://dronecode.org).

## Supported Hardware

For the most up to date information, please visit [PX4 User Guide > Autopilot Hardware](https://docs.px4.io/main/en/flight_controller/).

## Project Governance

The PX4 Autopilot project including all of its trademarks is hosted under [Dronecode](https://www.dronecode.org/), part of the Linux Foundation.

<a href="https://www.dronecode.org/" style="padding:20px" ><img src="https://dronecode.org/wp-content/uploads/sites/24/2020/08/dronecode_logo_default-1.png" alt="Dronecode Logo" width="110px"/></a>
<div style="padding:10px">&nbsp;</div>

# SAMV71-XULT PX4 Development - Team Setup Guide

This guide explains how to correctly clone and set up the PX4-Autopilot-Private repository for SAMV71 development.

## Prerequisites

- Git installed
- GitHub SSH key configured (recommended) or HTTPS access
- ARM toolchain installed (`arm-none-eabi-gcc`)
- Build tools: `make`, `cmake`, `python3`

## Quick Start (New Clone)

```bash
# 1. Clone the repository with submodules
git clone --recursive -b samv7-custom git@github.com:bhanuprakashjh/PX4-Autopilot-Private.git

# 2. Navigate to the directory
cd PX4-Autopilot-Private

# 3. Verify submodules are initialized
git submodule status
```

## Detailed Setup Steps

### Step 1: Clone the Repository

**Option A: SSH (Recommended)**
```bash
git clone -b samv7-custom git@github.com:bhanuprakashjh/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private
```

**Option B: HTTPS**
```bash
git clone -b samv7-custom https://github.com/bhanuprakashjh/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private
```

### Step 2: Initialize Submodules

```bash
# Sync submodule URLs from .gitmodules
git submodule sync

# Initialize and update all submodules recursively
git submodule update --init --recursive
```

**Expected output:** Should complete without errors. The NuttX submodule will be fetched from `bhanuprakashjh/NuttX` (our fork with SAMV7 fixes).

### Step 3: Verify NuttX Submodule

```bash
# Check NuttX is at the correct commit
cd platforms/nuttx/NuttX/nuttx
git log -1 --oneline
```

**Expected output:**
```
a8b05b8a58 SAMV71: Fix HSMCI TX DMA and add debug infrastructure
```

```bash
# Return to main directory
cd ../../../..
```

### Step 4: Build the Firmware

```bash
# Build for SAMV71-XULT with Click boards
make microchip_samv71-xult-clickboards_default
```

**Expected build output:**
```
Memory region         Used Size  Region Size  %age Used
       flash:     1,340,340 B         2 MB     63.91%
        sram:        52,588 B       320 KB     16.05%
     nocache:          5 KB        64 KB      7.81%
```

### Step 5: Flash the Board

```bash
# Using OpenOCD with EDBG debugger
openocd -f interface/cmsis-dap.cfg \
        -c "adapter speed 1000" \
        -f target/atsamv.cfg \
        -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

## Updating Your Local Repository

When pulling new changes:

```bash
# 1. Pull latest changes
git pull origin samv7-custom

# 2. Sync and update submodules
git submodule sync
git submodule update --init --recursive
```

## Troubleshooting

### Error: "not our ref" during submodule update

**Symptom:**
```
fatal: remote error: upload-pack: not our ref a8b05b8a58bfc194ccff4bc75fabd17be8e5ed9f
```

**Solution:**
```bash
# 1. Ensure you're on samv7-custom branch
git checkout samv7-custom
git pull origin samv7-custom

# 2. Sync submodule URLs
git submodule sync

# 3. Re-initialize submodules
git submodule update --init --recursive
```

### Error: Submodule URL mismatch

**Check current URL:**
```bash
git config --file .gitmodules submodule.platforms/nuttx/NuttX/nuttx.url
```

**Expected output:**
```
https://github.com/bhanuprakashjh/NuttX.git
```

If it shows `PX4/NuttX.git`, you have an old version. Pull the latest:
```bash
git pull origin samv7-custom
git submodule sync
```

### Clean Rebuild

If you encounter build issues:
```bash
# Clean build artifacts
make clean

# Or for a complete clean
make distclean

# Rebuild
make microchip_samv71-xult-clickboards_default
```

## Repository Structure

```
PX4-Autopilot-Private/
├── boards/microchip/samv71-xult-clickboards/  # Board-specific files
│   ├── src/
│   │   ├── board_config.h    # GPIO and hardware config
│   │   ├── spi.cpp           # SPI bus configuration
│   │   └── init.c            # Board initialization
│   └── nuttx-config/         # NuttX configuration
├── platforms/nuttx/NuttX/
│   └── nuttx/                # NuttX RTOS (submodule → bhanuprakashjh/NuttX)
└── build/                    # Build output directory
```

## Working Sensors

| Sensor | Type | Interface | Command | Status |
|--------|------|-----------|---------|--------|
| ICM20689 | IMU | SPI | `icm20689 start -s` | Working |
| BMP388 | Barometer | SPI | `bmp388 start -s` | In Progress |
| AK09915 | Magnetometer | I2C | `ak09916 -I start` | Configured |

## Serial Console

Connect to the board's debug UART (via EDBG USB):
```bash
# Find the serial port
ls /dev/ttyACM*

# Connect (typically 115200 baud)
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

## Contact

For issues with this setup, contact the SAMV7 development team.

---
*Last updated: December 2024*

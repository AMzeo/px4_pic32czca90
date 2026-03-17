# PX4 Port to PIC32CZ CA70 Curiosity — Status Document

**Date:** March 2026
**Branch:** `pic32cz-ca70-port`
**Base branch:** `samv71-custom`
**Author:** Microchip Technology (Embedded Firmware Team)

---

## 1. Overview

This document describes the work done to port PX4 autopilot firmware to the
**Microchip PIC32CZ CA70 Curiosity** development board (144-pin variant). The
PIC32CZ CA70 is a Cortex-M7 microcontroller with the same peripheral IP blocks
as the Microchip SAMV71, making it possible to base the port on the existing
`samv71-xult-clickboards` PX4 board definition with targeted modifications.

The port is functional for HITL (Hardware In The Loop) simulation and has all
major peripherals configured. Real sensor integration and flight validation are
the remaining steps before actual drone flight.

---

## 2. Target Hardware

| Item | Detail |
|---|---|
| Board | PIC32CZ CA70 Curiosity (EV46X93A) |
| MCU | PIC32CZ CA70 (Cortex-M7, 144-pin) |
| Clock | 150 MHz (MCK) |
| Flash | 2 MB internal |
| SRAM | 512 KB (0x80000) |
| External Flash | SST26VF032B 4MB QSPI |
| Debug USB | PKOB4 (J700) → `/dev/ttyACM0` on Linux (NSH console) |
| Target USB | USBHS (J200) → `/dev/ttyACM1` on Linux (MAVLink CDC/ACM) |

---

## 3. Port Strategy

The PIC32CZ CA70 shares the same peripheral IP blocks as the SAMV71:
- Same USBHS controller (identical register map at 0x40038000)
- Same SPI/QSPI/TWIHS/UART/USART controllers
- Same PWMC, TC, AFEC peripherals
- Same NuttX SAMV7 architecture support

The port was created by cloning `boards/microchip/samv71-xult-clickboards` and
making targeted changes for the PIC32CZ CA70 chip variant. NuttX was extended
via submodule patches to add PIC32CZ CA70 chip support.

---

## 4. NuttX Submodule Changes

The NuttX submodule (`platforms/nuttx/NuttX/nuttx`) was updated with multiple
patches to support the PIC32CZ CA70:

| Commit | Change |
|---|---|
| `7a62af7` | irq.h PIC32CZ fix |
| `991448a` | chip.h PIC32CZ peripheral counts |
| `be76bfe` | Full PIC32CZ CA70 chip support added to NuttX |
| `9b04941` | QSPI driver fix for PIC32CZ CA70 |
| `6f96617` | HSMCI, progmem, EEFC PIC32CZ fixes |

---

## 5. Board Definition Files Created

All files under `boards/microchip/pic32czca70-curiosity/`:

```
default.px4board              — CMake board config, chip=PIC32CZCA70144
firmware.prototype            — board_id=1372, USB PID
nuttx-config/include/board.h  — Clock, GPIO, peripheral pin assignments
nuttx-config/include/board_dma_map.h — DMA channel allocation
nuttx-config/nsh/defconfig    — NuttX Kconfig (drivers, USB, QSPI, HSMCI)
nuttx-config/scripts/script.ld — Linker script (flash=2MB, SRAM=512KB)
nuttx-config/scripts/Make.defs — Compiler/linker flags
init/rc.board_defaults        — PX4 parameter defaults
init/rc.board_mavlink         — sercon + MAVLink startup
init/rc.board_extras          — navigator start
init/rc.board_sensors         — sensor driver startup
init/rc.logging               — logging config
src/CMakeLists.txt            — board driver build list
src/board_config.h            — GPIO definitions, peripheral config
src/init.c                    — Board init (MPU, clocks, DMA, SD, I2C, QSPI)
src/usb.c                     — USB board functions
src/led.c                     — LED driver
src/spi.cpp                   — SPI bus init
src/i2c.cpp                   — I2C bus init
src/qspi.c                    — QSPI flash init + MTD partition setup
src/sam_hsmci.c               — HSMCI (SD card) board glue
src/sam_mpuinit.c             — MPU nocache region for DMA buffers
src/timer_config.cpp          — PWM IO timer config
src/airframes/60200_pic32czca70_dev — Airframe definition
```

---

## 6. Key Configuration Differences from SAMV71

| Item | SAMV71 | PIC32CZ CA70 |
|---|---|---|
| Chip name | SAMV71Q21 | PIC32CZCA70144 |
| SRAM | 384 KB | 512 KB |
| Board ID | 1371 | 1372 |
| NSH console | USART1 | UART1 (PKOB4 virtual COM) |
| QSPI flash | S25FL116K 2MB (W25 driver) | SST26VF032B 4MB (SST26 driver) |
| airframe | 60100_samv71_dev | 60200_pic32czca70_dev |

---

## 7. Peripheral Status

### 7.1 Boot & Core

| Peripheral | Status | Notes |
|---|---|---|
| NuttX boot | ✓ Working | UART1 console via PKOB4 |
| HRT timer | ✓ Working | TC0 CH0 + PCK6 @ 1 MHz |
| MPU | ✓ Working | Nocache region for DMA |
| DMA allocator | ✓ Working | 5120 byte pool |
| LEDs | ✓ Working | PA23 (blue), PC9 (safety) |
| Safety button | ✓ Working | PA9, active-low |

### 7.2 Storage

| Peripheral | Status | Notes |
|---|---|---|
| QSPI Flash | ✓ Working | SST26VF032B 4MB, 3 partitions |
| — /fs/mtd_params | ✓ | 128 KB, PX4 params |
| — /fs/mtd_caldata | ✓ | 64 KB, calibration backup |
| — /fs/mtd_waypoints | ✓ | 512 KB, dataman (missions) |
| HSMCI SD card | ✓ Driver working | No card inserted currently |

### 7.3 Communication

| Peripheral | Status | Notes |
|---|---|---|
| USB USBHS (CDC/ACM) | ✓ Working | MAVLink on /dev/ttyACM1 |
| UART1 (NSH console) | ✓ Working | Via PKOB4 debug USB |
| UART2 (GPS) | ✓ Configured | /dev/ttyS2, 57600 baud |
| UART4 (RC input) | ✓ Configured | /dev/ttyS3 |
| I2C0 (TWIHS0) | ✓ Working | PA3/PA4, sensors |
| SPI0 | ✓ Configured | ICM20689, BMP388 |

### 7.4 Actuators & Timers

| Peripheral | Status | Notes |
|---|---|---|
| PWMC0 CH0 (Motor 4) | ✓ **Hardware verified** | PB0, 400Hz, correct voltage confirmed |
| PWMC0 CH1 (Motor 2) | ✓ **Hardware verified** | PA2, 400Hz, correct voltage confirmed |
| PWMC0 CH2 (Motor 3) | ✓ **Hardware verified** | PC19, 400Hz, correct voltage confirmed |
| PWMC0 CH3 (Motor 1) | ✓ **Hardware verified** | PC13, 400Hz, correct voltage confirmed |
| RC input capture | ✓ Configured | TC5 (PC29) |

**PWM Verification results (multimeter on PC13):**
- Disarmed (1000µs pulse): measured **1.30V** — expected 1000/2500 × 3.3V = 1.32V ✓
- Actuator test 10% (1100µs pulse): measured **1.43V** — expected 1100/2500 × 3.3V = 1.45V ✓

**Required params for PWM to work** (must be set, not in defaults):
```
param set PWM_MAIN_FUNC1 101   # Motor 1 → Channel 0
param set PWM_MAIN_FUNC2 102   # Motor 2 → Channel 1
param set PWM_MAIN_FUNC3 103   # Motor 3 → Channel 2
param set PWM_MAIN_FUNC4 104   # Motor 4 → Channel 3
```
Without these, `pwm_out status` shows `func: 0` and actuator_test has no effect.

### 7.5 Sensors (board connectors)

| Sensor | Interface | Status | Notes |
|---|---|---|---|
| ICM20689 IMU | SPI0 (PD25 CS, PD28 DRDY) | Configured | Untested with hardware |
| BMP388 Barometer | SPI0 (PD27 CS) | Configured | Untested with hardware |
| AK09915 Magnetometer | I2C0 | Not connected | — |
| GPS | UART2 | Configured | Not connected |

---

## 8. Issues Encountered and Resolutions

### 8.1 SRAM Size Mismatch
- **Problem:** Initial port used SAMV71 384 KB RAM size; PIC32CZ CA70 has 512 KB
- **Fix:** Updated linker script `script.ld` and `CONFIG_RAM_SIZE=524288` in defconfig
- **Commit:** `75d967b889`

### 8.2 QSPI Driver Incompatibility
- **Problem:** SAMV71 used W25 MTD driver; SST26VF032B requires SST26 driver with different quad-enable and block-protect-unlock sequences
- **Fix:** Replaced W25 driver with SST26 MTD driver in `qspi.c`; NuttX submodule patched for QSPI fix
- **Commit:** `9b04941e0c`, `007e8103b2`

### 8.3 UART1 Console (PKOB4 Virtual COM)
- **Problem:** Initial port used USART1 for NSH console; PKOB4 debugger routes UART1 to its virtual COM port
- **Fix:** Changed console to UART1 with correct GPIO mux (PA5=RX Periph C, PA4=TX Periph A); GPIO_EXT1_RST removed from init list since PA5 conflicts
- **Commit:** `007e8103b2`

### 8.4 USB CDC/ACM Alternating Enumeration Failure
- **Problem:** Every other boot fails with `could not open /dev/ttyACM0`; Linux host does not detect device reconnect during soft reset when USB cable stays connected
- **Root cause:** Linux does not send a new bus reset when the cable stays physically connected across a board soft reset; device gets stuck in `DEVIMR=0x38` state (WAKEUP+EORSM enabled, waiting for host traffic that never comes)
- **Workaround:** Physically unplug and replug the TARGET USB cable before each board reset
- **Status:** Open — proper fix requires either host-side USB reset or USBHS hardware reset on every PX4 boot

### 8.5 MAVLink Race Condition on USB Open
- **Problem:** MAVLink opened `/dev/ttyACM0` before host enumeration completed; first write failed with `-EIO` causing MAVLink to exit
- **Fix:** Added `sleep 2` in `rc.board_mavlink` after `sercon` to allow host enumeration to complete before MAVLink starts
- **Commit:** `007e8103b2`

### 8.6 GPIO_EXT1_RST Conflict
- **Problem:** PA5 defined as `GPIO_EXT1_RST` (output) but PA5 is also UART1 RX (Periph C); configuring as GPIO output broke UART1 console
- **Fix:** Commented out `GPIO_EXT1_RST` from `PX4_GPIO_INIT_LIST`
- **Commit:** `5dd71ee7fc`

---

## 9. USB Port Mapping and Known Enumeration Issue

### 9.1 USB Port Assignment

The PIC32CZ CA70 Curiosity board has two USB connectors. Both must be connected
to the Linux host for full development use:

| Connector | USB Device | Linux Device | Used By |
|---|---|---|---|
| J700 — PKOB4 debug USB | MPLAB PKoB4 virtual COM | `/dev/ttyACM0` | NSH console |
| J200 — Target USB | PX4 USBHS CDC/ACM (MAVLink) | `/dev/ttyACM1` | jMAVSim / QGC |

**Typical terminal layout:**
```
Terminal 1: minicom -D /dev/ttyACM0 -b 115200     ← NSH console
Terminal 2: jmavsim_run.sh -d /dev/ttyACM1 ...    ← jMAVSim
Terminal 3: ./QGroundControl.AppImage              ← QGC (UDP 14550)
```

### 9.2 USB Enumeration Symptom

jMAVSim will fail with:
```
ERROR: Failed to open MAV port: Port name - /dev/ttyACM1; Exception type - Port not found.
```

This means the TARGET USB did not enumerate on the Linux host. On the board side,
`mavlink status` will show:
```
tx: 0.0 B/s
txerr: 948.0 B/s     ← MAVLink trying to send but host not connected
rx: 0.0 B/s
```

### 9.3 Root Cause

The USBHS peripheral on PIC32CZ CA70 is **not reset by a software/watchdog reset**.
It retains state from the previous boot. When the USB cable remains physically
connected across a board reset, the Linux host does not detect a disconnect/reconnect
event and never sends a new USB bus reset. The device stays stuck in
`DEVIMR=0x38` (WAKEUP+EORSM enabled) waiting for host traffic that never comes,
causing every other boot to fail enumeration.

### 9.4 Workaround

**Before every board reset:**
1. Physically unplug the TARGET USB cable (J200)
2. Reset the board
3. Wait for the board to fully boot
4. Replug the TARGET USB cable
5. Verify `/dev/ttyACM1` appears: `ls /dev/ttyACM*`
6. Then start jMAVSim

This forces Linux to detect a clean disconnect/reconnect and enumerate fresh
every time.

---

## 10. HITL Simulation Setup (verified working)

HITL (Hardware In The Loop) has been verified working with jMAVSim:

**Host setup:**
- Java 11 (jMAVSim requires ≤ Java 11)
- jMAVSim built from PX4 submodule: `Tools/simulation/jmavsim/jMAVSim`
- QGroundControl AppImage on Linux host

**Board parameters for HITL:**
```
SYS_HITL        = 1
SYS_AUTOSTART   = 1001    (HIL Quadcopter X)
CBRK_IO_SAFETY  = 22027   (bypass safety button)
CBRK_SUPPLY_CHK = 894281  (bypass voltage check)
COM_ARM_WO_GPS  = 1
COM_DISARM_PRFLT = -1     (disable preflight auto-disarm)
SDLOG_MODE      = -1      (logging disabled, no SD card)
```

**Run command:**
```bash
Tools/simulation/jmavsim/jmavsim_run.sh -d /dev/ttyACM1 -b 57600 -q
```

**Verified:**
- Arm / Takeoff / Land cycle ✓
- EKF2 initializes with simulated GPS/IMU/baro from jMAVSim ✓
- QGC connects via UDP localhost:14550 ✓

---

## 11. Pending Work for Real Drone Flight

### High Priority
1. **IMU verification** — Connect ICM20689 on SPI0, verify driver reads valid accel/gyro data
2. **Barometer verification** — Connect BMP388 on SPI0, verify altitude readings
3. **Magnetometer** — Connect AK09915 on I2C0, configure and test
4. **GPS** — Connect GPS module to UART2, verify position fix
5. **PWM output to ESCs** — PWM signal verified ✓; next: connect real ESCs, verify motor spin and direction

### Medium Priority
6. **RC input** — Connect RC receiver to UART4, verify channel decode
7. **SD card** — Insert microSD, verify logging works
8. **Battery monitoring** — Verify AFEC0 ADC channels (PD30=voltage, PA18=current)
9. **Full sensor calibration** — Accel, gyro, mag, level horizon

### Low Priority / Known Issues
10. **USB enumeration bug** — Implement proper USBHS reset on every soft boot to avoid needing physical USB replug
11. **Safety button polarity** — Confirm `BOARD_SAFETY_BUTTON_ACTIVE_LOW` logic works correctly with real arming

---

## 12. Build Instructions

```bash
# Clone and setup
git clone <repo>
cd PX4-PIC32CZ-CA70
git checkout pic32cz-ca70-port
git submodule update --init --recursive

# Build firmware
make microchip_pic32czca70-curiosity_default

# Generate Intel HEX for MPLAB programming
arm-none-eabi-objcopy -O ihex \
  build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.elf \
  build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.hex
```

**Output files:**
- `.elf` — ELF binary (for debugging)
- `.bin` — Raw binary
- `.px4` — PX4 upload format
- `.hex` — Intel HEX (for MPLAB X / PKOB4 programmer)

**Flash usage:** ~1352 KB / 2048 KB (64.5%)
**SRAM usage:** ~53 KB / 448 KB (11.7%)

---

## 13. Key File Reference

| File | Purpose |
|---|---|
| `boards/microchip/pic32czca70-curiosity/default.px4board` | Top-level board config |
| `boards/microchip/pic32czca70-curiosity/nuttx-config/include/board.h` | All GPIO/clock/peripheral pin assignments |
| `boards/microchip/pic32czca70-curiosity/nuttx-config/nsh/defconfig` | NuttX Kconfig options |
| `boards/microchip/pic32czca70-curiosity/nuttx-config/scripts/script.ld` | Linker script |
| `boards/microchip/pic32czca70-curiosity/src/board_config.h` | PX4 GPIO macros, ADC config, QSPI partition layout |
| `boards/microchip/pic32czca70-curiosity/src/init.c` | Board initialization sequence |
| `boards/microchip/pic32czca70-curiosity/src/qspi.c` | QSPI flash + MTD partitions |
| `boards/microchip/pic32czca70-curiosity/init/rc.board_defaults` | Default PX4 parameters |
| `boards/microchip/pic32czca70-curiosity/init/rc.board_mavlink` | MAVLink/USB startup script |
| `platforms/nuttx/NuttX/nuttx` | NuttX submodule (patched for PIC32CZ CA70) |

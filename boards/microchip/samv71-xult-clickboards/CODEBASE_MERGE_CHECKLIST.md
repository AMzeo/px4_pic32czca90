# Codebase Merge Checklist — SAMV71 PX4 Port

> **Purpose:** Cross-verify pin assignments and driver configs between the two codebases
> before merging. One codebase (this repo, `samv7-custom` branch) is the reference.
> The other is the teammate's clone with hardware-verified changes.
>
> **Date:** 2026-02-10
> **Pin Map Reference:** `SAMV71Q21B_PX4_DRONE_PIN_MAPPING.md`

---

## How To Use This Checklist

For each item, check what the **teammate's code** has and compare against what
**this repo** has. Mark the resolution column with who is correct or if both
need to change.

---

## 1. PWM Motor Pin Mapping

The pin map doc and this codebase disagree on motor numbering.

| Motor | Pin Map Doc | This Repo (`timer_config.cpp`) | Teammate Code | Resolution |
|-------|-------------|-------------------------------|---------------|------------|
| Motor 1 (index 0) | PB0 / CH0 | **PC13 / CH3 / Periph B** | [ ] _________ | [ ] |
| Motor 2 (index 1) | PA2 / CH1 | **PA2 / CH1 / Periph A** | [ ] _________ | [ ] |
| Motor 3 (index 2) | PC19 / CH2 | **PC19 / CH2 / Periph B** | [ ] _________ | [ ] |
| Motor 4 (index 3) | PC13 / CH3 | **PB0 / CH0 / Periph A** | [ ] _________ | [ ] |

**Key question:** Which physical motor position on the drone frame is Motor 1?
Is it wired to PC13 or PB0? The array index in `timer_io_channels[]` determines
the motor number that PX4 uses (index 0 = Motor 1).

### Peripheral Mux Verification

| Pin | Pin Map Doc says | NuttX pinmap header says | This Repo uses |
|-----|-----------------|-------------------------|----------------|
| PC13 (PWMC0_H3) | Peripheral D | **Peripheral B** (`GPIO_PWMC0_H3_4`) | **Peripheral B** |
| PA2 (PWMC0_H1) | Peripheral A | Peripheral A | Peripheral A |
| PC19 (PWMC0_H2) | Peripheral B | Peripheral B | Peripheral B |
| PB0 (PWMC0_H0) | Peripheral A | Peripheral A | Peripheral A |

**Action:** The pin map doc is wrong about PC13 — it is Peripheral B, not D.
Verify teammate's code also uses Peripheral B. PWM is working on hardware
with Peripheral B, so this is confirmed correct.

**File to check in teammate's code:**
```
boards/microchip/samv71-xult-clickboards/src/timer_config.cpp
```

---

## 2. SPI IMU Sensor

The pin map doc references "6DOF IMU 27 Click" (ICM-42688-P). This repo has
ICM-20689 ("6DOF IMU 6 Click"). Different Click boards have different chips
and may use different CS/DRDY pins.

| Item | Pin Map Doc | This Repo | Teammate Code | Resolution |
|------|-------------|-----------|---------------|------------|
| IMU Click board | 6DOF IMU 27 (ICM-42688-P) | 6DOF IMU 6 (ICM-20689) | [ ] _________ | [ ] |
| CS pin | PD27 | **PD25** (EXT1 pin 15) | [ ] _________ | [ ] |
| DRDY pin | (not listed) | **PD28** (EXT1 pin 9) | [ ] _________ | [ ] |
| SPI MISO | PD20 | PD20 | [ ] _________ | [ ] |
| SPI MOSI | PD21 | PD21 | [ ] _________ | [ ] |
| SPI SCK | PD22 | PD22 | [ ] _________ | [ ] |
| Driver started | — | `icm20689 start -s` | [ ] _________ | [ ] |

**Key question:** Which EXT header is the IMU plugged into?
- EXT1 pin 15 = PD25 (CS in this repo)
- EXT2 pin 15 = PD27 (CS in pin map doc)

If teammate uses EXT2, CS should be PD27. If EXT1, CS should be PD25.

**Files to check in teammate's code:**
```
boards/microchip/samv71-xult-clickboards/src/board_config.h   (GPIO_SPI0_CS_*, GPIO_SPI0_DRDY_*)
boards/microchip/samv71-xult-clickboards/src/spi.cpp          (SPI device table)
boards/microchip/samv71-xult-clickboards/init/rc.board_sensors (driver start commands)
boards/microchip/samv71-xult-clickboards/default.px4board      (CONFIG_DRIVERS_IMU_*)
```

---

## 3. Barometer (BMP388)

| Item | Pin Map Doc | This Repo | Teammate Code | Resolution |
|------|-------------|-----------|---------------|------------|
| Interface | **I2C** (TWI0) | **SPI** (SPI0) | [ ] _________ | [ ] |
| CS pin (if SPI) | — | PD27 (EXT2 pin 15) | [ ] _________ | [ ] |
| I2C address (if I2C) | (implied 0x76/0x77) | — | [ ] _________ | [ ] |
| Driver start cmd | — | `bmp388 start -s` | [ ] _________ | [ ] |

The Pressure 5 Click (BMP388) supports both SPI and I2C. The interface
depends on jumper/solder bridge configuration on the Click board.

**Files to check in teammate's code:**
```
boards/microchip/samv71-xult-clickboards/init/rc.board_sensors
boards/microchip/samv71-xult-clickboards/src/board_config.h
```

---

## 4. Magnetometer

| Item | Pin Map Doc | This Repo | Teammate Code | Resolution |
|------|-------------|-----------|---------------|------------|
| Chip | BMM150 | BMM150 + AK09916 (both enabled) | [ ] _________ | [ ] |
| Interface | I2C (TWI0) | I2C (TWI0) | [ ] _________ | [ ] |
| Bus | PA3/PA4 | PA3/PA4 | [ ] _________ | [ ] |
| Driver start | — | `bmm150 start -I -b 1` | [ ] _________ | [ ] |

**Question:** Which mag Click board is physically connected? GeoMagnetic (BMM150)
or Compass 4 (AK09915/AK09916)? Both drivers are enabled in this repo.

---

## 5. RC Input — CONFIRMED WORKING on Teammate's Setup

| Item | Pin Map Doc | This Repo | Teammate Code | Resolution |
|------|-------------|-----------|---------------|------------|
| Peripheral | UART4 | UART4 | [ ] _________ | [ ] |
| RX pin | PD18 | PD18 (`/dev/ttyS3`) | [ ] _________ | [ ] |
| TX pin | — (RX only) | — | [ ] _________ | [ ] |
| Baud rate | — | 115200 (defconfig) | [ ] _________ | [ ] |
| RC protocol | RadioMaster R81 V2 | serial RC (SBUS/CRSF) | [ ] _________ | [ ] |
| PX4 serial config | — | `CONFIG_BOARD_SERIAL_RC="/dev/ttyS3"` | [ ] _________ | [ ] |

RC is working on teammate's hardware — this confirms PD18/UART4 is correct.

**Question for teammate:** What `RC_PORT_CONFIG` param value is set?
This repo sets `param set-default RC_PORT_CONFIG 300` in `rc.board_defaults`.

**Note:** PD18 is shared with SD Card Detect (`GPIO_HSMCI0_CD`). Since RC
works, UART4 peripheral mux overrides the GPIO CD function. The CD define
in `board_config.h` is stale — SD card works without hardware card detect
(always-present assumption).

---

## 6. GPS

| Item | Pin Map Doc | This Repo | Teammate Code | Resolution |
|------|-------------|-----------|---------------|------------|
| Peripheral | UART2 | UART2 | [ ] _________ | [ ] |
| TX pin | PD26 (Periph C) | PD26 | [ ] _________ | [ ] |
| RX pin | PD25 (Periph C) | PD25 | [ ] _________ | [ ] |
| Baud rate | — | 57600 (defconfig) | [ ] _________ | [ ] |
| PX4 serial config | — | `CONFIG_BOARD_SERIAL_GPS1="/dev/ttyS2"` | [ ] _________ | [ ] |

**IMPORTANT conflict:** PD25 is listed as both GPS UART2 RX (Peripheral C)
AND SPI0 CS for IMU (GPIO output) in this repo. These cannot coexist on the
same pin. If GPS is on UART2, then the IMU CS must be on a different pin.

**Question for teammate:** Is GPS working simultaneously with the IMU?
If yes, what pin is the IMU CS actually on?

---

## 7. Debug Console

| Item | Pin Map Doc | This Repo (defconfig) | Teammate Code | Resolution |
|------|-------------|----------------------|---------------|------------|
| Peripheral | USART1 | USART1 (serial console) | [ ] _________ | [ ] |
| TX pin | PB4 (Periph C) | PB4 | [ ] _________ | [ ] |
| RX pin | PA21 (Periph A) | PA21 | [ ] _________ | [ ] |
| Baud rate | — | 115200 | [ ] _________ | [ ] |

---

## 8. UART0 — Potential Stale Config

| Item | This Repo | Teammate Code | Resolution |
|------|-----------|---------------|------------|
| UART0 enabled | **YES** (`CONFIG_SAMV7_UART0=y`) | [ ] _________ | [ ] |
| UART0 used for? | **Nothing** (not in serial port assignments) | [ ] _________ | [ ] |
| PA9 conflict | UART0_RXD vs Safety Button | [ ] _________ | [ ] |
| PB0 conflict | UART0_TXD vs Motor 4 PWM | [ ] _________ | [ ] |

**Recommendation:** Disable UART0 in defconfig unless teammate is using it.
It claims PA9 and PB0 at the NuttX level even if nothing uses the serial port.

```
# In nuttx-config/nsh/defconfig, change:
CONFIG_SAMV7_UART0=y  →  # CONFIG_SAMV7_UART0 is not set
```

---

## 9. GPIO / Status Signals

| Signal | Pin | This Repo | Teammate Code | Resolution |
|--------|-----|-----------|---------------|------------|
| Safety Button | PA9 | Input, pull-up, active-low | [ ] _________ | [ ] |
| Armed LED | PA23 | Output (LED0) | [ ] _________ | [ ] |
| Safety LED | PC9 | Output (LED1) | [ ] _________ | [ ] |
| nArmed | PA20 | Output, active-low | [ ] _________ | [ ] |

**Note on PA20:** Pin map doc flags SDRAM conflict. SDRAM is NOT enabled
in this repo's defconfig (`CONFIG_SAMV7_SDRAMCS` is not set). Verify
teammate also has SDRAM disabled.

---

## 10. ADC / Battery Monitoring

| Channel | Pin | Function | This Repo | Teammate Code | Resolution |
|---------|-----|----------|-----------|---------------|------------|
| AFEC0_AD0 | PD30 | Battery Voltage | CH 0 | [ ] _________ | [ ] |
| AFEC0_AD7 | PA18 | Battery Current | CH 7 | [ ] _________ | [ ] |

---

## 11. Storage Configuration

This repo now uses QSPI flash for params/caldata/dataman. Teammate's code
likely still uses SD card or LittleFS.

| Item | This Repo | Teammate Code | Resolution |
|------|-----------|---------------|------------|
| Param file | `/fs/mtd_params` (QSPI) | [ ] _________ | [ ] |
| Param backup | `/fs/microsd/parameters_backup.bson` | [ ] _________ | [ ] |
| Caldata | `/fs/mtd_caldata` (QSPI) | [ ] _________ | [ ] |
| Dataman | `/fs/mtd_waypoints` (QSPI) | [ ] _________ | [ ] |
| LittleFS | **Removed** | [ ] _________ | [ ] |
| CONFIG_BCH | **Enabled** | [ ] _________ | [ ] |

**Action:** When merging, teammate gets QSPI partition storage from this repo.

---

## 12. Enabled Sensor Drivers

Check which drivers are actually needed for the hardware physically connected.

| Driver | This Repo | Teammate has HW? | Action |
|--------|-----------|-------------------|--------|
| `icm20689` (SPI) | Enabled | [ ] Yes / No | [ ] |
| `icm45686` (SPI) | Enabled | [ ] Yes / No | [ ] |
| `bmi088` (SPI) | Enabled | [ ] Yes / No | [ ] |
| `bmi088_i2c` (I2C) | Enabled | [ ] Yes / No | [ ] |
| `ak09916` (I2C) | Enabled | [ ] Yes / No | [ ] |
| `bmm150` (I2C) | Enabled | [ ] Yes / No | [ ] |
| `dps310` (I2C) | Enabled | [ ] Yes / No | [ ] |
| `bmp388` (SPI) | Enabled | [ ] Yes / No | [ ] |
| `gps` (UART2) | Enabled | [ ] Yes / No | [ ] |
| `rc_input` (UART4) | Enabled | [ ] Yes / No | [ ] |

Drivers for missing hardware just log "no device on bus" — harmless but adds
boot noise. Can be disabled for cleaner boot.

---

## 13. NuttX ttyS Mapping

NuttX assigns `/dev/ttyS*` sequentially based on which UARTs are enabled in
defconfig (alphabetical: UART0, UART2, UART4, USART1). Changing which UARTs
are enabled shifts the numbering.

| Device | This Repo (UART0+UART2+UART4+USART1) | If UART0 disabled |
|--------|---------------------------------------|-------------------|
| `/dev/ttyS0` | UART0 (unused) | UART2 (GPS) |
| `/dev/ttyS1` | UART2 (GPS) | UART4 (RC) |
| `/dev/ttyS2` | UART4 (RC) | USART1 (console) |
| `/dev/ttyS3` | USART1 (console) | — |

**CRITICAL:** If teammate disables/enables different UARTs, the ttyS numbers
shift. `default.px4board` serial assignments (`GPS1`, `RC`, `TEL2`) must match.

**Question for teammate:** What does their ttyS mapping look like?
Run `ls /dev/ttyS*` on their board to verify.

---

## 14. Summary of Actions

### Must Resolve Before Merge

- [ ] **Motor numbering**: Agree on which physical motor is index 0 in `timer_io_channels[]`
- [ ] **IMU Click board**: Confirm chip, CS pin, DRDY pin, and EXT header
- [ ] **PD25 dual-use**: GPS RX vs IMU CS — cannot be both simultaneously
- [ ] **BMP388 interface**: SPI or I2C — match code to hardware
- [ ] **ttyS mapping**: Verify serial port assignments match UART enables

### Should Fix

- [ ] **Disable UART0**: Free PA9/PB0 from phantom UART0 claim
- [ ] **Remove stale GPIO_HSMCI0_CD on PD18**: RC input owns this pin
- [ ] **Fix pin map doc**: PC13 is Peripheral B (not D), motor numbering

### Nice to Have

- [ ] Disable unused sensor drivers for cleaner boot log
- [ ] Reconcile `rc.board_sensors` with actual Click boards connected

---

## 15. Files to Diff Between Codebases

Run these diffs to see all differences:

```bash
# Core board config
diff teammate/boards/microchip/samv71-xult-clickboards/src/board_config.h \
     thisrepo/boards/microchip/samv71-xult-clickboards/src/board_config.h

# PWM/motor config
diff teammate/boards/microchip/samv71-xult-clickboards/src/timer_config.cpp \
     thisrepo/boards/microchip/samv71-xult-clickboards/src/timer_config.cpp

# SPI device table
diff teammate/boards/microchip/samv71-xult-clickboards/src/spi.cpp \
     thisrepo/boards/microchip/samv71-xult-clickboards/src/spi.cpp

# NuttX config (UART enables, peripherals)
diff teammate/boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig \
     thisrepo/boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig

# PX4 board config (serial ports, drivers)
diff teammate/boards/microchip/samv71-xult-clickboards/default.px4board \
     thisrepo/boards/microchip/samv71-xult-clickboards/default.px4board

# Sensor startup
diff teammate/boards/microchip/samv71-xult-clickboards/init/rc.board_sensors \
     thisrepo/boards/microchip/samv71-xult-clickboards/init/rc.board_sensors

# Board init
diff teammate/boards/microchip/samv71-xult-clickboards/src/init.c \
     thisrepo/boards/microchip/samv71-xult-clickboards/src/init.c
```

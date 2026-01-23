# Engineering Task: GPS and RC Input Testing

**Assigned To:** Engineer
**Priority:** HIGH
**Estimated Effort:** 1-2 days
**Prerequisites:** SAMV71-XULT board, GPS module, RC receiver

---

## Executive Summary

This task involves testing and validating GPS and RC input functionality on the SAMV71-XULT PX4 port. Both drivers are enabled in the build but require parameter configuration and hardware connection verification.

---

## Part 1: Hardware Setup

### 1.1 Serial Port Mapping

| Device | NuttX Peripheral | Physical Pins | Function | Baud Rate |
|--------|------------------|---------------|----------|-----------|
| `/dev/ttyS0` | UART0 | PB0 (RX), PB1 (TX) | Available | 115200 |
| `/dev/ttyS1` | USART1 | PA21 (RX), PB04 (TX) | Console/VCOM | 115200 |
| `/dev/ttyS2` | UART2 | PD15 (RX), PD16 (TX) | **GPS** | 57600 |
| `/dev/ttyS3` | UART4 | PD18 (RX), PD19 (TX) | **RC Input** | 115200 |
| `/dev/ttyACM0` | USB CDC | USB connector | MAVLink | - |

**IMPORTANT:** UART4 appears as `ttyS3` (not `ttyS4`) because NuttX numbers ports sequentially.

### 1.2 Physical Pin Locations

All GPS and RC pins are on the **Arduino Communications Connector (J505)**:

```
SAMV71-XULT Board - J505 Connector
┌─────────────────────────────────────┐
│  J505 - Arduino Communications      │
│                                     │
│  Pin 1: NC                          │
│  Pin 2: NC                          │
│  Pin 3: PD18 (UART4 RX) ← RC INPUT  │
│  Pin 4: PD19 (UART4 TX)             │
│  Pin 5: PD15 (UART2 RX) ← GPS TX    │
│  Pin 6: PD16 (UART2 TX) → GPS RX    │
│  Pin 7: PB0  (UART0 RX)             │
│  Pin 8: PB1  (UART0 TX)             │
│  GND:   Multiple ground pins        │
│  VCC:   3.3V and 5V available       │
└─────────────────────────────────────┘
```

### 1.3 GPS Module Wiring

**Supported GPS Modules:** u-blox NEO-M8N, NEO-M9N, BN-220, or similar NMEA/UBX GPS

| GPS Module Pin | Connect To | SAMV71 Pin |
|----------------|------------|------------|
| TX | J505 Pin 5 | PD15 (UART2 RX) |
| RX | J505 Pin 6 | PD16 (UART2 TX) |
| VCC | 3.3V or 5V | Board power |
| GND | GND | Board ground |

**Wiring Diagram:**
```
GPS Module                    SAMV71-XULT (J505)
┌──────────┐                  ┌──────────┐
│  TX  ●───┼──────────────────┼─● Pin 5  │ (PD15 UART2 RX)
│  RX  ●───┼──────────────────┼─● Pin 6  │ (PD16 UART2 TX)
│  VCC ●───┼──────────────────┼─● 3.3V   │
│  GND ●───┼──────────────────┼─● GND    │
└──────────┘                  └──────────┘
```

### 1.4 RC Receiver Wiring

**Supported Protocols:** SBUS (inverted), CRSF, DSM/DSM2/DSMX, SUMD

| RC Receiver Pin | Connect To | SAMV71 Pin |
|-----------------|------------|------------|
| Signal/TX | J505 Pin 3 | PD18 (UART4 RX) |
| VCC | 5V | Board power |
| GND | GND | Board ground |

**SBUS Note:** SBUS signal is inverted. You may need an inverter circuit OR use a receiver with uninverted SBUS output.

**Wiring Diagram:**
```
RC Receiver                   SAMV71-XULT (J505)
┌──────────┐                  ┌──────────┐
│ Signal●──┼──────────────────┼─● Pin 3  │ (PD18 UART4 RX)
│  VCC ●───┼──────────────────┼─● 5V     │
│  GND ●───┼──────────────────┼─● GND    │
└──────────┘                  └──────────┘

For SBUS with inverter:
RC SBUS ──►[Inverter]──► Pin 3
```

---

## Part 2: Software Configuration

### 2.1 Verify Serial Devices

```bash
# List available serial devices
nsh> ls /dev/tty*

# Expected output:
# /dev/ttyACM0  (USB)
# /dev/ttyS0    (UART0)
# /dev/ttyS1    (USART1 - console)
# /dev/ttyS2    (UART2 - GPS)
# /dev/ttyS3    (UART4 - RC)
```

**If ttyS2 or ttyS3 is missing:** Check NuttX defconfig (see Section 5).

### 2.2 Configure GPS Parameters

```bash
# Enable GPS on ttyS2
nsh> param set GPS_1_CONFIG 201

# Set GPS baud rate (57600 for most u-blox modules)
nsh> param set SER_GPS1_BAUD 57600

# Optional: Set GPS protocol (0=Auto, 1=u-blox, 2=MTK, 5=NMEA)
nsh> param set GPS_1_PROTOCOL 1

# Save parameters
nsh> param save
```

### 2.3 Configure RC Parameters

```bash
# Enable RC input on ttyS3
nsh> param set RC_PORT_CONFIG 301

# Set RC protocol (if needed)
# 0=Auto, 1=SBUS, 2=DSM, 3=SUMD, 4=ST24, 5=CRSF
# Leave at 0 for auto-detection

# Save parameters
nsh> param save
```

### 2.4 Reboot to Apply

```bash
nsh> reboot
```

---

## Part 3: Testing Procedures

### 3.1 GPS Testing

#### Step 1: Verify Driver Started
```bash
nsh> gps status

# Expected output (if running):
# INFO  [gps] Main GPS:
# ...protocol: UBX
# ...port: /dev/ttyS2
# ...baudrate: 57600
```

**If "not running":** Start manually:
```bash
nsh> gps start -d /dev/ttyS2 -b 57600
```

#### Step 2: Check Raw Data Reception
```bash
# View raw serial data (Ctrl+C to stop)
nsh> cat /dev/ttyS2
```

**Expected:** NMEA sentences like:
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

**If no data:** Check wiring, GPS module power, antenna connection.

#### Step 3: Monitor GPS Topic
```bash
# Watch GPS data (updates every second)
nsh> listener sensor_gps

# Expected output:
# TOPIC: sensor_gps
#   timestamp: 12345678
#   lat: 48.1234567
#   lon: 11.5678901
#   alt: 545.4
#   satellites_used: 8
#   fix_type: 3
```

**fix_type values:**
- 0 = No fix
- 1 = Dead reckoning
- 2 = 2D fix
- 3 = 3D fix (required for flight)
- 4 = GNSS + dead reckoning
- 5 = Time only

#### Step 4: Verify Position
```bash
nsh> listener vehicle_gps_position
```

### 3.2 RC Input Testing

#### Step 1: Verify Driver Started
```bash
nsh> rc_input status

# Expected output (if running):
# RC input: valid
# Protocol: SBUS
# Channels: 16
# RSSI: 100
```

**If "not running":** Start manually:
```bash
nsh> rc_input start -d /dev/ttyS3
```

#### Step 2: Monitor RC Channels
```bash
# Watch RC channel values
nsh> listener input_rc

# Expected output:
# TOPIC: input_rc
#   timestamp: 12345678
#   channel_count: 16
#   values: [1500, 1500, 1000, 1500, 1000, 1000, ...]
#   rssi: 100
#   rc_lost: false
```

**Channel values:**
- ~1000 = Low/Left
- ~1500 = Center
- ~2000 = High/Right

#### Step 3: Verify Stick Movements
```bash
# Continuous monitoring
nsh> listener input_rc -n 100
```

Move transmitter sticks and verify:
- Channel 1 (Roll): Left/Right movement
- Channel 2 (Pitch): Forward/Back movement
- Channel 3 (Throttle): Up/Down movement
- Channel 4 (Yaw): Left/Right rotation

#### Step 4: Check Manual Control
```bash
nsh> listener manual_control_setpoint
```

---

## Part 4: Troubleshooting

### 4.1 GPS Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| No `/dev/ttyS2` | UART2 not enabled | Add `CONFIG_SAMV7_UART2=y` to defconfig |
| No data from GPS | TX/RX swapped | Swap wires on pins 5/6 |
| No data from GPS | Wrong baud rate | Try 9600, 38400, 115200 |
| `gps status` not running | GPS_1_CONFIG not set | `param set GPS_1_CONFIG 201` |
| No satellite fix | Indoor/no antenna | Move outdoors, check antenna |
| NMEA but no PX4 data | Wrong protocol | `param set GPS_1_PROTOCOL 0` (auto) |

### 4.2 RC Input Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| No `/dev/ttyS3` | UART4 not enabled | Add `CONFIG_SAMV7_UART4=y` to defconfig |
| No RC data | SBUS not inverted | Add inverter circuit |
| No RC data | Wrong pin | Verify Pin 3 (PD18) connection |
| `rc_input status` not running | RC_PORT_CONFIG not set | `param set RC_PORT_CONFIG 301` |
| rc_lost: true | Transmitter off/binding | Power on TX, check binding |
| Values stuck at 1500 | No signal variation | Move sticks, check TX batteries |

### 4.3 SBUS Inverter Circuit

If using SBUS (inverted signal), build a simple inverter:

```
SBUS Signal ───┬───[10kΩ]───► 3.3V
               │
               └───[NPN BC547]───► To PD18 (Pin 3)
                      │
                     GND

Or use dedicated SBUS inverter IC (e.g., 74HC04)
```

**Alternative:** Use receivers with uninverted output:
- FrSky with "SBUS Out" option
- TBS Crossfire (CRSF protocol - no inversion needed)
- Spektrum (DSM protocol - no inversion needed)

---

## Part 5: Code Modifications (If Needed)

### 5.1 NuttX Serial Configuration

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

```diff
# Verify these are enabled:
CONFIG_SAMV7_UART0=y
+CONFIG_SAMV7_UART2=y    # GPS
CONFIG_SAMV7_UART4=y    # RC (appears as ttyS3)
CONFIG_SAMV7_USART1=y   # Console
```

### 5.2 PX4 Board Configuration

**File:** `boards/microchip/samv71-xult-clickboards/default.px4board`

```cmake
# Serial port assignments
CONFIG_BOARD_SERIAL_GPS1="/dev/ttyS2"
CONFIG_BOARD_SERIAL_RC="/dev/ttyS3"

# Drivers (should already be enabled)
CONFIG_DRIVERS_GPS=y
CONFIG_DRIVERS_RC_INPUT=y
```

### 5.3 Default Parameters

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

Add these lines to auto-configure GPS and RC:

```bash
# GPS Configuration
param set-default GPS_1_CONFIG 201      # ttyS2
param set-default SER_GPS1_BAUD 57600   # Standard GPS baud

# RC Configuration
param set-default RC_PORT_CONFIG 301    # ttyS3
```

### 5.4 After Code Changes

```bash
# Rebuild firmware
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make microchip_samv71-xult-clickboards_default

# Flash to board
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.bin 0x00400000 verify reset exit"
```

---

## Part 6: Test Matrix

### 6.1 GPS Tests

| Test | Command | Expected Result | Pass/Fail |
|------|---------|-----------------|-----------|
| Device exists | `ls /dev/ttyS2` | File listed | [ ] |
| Driver starts | `gps status` | Shows running | [ ] |
| Raw data | `cat /dev/ttyS2` | NMEA sentences | [ ] |
| Topic data | `listener sensor_gps` | Valid coordinates | [ ] |
| Satellite fix | Check fix_type | fix_type >= 3 | [ ] |
| Position valid | `listener vehicle_gps_position` | lat/lon non-zero | [ ] |

### 6.2 RC Input Tests

| Test | Command | Expected Result | Pass/Fail |
|------|---------|-----------------|-----------|
| Device exists | `ls /dev/ttyS3` | File listed | [ ] |
| Driver starts | `rc_input status` | Shows valid | [ ] |
| Channel count | `listener input_rc` | channel_count > 0 | [ ] |
| Stick response | Move sticks | Values change | [ ] |
| RSSI valid | Check rssi field | rssi > 0 | [ ] |
| rc_lost false | Check rc_lost | false when TX on | [ ] |

---

## Part 7: Success Criteria

- [ ] GPS module receives satellite fix (fix_type >= 3)
- [ ] GPS position appears in `sensor_gps` topic
- [ ] RC receiver shows valid channel data
- [ ] All 4 main channels respond to stick movement
- [ ] No `rc_lost` warnings with transmitter powered on
- [ ] Both work simultaneously without conflicts
- [ ] Parameters persist across reboot

---

## Part 8: Hardware Recommendations

### 8.1 Tested GPS Modules
- u-blox NEO-M8N (recommended)
- u-blox NEO-M9N
- Beitian BN-220

### 8.2 Tested RC Systems
- FrSky X8R (SBUS - needs inverter)
- TBS Crossfire Nano (CRSF - no inverter needed)
- Spektrum DSMX (DSM - no inverter needed)

---

## Appendix: Quick Reference Commands

```bash
# === GPS ===
gps status                    # Check GPS driver
gps start -d /dev/ttyS2      # Manual start
listener sensor_gps           # Monitor GPS data
listener vehicle_gps_position # Monitor position

# === RC ===
rc_input status              # Check RC driver
rc_input start -d /dev/ttyS3 # Manual start
listener input_rc            # Monitor RC channels
listener manual_control_setpoint  # Monitor control input

# === Parameters ===
param show GPS_1_CONFIG      # Check GPS config
param show RC_PORT_CONFIG    # Check RC config
param set GPS_1_CONFIG 201   # Set GPS to ttyS2
param set RC_PORT_CONFIG 301 # Set RC to ttyS3
param save                   # Save to SD card

# === Debug ===
cat /dev/ttyS2              # Raw GPS data
dmesg | grep -i gps         # GPS debug messages
dmesg | grep -i rc          # RC debug messages
```

---

**Document Version:** 1.0
**Created:** January 2026
**Status:** Ready for Testing

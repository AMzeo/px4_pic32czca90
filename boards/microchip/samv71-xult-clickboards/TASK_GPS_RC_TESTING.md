# Engineering Task: GPS and RC Input Testing

**Assigned To:** Engineer
**Priority:** HIGH
**Prerequisites:** SAMV71-XULT board, GPS module, RC receiver
**Firmware Version:** Latest build with GPS/RC enabled
**Last Updated:** January 2026

---

## QUICK START (TL;DR)

### GPS Setup (5 minutes)
```
1. Wire GPS module:
   - GPS TX  → J505 Pin 5 (PD15)
   - GPS RX  → J505 Pin 6 (PD16)
   - GPS VCC → 3.3V
   - GPS GND → GND

2. Power on board, connect via USB serial console

3. Test:
   nsh> gps start -d /dev/ttyS2 -b 57600
   nsh> listener sensor_gps

4. Success = lat/lon values appear, fix_type >= 3
```

### RC Setup (5 minutes)
```
1. Wire RC receiver:
   - RC Signal → J505 Pin 3 (PD18)
   - RC VCC    → 5V
   - RC GND    → GND

   NOTE: SBUS needs signal inverter! Use CRSF/DSM to avoid this.

2. Power on transmitter, bind receiver

3. Test:
   nsh> rc_input start -d /dev/ttyS3
   nsh> listener input_rc

4. Success = channel values change when moving sticks (1000-2000 range)
```

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
nsh> param set RC_PORT_CONFIG 300

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
| `rc_input status` not running | RC_PORT_CONFIG not set | `param set RC_PORT_CONFIG 300` |
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

### 4.4 Debug Commands Deep Dive

#### Check What's Running
```bash
# List all running modules
nsh> top

# Check specific driver status
nsh> gps status
nsh> rc_input status

# View system messages (errors/warnings)
nsh> dmesg
```

#### Parameter Debugging
```bash
# Show all GPS-related parameters
nsh> param show GPS*

# Show all RC-related parameters
nsh> param show RC*

# Show serial port configuration
nsh> param show SER*

# Reset a parameter to default
nsh> param reset GPS_1_CONFIG
```

#### Serial Port Debugging
```bash
# List all serial devices
nsh> ls /dev/tty*

# Check if device exists and is accessible
nsh> ls -l /dev/ttyS2
nsh> ls -l /dev/ttyS3

# Read raw data from serial port (Ctrl+C to stop)
nsh> cat /dev/ttyS2    # GPS raw data
nsh> cat /dev/ttyS3    # RC raw data (may show garbage for binary protocols)
```

#### uORB Topic Debugging
```bash
# List all available topics
nsh> uorb status

# Monitor specific topics
nsh> listener sensor_gps -n 5           # 5 samples
nsh> listener input_rc -n 10            # 10 samples
nsh> listener vehicle_gps_position      # Processed GPS
nsh> listener manual_control_setpoint   # Processed RC
```

### 4.5 Expected Boot Messages

#### Normal Boot (No Hardware Connected)
```
INFO  [rc_input] valid device required
ERROR [rc_input] Task start failed (-1)
...
nsh> gps status
INFO  [gps] not running
```
**This is NORMAL** - drivers don't start without hardware.

#### Normal Boot (GPS Connected)
```
INFO  [gps] starting Main GPS...
INFO  [gps] Main GPS on /dev/ttyS2
...
nsh> gps status
INFO  [gps] Main GPS:
        protocol: UBX
        port: /dev/ttyS2
        baudrate: 57600
```

#### Normal Boot (RC Connected)
```
INFO  [rc_input] Starting RC input on /dev/ttyS3
INFO  [rc_input] SBUS input detected
...
nsh> rc_input status
INFO  [rc_input] RC input: valid
        Protocol: SBUS
        Channels: 16
```

### 4.6 Common Error Messages

| Error Message | Meaning | Solution |
|---------------|---------|----------|
| `valid device required` | No RC hardware detected | Connect RC receiver, check wiring |
| `gps not running` | GPS driver not started | Check GPS_1_CONFIG param, connect GPS |
| `rc_lost: true` | RC signal lost | Power on transmitter, check binding |
| `fix_type: 0` | No GPS satellite fix | Move outdoors, check antenna |
| `param 65535 invalid` | Parameter not found | Ignore - cosmetic issue |
| `Preflight Fail: Accel Sensor 0 missing` | No IMU connected | Expected without Click IMU boards |

### 4.7 Wiring Verification Steps

#### Step 1: Visual Inspection
- [ ] All wires securely connected
- [ ] No bent/broken pins
- [ ] Correct voltage (3.3V for GPS, 5V for RC)
- [ ] Common ground between all devices

#### Step 2: Continuity Test (with multimeter)
```
GPS:
  GPS TX ──── J505 Pin 5 (PD15) : Should show continuity
  GPS RX ──── J505 Pin 6 (PD16) : Should show continuity

RC:
  RC Signal ── J505 Pin 3 (PD18) : Should show continuity
```

#### Step 3: Voltage Test
```
With board powered:
  J505 3.3V pin : Should read 3.3V ± 0.1V
  J505 5V pin   : Should read 5.0V ± 0.2V
  J505 GND      : Should read 0V (reference)
```

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
param set-default RC_PORT_CONFIG 300    # ttyS3
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
param set RC_PORT_CONFIG 300 # Set RC to ttyS3
param save                   # Save to SD card

# === Debug ===
cat /dev/ttyS2              # Raw GPS data
dmesg | grep -i gps         # GPS debug messages
dmesg | grep -i rc          # RC debug messages
```

---

## Part 9: Reporting Results

### 9.1 Test Report Template

Please fill out and submit after testing:

```
=== GPS/RC Test Report ===
Date: _______________
Tester: _______________
Board Serial: _______________

GPS MODULE:
  Model: _______________
  Result: [ ] PASS  [ ] FAIL  [ ] NOT TESTED
  Notes: _______________

RC RECEIVER:
  Model: _______________
  Protocol: [ ] SBUS  [ ] CRSF  [ ] DSM  [ ] Other: ____
  Inverter Used: [ ] Yes  [ ] No
  Result: [ ] PASS  [ ] FAIL  [ ] NOT TESTED
  Notes: _______________

ISSUES ENCOUNTERED:
  _______________
  _______________

CONSOLE OUTPUT (attach dmesg if errors):
  _______________
```

### 9.2 What to Capture if Something Fails

1. **Full boot log** - Copy everything from power-on to NSH prompt
2. **dmesg output** - Run `dmesg` and copy all output
3. **Parameter dump** - Run `param show GPS* RC* SER*`
4. **Photo of wiring** - Clear photo showing all connections

### 9.3 Known Working Configurations

| GPS Module | Baud Rate | Protocol | Status |
|------------|-----------|----------|--------|
| u-blox NEO-M8N | 57600 | UBX | Untested |
| Beitian BN-220 | 9600 | NMEA | Untested |

| RC Receiver | Protocol | Inverter | Status |
|-------------|----------|----------|--------|
| FrSky X8R | SBUS | Yes | Untested |
| TBS Crossfire | CRSF | No | Untested |
| Spektrum AR620 | DSM2 | No | Untested |

---

## Part 10: Parameter Reference

### GPS Parameters
| Parameter | Value | Description |
|-----------|-------|-------------|
| `GPS_1_CONFIG` | 201 | Serial port (201 = GPS1 = /dev/ttyS2) |
| `SER_GPS1_BAUD` | 57600 | Baud rate (0=auto, or specific rate) |
| `GPS_1_PROTOCOL` | 0 | Protocol (0=auto, 1=UBX, 2=MTK, 5=NMEA) |

### RC Parameters
| Parameter | Value | Description |
|-----------|-------|-------------|
| `RC_PORT_CONFIG` | 300 | Serial port (300 = RC = /dev/ttyS3) |
| `SER_RC_BAUD` | 0 | Baud rate (0=auto for most protocols) |

### Serial Port Mapping (This Board)
| Parameter Value | Port Name | Device | Physical Pins |
|-----------------|-----------|--------|---------------|
| 101 | TELEM1 | /dev/ttyACM0 | USB |
| 102 | TELEM2 | /dev/ttyS1 | PA21/PB04 |
| 201 | GPS1 | /dev/ttyS2 | PD15/PD16 |
| 300 | RC | /dev/ttyS3 | PD18/PD19 |

---

**Document Version:** 1.1
**Created:** January 2026
**Last Updated:** January 2026
**Status:** Ready for Testing

# HITL Testing Guide — SAMV71-XULT + jMAVSim

Hardware-In-The-Loop (HITL) simulation using jMAVSim with the SAMV71-XULT board.

## Prerequisites

### Hardware
- Microchip SAMV71-XULT board
- 2x micro-USB cables
- Host PC running Linux

### Software
- PX4 Autopilot (branch `samv7-custom`)
- jMAVSim (included in PX4 source tree under `Tools/simulation/jmavsim`)
- Java JDK 11+ (for jMAVSim)
- OpenOCD (for flashing)
- picocom or minicom (for NSH console)

## Physical Connections

```
Host PC
  |
  |--- USB cable 1 --> J501 (DEBUG USB) = NSH console (/dev/ttyACMx)
  |
  |--- USB cable 2 --> J500 (TARGET USB) = MAVLink serial (/dev/ttyACMx)
```

- **J501** (DEBUG USB): Provides the NSH shell console via USB CDC/ACM
- **J500** (TARGET USB): Provides the MAVLink serial link for jMAVSim

After connecting both cables, you should see two `/dev/ttyACM*` devices on the host.
Use `dmesg | tail` to identify which is which — J501 typically enumerates first.

## Build and Flash

### 1. Set airframe to HITL Quadcopter

On the NSH console (or via saved params):

```
param set SYS_AUTOSTART 1001
param set COM_DISARM_PRFLT -1
param save
reboot
```

- `SYS_AUTOSTART 1001`: HIL Quadcopter airframe (board default is `4001` for real quad)
- `COM_DISARM_PRFLT -1`: Disables preflight auto-disarm timeout, which triggers before
  EKF2 converges in HITL and causes immediate disarm after arming

Both persist across reboots via QSPI flash.

### 2. Build firmware

```bash
cd PX4-Autopilot-Private
make clean
make microchip_samv71-xult-clickboards_default
```

### 3. Flash via OpenOCD

```bash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/samv71-xult-clickboards_default.elf verify reset exit"
```

## Running HITL

### 1. Open NSH Console

```bash
picocom -b 115200 /dev/ttyACM0
```

(Use whichever ttyACM device corresponds to J501.)

### 2. Verify Boot

After reset/power-on, you should see in NSH:

```
sercon: Successfully registered the CDC/ACM serial driver
...
INFO: Board mavlink config - USB CDC/ACM enabled
...
Starting MAVLink on /dev/ttyACM0
```

If you see `Device /dev/ttyACM0 does not exist`, the `sercon` call in `rc.board_mavlink`
is not working — check that you have the latest firmware.

### 3. Start jMAVSim

On the host PC, in a separate terminal:

```bash
cd PX4-Autopilot-Private/Tools/simulation/jmavsim
./jmavsim_run.sh -d /dev/ttyACM1 -b 921600 -r 250
```

- `-d /dev/ttyACM1`: Serial device for the MAVLink USB (J500 — adjust if needed)
- `-b 921600`: Baud rate
- `-r 250`: Sim update rate in Hz

> **Tip**: If jMAVSim can't open the port, check that no other process (QGC, mavlink-router)
> is using it. Use `lsof /dev/ttyACM1` to check.

### 4. Wait for EKF2 Convergence

After jMAVSim connects, wait ~10-15 seconds for EKF2 to converge. The jMAVSim window
should show a stationary quadcopter on the ground.

### 5. Arm and Fly

On the NSH console:

```
commander takeoff
```

The quad should take off in the jMAVSim window and hover at the default takeoff altitude.

## Flight Commands (NSH)

| Command | Description |
|---------|-------------|
| `commander takeoff` | Arm and take off to default altitude |
| `commander land` | Land at current position |
| `commander disarm` | Disarm motors (use after landing) |
| `commander mode manual` | Switch to manual mode |
| `commander mode posctl` | Switch to position control mode |
| `commander mode auto:loiter` | Loiter at current position |
| `listener vehicle_local_position` | Check EKF2 position estimate |
| `listener vehicle_status` | Check arming/flight state |

## Troubleshooting

### "Device /dev/ttyACM0 does not exist"
The USB CDC device wasn't created before MAVLink tried to start. Ensure your firmware
includes the `sercon` call in `rc.board_mavlink`. As a manual workaround on older firmware:
```
sercon
mavlink start -d /dev/ttyACM0 -b 921600
```

### Board arms then immediately disarms
The `COM_DISARM_PRFLT` timeout triggers before EKF2 converges. Make sure you set it
to `-1` during HITL setup (see step 1 above):
```
param set COM_DISARM_PRFLT -1
param save
reboot
```

### jMAVSim shows "No connection"
- Verify the correct `/dev/ttyACM*` device (use `dmesg | tail` after plugging in J500)
- Check baud rate matches (`-b 921600`)
- Ensure MAVLink is actually running: `mavlink status` on NSH

### No sensor data in HITL (EKF2 never converges)
- Verify `SYS_HAS_BARO 1` and `SYS_HAS_MAG 1` are set (check with `param show SYS_HAS_*`)
- These enable the VehicleAirData and VehicleMagnetometer modules that process sim sensor data

### Duplicate MAVLink instances / port conflict
If `SYS_USB_AUTO` is set to `2`, cdcacm_autostart will try to start a second MAVLink on
ttyACM0. Set `SYS_USB_AUTO 0` so only `rc.serial` manages MAVLink.

## Key Parameters for HITL

| Parameter | Value | Set by | Purpose |
|-----------|-------|--------|---------|
| `SYS_AUTOSTART` | `1001` | **Manual** | HIL Quadcopter airframe |
| `COM_DISARM_PRFLT` | `-1` | **Manual** | Disable preflight auto-disarm |
| `SYS_USB_AUTO` | `0` | Board default | Prevent duplicate MAVLink from cdcacm |
| `SYS_HAS_BARO` | `1` | Board default | Enable baro processing pipeline |
| `SYS_HAS_MAG` | `1` | Board default | Enable mag processing pipeline |
| `CBRK_FLIGHTTERM` | `121212` | Board default | Disable flight termination (required for HITL) |
| `MAV_0_CONFIG` | `101` | Board default | MAVLink on ttyACM0 |
| `MAV_0_RATE` | `57600` | Board default | MAVLink data rate |

## Switching Back to Real Flight

To switch back from HITL to real hardware flight:

```
param set SYS_AUTOSTART 4001
param reset COM_DISARM_PRFLT
param save
reboot
```

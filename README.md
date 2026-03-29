# PX4 Autopilot — PIC32CZ CA90 Port

This branch contains PX4 Autopilot ported to the **Microchip PIC32CZ CA90 Curiosity** development board.

## Port Status

| Component | Status |
|-----------|--------|
| PX4 Boot | ✅ Working |
| NSH Shell | ✅ Working (UART1 / PKOB4 USB) |
| QSPI Flash | ✅ Working (SST26VF032B 4MB) |
| Parameter Storage | ✅ Working (/fs/mtd_params) |
| Dataman | ✅ Working (/fs/mtd_waypoints) |
| USB CDC-ACM | ✅ Working (TARGET USB) |
| MAVLink | ✅ Working (/dev/ttyACM1) |
| PWM Output | ✅ Working (4 channels, 400Hz) |
| EKF2 | ✅ Working (HITL verified) |
| HITL Simulation | ✅ Working (jMAVSim) |
| SPI Bus | 🔄 Configured (sensors not connected yet) |
| I2C Bus | 🔄 Configured (sensors not connected yet) |
| ICM20689 IMU | ⬜ Not tested (real sensor) |
| BMP388 Barometer | ⬜ Not tested (real sensor) |
| RC Receiver | ⬜ Not connected yet |
| DShot | ⬜ Not started |
| CAN/UAVCAN | ⬜ Not started |

## Quick Start

See [QUICKSTART.md](QUICKSTART.md) for full clone, build, flash, and HITL setup instructions.

## Build Stats

| Region | Used | Total | Usage |
|--------|------|-------|-------|
| Flash | 1,352,048 B | 2 MB | 64.47% |
| SRAM | 53,452 B | 448 KB | 11.65% |
| nocache | 5 KB | 64 KB | 7.81% |

## NuttX Submodule

This port uses a forked NuttX with PIC32CZ CA90 support:
- **Fork:** [Vigneshjr1/NuttX](https://github.com/Vigneshjr1/NuttX)
- **Branch:** `pic32cz-ca70-port`

## Hardware

- **Board:** PIC32CZ CA90 Curiosity (144-pin)
- **QSPI Flash:** SST26VF032B 4MB
- **Console:** J700 PKOB4 USB → `/dev/ttyACM0` (115200 baud)
- **MAVLink:** J200 TARGET USB → `/dev/ttyACM1`

## Documentation

| Document | Description |
|----------|-------------|
| [QUICKSTART.md](QUICKSTART.md) | Clone, build, flash and run |
| [Port Status](docs/pic32czca90_px4_port_status.md) | Detailed port status and notes |

---

*Branch: pic32cz-ca70-port*

# PX4 Autopilot — PIC32CZ CA90 Port

This branch ports PX4 Autopilot to the **Microchip PIC32CZ CA90 Curiosity Ultra** development board (EV16W43A).

## Port Status

| Component | Status | Notes |
|-----------|--------|-------|
| Build | Working | ~911 KB flash (10.9% of 8 MB) |
| Clock tree | Working | PLL0 → 300 MHz, MCLK/2 → 150 MHz CPU, GCLK1 → 150 MHz SERCOM1 |
| SERCOM1 console | Working | PC04/PC07, 115200 baud via PKOB4 J700 |
| NSH shell | Working | Verified on hardware, full keyboard input |
| LED driver | Working | PB21/PB22 active-LOW, verified on hardware |
| DMA | Not started | |
| I2C / SPI | Not started | |
| USB CDC-ACM | Not started | |
| TCC PWM | Not started | |
| ADC | Not started | |
| CAN-FD | Not started | |

## Hardware

| Item | Value |
|------|-------|
| Board | PIC32CZ CA90 Curiosity Ultra (EV16W43A) |
| MCU | PIC32CZ CA90 (Cortex-M7, 300 MHz) |
| BFM (Boot Flash) | 128 KB at 0x08000000 |
| PFM (Program Flash) | 8 MB at 0x0C000000 |
| SRAM | 832 KB at 0x20020000 |
| CA80 vs CA90 | Identical; CA90 adds HSM |

## USB Connectors

| Connector | Purpose | Linux device |
|-----------|---------|--------------|
| J700 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` |
| J200 — Target USB | MAVLink CDC/ACM (when USB implemented) | `/dev/ttyACM1` |

## Build & Flash

See [QUICKSTART.md](QUICKSTART.md) for full instructions.

```bash
make microchip_czca90curiosity_default
```

## NuttX Submodule

| Submodule | Repository | Branch |
|-----------|-----------|--------|
| `platforms/nuttx/NuttX/nuttx` | [AMzeo/nuttx](https://github.com/AMzeo/nuttx) | `pic32czca90-bringup` |
| `platforms/nuttx/NuttX/apps` | [PX4/NuttX-apps](https://github.com/PX4/NuttX-apps) | `px4_firmware_nuttx-10.3.0+` |

## Documentation

| Document | Description |
|----------|-------------|
| [QUICKSTART.md](QUICKSTART.md) | Build, flash, and connect |
| [CLAUDE.md](CLAUDE.md) | Architecture, clock tree, bug history, pending tasks |
| [docs/pic32czca90_port_status.md](docs/pic32czca90_port_status.md) | Detailed port status |

---

*Branch: pic32czca90-port*

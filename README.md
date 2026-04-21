# PX4 Autopilot — PIC32CZ CA90 Port

This branch ports PX4 Autopilot to the **Microchip PIC32CZ CA90 Curiosity Ultra** development board (EV16W43A).

## Port Status

| Component | Status | Notes |
|-----------|--------|-------|
| Build | ✅ Working | ~911 KB flash (10.9% of 8 MB), ~46 KB SRAM |
| Clock tree | ✅ Working | PLL0 → 300 MHz CPU; GCLK1 → 150 MHz; GCLK3 → 32 kHz |
| SERCOM1 console | ✅ Working | PC04/PC07, 115200 baud, J700 PKOB4 USB |
| NSH shell | ✅ Working | Verified on hardware, stable 3+ days |
| Interrupt system | ✅ Working | 222-IRQ table, all 16 Cortex-M7 exception vectors |
| LED driver | ✅ Working | PB21/PB22 active-LOW; LED1 blinks 1 Hz = scheduler alive |
| HRT (TCC0) | ✅ Working | 150 MHz free-running, 64-bit µs counter, compare-match ISR |
| PX4 scheduler | ✅ Working | commander, sensors, EKF2, navigator all firing |
| I-cache / D-cache | ✅ Working | Write-through D-cache; MPU 16 regions |
| SD card logging | 🔲 Next | Stage 1.1 — SDMMC PIO mode, no DMA needed |
| SQI param storage | 🔲 Next | Stage 1.2 — SST26VF032BAT on SQI1 (own BD-DMA) |
| System DMA | 🔲 Pending | Stage 2.1 |
| SPI / IMU | 🔲 Pending | Stage 3.1 — SERCOM3, ICM-42688-P |
| I2C / mag / baro | 🔲 Pending | Stage 3.2 — SERCOM5, IST8310, BMP388 |
| PWM outputs | 🔲 Pending | Stage 4.1 — TCC1/TCC2 |
| USBHS (MAVLink) | 🔲 Pending | Stage 5.1 — needed before first hover |
| CAN-FD | 🔲 Pending | Stage 9.1 |

## Hardware

| Item | Value |
|------|-------|
| Board | PIC32CZ CA90 Curiosity Ultra (EV16W43A) |
| MCU | PIC32CZ8110CA90208 — Cortex-M7 @ 300 MHz |
| BFM (Boot Flash) | 128 KB at 0x08000000 — vector table lives here |
| PFM (Program Flash) | 8 MB at 0x0C000000 — application code |
| SRAM | 832 KB at 0x20020000 |
| DMA nocache region | 64 KB at 0x200F0000 (linker-reserved) |
| CA80 vs CA90 | Identical peripherals; CA90 adds HSM only |

## USB Connectors

| Connector | Purpose | Linux device |
|-----------|---------|--------------|
| J700 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` |
| J200 — Target USB | MAVLink / CDC-ACM (Stage 5.1 USBHS) | `/dev/ttyACM1` |

## Sensor Bus Pins

| Interface | SERCOM | Pins | Connector |
|-----------|--------|------|-----------|
| SPI MOSI/MISO/SCK/CS | SERCOM3 | PC12/PC15/PC13/PC14 | EXT2 pins 16/17/18/15 |
| I2C SDA/SCL | SERCOM5 | PC25/PC26 | EXT2 pins 11/12 |
| IMU DRDY | — (GPIO) | PA8 | MikroBUS pin 15 |
| CAN3 | CAN3 | PD13/PC29 | J701 (ATA6561) |
| CAN4 | CAN4 | PA31/PA30 | J702 (ATA6561) |

## Build & Flash

```bash
# Build
make microchip_czca90curiosity_default

# Generate Intel HEX for MPLAB IPE / PKOB4
arm-none-eabi-objcopy -O ihex \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.hex

# Open NSH console
minicom -D /dev/ttyACM0 -b 115200
```

Flash with MPLAB X IDE or MPLAB IPE using the `.hex` file and the on-board PKOB4 programmer (J700).

See [QUICKSTART.md](QUICKSTART.md) for full instructions.

## Expected Boot Output

```
ABDE
NuttShell (NSH) NuttX-12.x
nsh>
```

LED0 = steady ON after boot. LED1 = 1 Hz blink (scheduler alive). LED1 frozen = scheduler stalled.

## NuttX Submodule

| Submodule | Repository | Branch |
|-----------|-----------|--------|
| `platforms/nuttx/NuttX/nuttx` | [AMzeo/nuttx](https://github.com/AMzeo/nuttx) | `pic32czca90-bringup` |
| `platforms/nuttx/NuttX/apps` | [PX4/NuttX-apps](https://github.com/PX4/NuttX-apps) | `px4_firmware_nuttx-10.3.0+` |

## Documentation

| Document | Description |
|----------|-------------|
| [QUICKSTART.md](QUICKSTART.md) | Build, flash, and connect |
| [CLAUDE.md](CLAUDE.md) | Architecture, clock tree, key bugs fixed, pending driver work |
| [docs/tasks.md](docs/tasks.md) | Full task board — all stages ordered by development priority |
| [docs/sqi_filesystem.md](docs/sqi_filesystem.md) | NuttX MTD stack for SQI flash (Stage 1.2 implementation guide) |

---

*Branch: `pic32czca90-port`*

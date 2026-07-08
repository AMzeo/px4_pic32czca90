# PX4 Autopilot — PIC32CZ CA90 Port

This branch ports PX4 Autopilot to the **Microchip PIC32CZ CA90 Curiosity Ultra** development board (EV16W43A).

## Port Status

| Component | Status | Notes |
|-----------|--------|-------|
| Build | ✅ Working | ~911 KB flash (10.9% of 8 MB), ~46 KB SRAM (5.5%) |
| Clock tree | ✅ Working | PLL0 → 300 MHz CPU; GCLK1 → 150 MHz; GCLK3 → 32 kHz |
| SERCOM1 console | ✅ Working | PC04/PC07, 115200 baud, J200 PKOB4 USB |
| NSH shell | ✅ Working | Verified on hardware, stable 3+ days |
| Interrupt system | ✅ Working | 222-IRQ table, all 16 Cortex-M7 exception vectors |
| LED driver | ✅ Working | PB21/PB22 active-LOW; LED1 blinks 1 Hz = scheduler alive |
| HRT (TCC0) | ✅ Working | 150 MHz free-running, 64-bit µs counter, compare-match ISR |
| PX4 scheduler | ✅ Working | commander, sensors, EKF2, navigator all firing |
| I-cache / D-cache | ✅ Working | Write-through D-cache; MPU 16 regions |
| SQI param storage | ✅ Working | SST26VF032BAT on SQI1 — BD-DMA, XIP reads, D-cache safe |
| SD card logging | ✅ Working | SDMMC1 ADMA2 mode; 0 dropouts |
| System DMA | ✅ Working | 16-channel DMA controller; SPI + I2C offload |
| SPI / IMU | ✅ Working | SERCOM3 + DMA, ICM45686 @ 800 Hz |
| I2C / mag+accel | ✅ Working | SERCOM5 + DMA RX, BMI088 + BMM150 |
| EIC (ext. interrupts) | ✅ Built | 16-channel async EIC + gpiosetevent; no active consumer |
| PWM outputs | ✅ Working | TCC1 (PB10/PB11) + TCC7 (PA22/PA23) = 4ch @ 403 Hz |
| DSU / chip ID | ✅ Working | `ver all` shows silicon rev + unique PX4GUID |
| USB HS (MAVLink) | ✅ Working | USBHS0, 480 Mbps, QGC connected, txerr=0, bidirectional |
| ADC / Battery | 🔲 Pending | Needed for arming |
| RC Input (SBUS) | 🔲 Pending | Needs UART + rc_input module |
| Watchdog | 🔲 Pending | WDT driver needed |
| BMP388 Barometer | 🔲 Pending | I2C driver ready, sensor not connected on eval board |
| Ethernet (GMAC) | 🔲 Pending | Required for custom board |
| CAN-FD (MCAN) | 🔲 Pending | MCAN3/MCAN4 with ATA6561; required for custom board |

## Hardware

| Item | Value |
|------|-------|
| Board | PIC32CZ CA90 Curiosity Ultra (EV16W43A) |
| MCU | PIC32CZ8110CA90208 — Cortex-M7 @ 300 MHz |
| BFM (Boot Flash) | 128 KB at 0x08000000 — vector table lives here |
| PFM (Program Flash) | 8 MB at 0x0C000000 — application code |
| SRAM | 832 KB at 0x20020000 |
| DMA nocache region | 64 KB at 0x200F0000 (linker-reserved) |

## USB Connectors

| Connector | Purpose | Host device |
|-----------|---------|-------------|
| J200 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` (Linux) |
| J102 — Target USB | MAVLink CDC-ACM, QGC (USBHS0, 480 Mbps) | COM port (Windows) |

## Peripheral Pins

| Interface | Instance | Pins | Connector | Function |
|-----------|----------|------|-----------|----------|
| SPI MOSI/MISO/SCK/CS | SERCOM3 | PC12/PC15/PC13/PC14 | EXT2 pins 16/17/18/15 | ICM45686 (6DOF IMU) |
| I2C SDA/SCL | SERCOM5 | PC25/PC26 | EXT2 pins 11/12 | BMI088 + BMM150 |
| PWM Motor 1-2 | TCC1 | PB10/PB11 | EXT1 pins 7/8 | 403 Hz ESC PWM |
| PWM Motor 3-4 | TCC7 | PA22/PA23 | EXT2 pins 7/8 | 403 Hz ESC PWM |
| SD Card | SDMMC1 | PC30/PG03/PC31/PG00-02/PC28 | On-board microSD | Flight logs |
| Param Flash | SQI1 | PC30/PC31/PG00-03 | On-board SST26 | Params (dedicated) |
| USB HS | USBHS0 | Internal PHY | J102 | MAVLink CDC-ACM |
| CAN3 | CAN3 | PD13/PC29 | J701 (ATA6561) | DroneCAN |
| CAN4 | CAN4 | PA31/PA30 | J702 (ATA6561) | DroneCAN |

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

Flash with MPLAB X IDE or MPLAB IPE using the `.hex` file and the on-board PKOB4 programmer (J200).

See [QUICKSTART.md](QUICKSTART.md) for full instructions.

## Build Stats

| Region | Used | Available | % |
|--------|------|-----------|---|
| Flash (PFM) | ~911 KB | 8 MB | 10.9% |
| SRAM | ~46 KB | 832 KB | 5.5% |

## Expected Boot Output

```
ABDE
NuttShell (NSH) NuttX-12.x
nsh>
```

LED0 = steady ON after boot. LED1 = 1 Hz blink (scheduler alive). LED1 frozen = scheduler stalled.

## NuttX Submodule

| Submodule | Repository | Branch | Commit |
|-----------|-----------|--------|--------|
| `platforms/nuttx/NuttX/nuttx` | [AMzeo/nuttx](https://github.com/AMzeo/nuttx) | `pic32czca90-bringup` | `72583fc` |
| `platforms/nuttx/NuttX/apps` | [PX4/NuttX-apps](https://github.com/PX4/NuttX-apps) | `px4_firmware_nuttx-10.3.0+` | `e37940d` |

The NuttX fork contains the full PIC32CZ CA90 chip support: clock, GPIO, SERCOM (UART/SPI/I2C), TCC, USBHS, SQI, SDMMC, DMA, EIC, and IRQ table.

## Pending Development

Priority order for reaching first flight:

1. **SD card on SDMMC0/EXT1** — eliminate SQI1/SDMMC1 pin sharing
2. **Final sensors** — match CA70/SAMV71 custom board (ICM45686 + BMM150 + BMP388)
3. **ADC / Battery monitoring** — required for arming
4. **RC Input (SBUS)** — UART + rc_input module
5. **Watchdog** — WDT driver
6. **Ethernet (GMAC)** — required for custom board
7. **CAN-FD (MCAN)** — GPS/ESC telemetry on custom board
8. **Additional UART/I2C** — GPS, rangefinder, telemetry radio

## Comparison to CA70 Port

Reference: [PX4-Autopilot-Private_Rathi/ca70](https://github.com/RATHI16/PX4-Autopilot-Private_Rathi/tree/ca70)

| Feature | CA70 | CA90 |
|---------|------|------|
| USB | FS (12 Mbps) | **HS (480 Mbps)** |
| QSPI/SQI | SST26 3-partition | SQI1 BD-DMA + XIP |
| BMP388 barometer | Working | Pending (I2C ready) |
| board_adc (battery) | Working | Pending (ADC driver needed) |
| RC Input (SBUS) | Working | Pending (UART needed) |
| Watchdog | Enabled | Pending |
| EKF2/HITL | Verified | Not yet tested |
| Ethernet | N/A (no GMAC) | Pending (CA90 has GMAC) |
| CAN-FD | Not started | Pending (CA90 has 6x MCAN) |

## Documentation

| Document | Description |
|----------|-------------|
| [QUICKSTART.md](QUICKSTART.md) | Build, flash, and connect |
| [CLAUDE.md](CLAUDE.md) | Architecture, clock tree, key bugs fixed, pending driver work |

---

*Branch: `pic32czca90-port`*

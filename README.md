# PX4 Autopilot — PIC32CZ CA90 Port

This branch ports PX4 Autopilot to the **Microchip PIC32CZ CA90 Curiosity Ultra** development board (EV16W43A).

## Port Status

| Component | Status | Notes |
|-----------|--------|-------|
| Build | ✅ Working | ~1299 KB flash (15.5% of 8 MB), ~51 KB SRAM (5.5%) |
| Clock tree | ✅ Working | PLL0 → 300 MHz CPU; GCLK1 → 150 MHz; GCLK2 → 100 MHz; GCLK3 → 32 kHz |
| SERCOM1 console | ✅ Working | PC04/PC07, 115200 baud, J200 PKOB4 USB |
| NSH shell | ✅ Working | Verified on hardware, stable |
| Interrupt system | ✅ Working | 222-IRQ table, all 16 Cortex-M7 exception vectors |
| LED driver | ✅ Working | PB21/PB22 active-LOW; LED1 blinks 1 Hz = scheduler alive |
| HRT (TCC0) | ✅ Working | 150 MHz free-running, 64-bit µs counter, compare-match ISR |
| PX4 scheduler | ✅ Working | commander, sensors, EKF2, navigator all firing |
| I-cache / D-cache | ✅ Working | Write-through D-cache; MPU 16 regions |
| SQI param storage | ✅ Working | SST26VF032BAT on SQI1 — BD-DMA, XIP reads, D-cache safe |
| SD card logging | ✅ Working | SDMMC0 ADMA2 4-bit mode; 0 dropouts; /fs/microsd |
| System DMA | ✅ Working | BD-based DMA; SPI + I2C offload |
| SPI / IMU | ✅ Working | SERCOM3 + DMA, ICM45686 @ 800 Hz, 0 bad transfers |
| I2C / mag+accel | ✅ Working | SERCOM5 + DMA RX, BMI088 + BMM150, 1.6M+ completions |
| EIC (ext. interrupts) | ✅ Built | 16-channel async EIC + gpiosetevent; no active consumer yet |
| PWM outputs | ✅ Working | TCC1 (PB10/PB11) + TCC7 (PA22/PA23) = 4ch @ 400 Hz |
| DSU / chip ID | ✅ Working | `ver all` shows silicon rev + unique PX4GUID |
| USB HS (MAVLink) | ✅ Working | USBHS0, 480 Mbps, QGC connected, txerr=0, bidirectional |
| GPS (SERCOM0) | ✅ Working | /dev/ttyS1, u-blox UBX, 115200 baud, 1594 B/s, 9.94 Hz |
| Telemetry (SERCOM4) | ✅ Working | /dev/ttyS2, SiK radio MAVLink, 57600 baud |
| RC Input (SERCOM7) | ✅ Working | /dev/ttyS3, SBUS 100000 8E2, hardware RXINV in CTRLA |
| ADC / Battery | 🔲 Pending | Needed for arming checks |
| Watchdog | 🔲 Pending | WDT driver needed |
| PROGMEM crash log | 🔲 Pending | Crash dump across reboot |
| CAN-FD (MCAN) | 🔲 Pending | MCAN3/MCAN4 with ATA6561 transceivers on-board |
| Ethernet (GMAC) | 🔲 Pending | For custom production board |

## Hardware

| Item | Value |
|------|-------|
| Board | PIC32CZ CA90 Curiosity Ultra (EV16W43A) |
| MCU | PIC32CZ8110CA90208 — Cortex-M7 @ 300 MHz |
| BFM (Boot Flash) | 128 KB at 0x08000000 — vector table lives here |
| PFM (Program Flash) | 8 MB at 0x0C000000 — application code |
| SRAM | 960 KB at 0x20020000 |
| DMA nocache region | 64 KB at 0x200F0000 (linker-reserved) |

## USB Connectors

| Connector | Purpose | Host device |
|-----------|---------|-------------|
| J200 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` (Linux) |
| J102 — Target USB | MAVLink CDC-ACM, QGC (USBHS0, 480 Mbps) | COM port (Windows) |

## Peripheral Pins

| Interface | Instance | Pins | Connector | Function |
|-----------|----------|------|-----------|----------|
| GPS TX/RX | SERCOM0 | PC00/PC01 | EXT header | u-blox GPS, 115200 baud |
| SPI MOSI/MISO/SCK/CS | SERCOM3 | PC12/PC15/PC13/PC14 | EXT2 pins 16/17/18/15 | ICM45686 (6DOF IMU) |
| I2C SDA/SCL | SERCOM5 | PC25/PC26 | EXT2 pins 11/12 | BMI088 + BMM150 |
| Telemetry TX/RX | SERCOM4 | PC21/PC22 | EXT2 pins 13/14 | SiK radio MAVLink |
| RC Input (SBUS) | SERCOM7 | PD14/PD15 | EXT header | SBUS, hardware RXINV |
| PWM Motor 1-2 | TCC1 | PB10/PB11 | EXT1 pins 7/8 | 400 Hz ESC PWM |
| PWM Motor 3-4 | TCC7 | PA22/PA23 | EXT2 pins 7/8 | 400 Hz ESC PWM |
| SD Card | SDMMC0 | PC08-PC15 | On-board microSD | Flight logs |
| Param Flash | SQI1 | PC30/PC31/PG00-03 | On-board SST26 | Params / caldata |
| USB HS | USBHS0 | Internal PHY | J102 | MAVLink CDC-ACM |
| CAN3 | MCAN3 | — | J701 (ATA6561) | DroneCAN (pending) |
| CAN4 | MCAN4 | — | J702 (ATA6561) | DroneCAN (pending) |

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

After every reflash from fresh firmware:
```
param reset_all; param save; reboot
```

## Build Stats

| Region | Used | Available | % |
|--------|------|-----------|---|
| Flash (PFM) | ~1299 KB | 8 MB | 15.5% |
| SRAM | ~51 KB | 960 KB | 5.3% |

## Expected Boot Output

```
NuttShell (NSH) NuttX-12.x
nsh>
```

LED0 = steady ON after boot. LED1 = 1 Hz blink (scheduler alive). LED1 frozen = scheduler stalled.

## NuttX Submodule

| Submodule | Repository | Branch | Commit |
|-----------|-----------|--------|--------|
| `platforms/nuttx/NuttX/nuttx` | [AMzeo/nuttx](https://github.com/AMzeo/nuttx) | `pic32czca90-bringup` | `01224dbefd` |
| `platforms/nuttx/NuttX/apps` | [PX4/NuttX-apps](https://github.com/PX4/NuttX-apps) | `px4_firmware_nuttx-10.3.0+` | `e37940d` |

The NuttX fork contains the full PIC32CZ CA90 chip support: clock, GPIO, SERCOM (UART/SPI/I2C), TCC, USBHS, SQI, SDMMC, DMA, EIC, IRQ table, and TCGETS/TCSETS termios support.

## Pending Development

Priority order for reaching first flight:

1. **ADC / Battery monitoring** — required for arming (P10)
2. **Watchdog (WDT)** — 4 s timeout safety (P12)
3. **PROGMEM crash log** — crash dump survives reboot (P9)
4. **CAN-FD (MCAN)** — DroneCAN ESC/GPS telemetry (P11)
5. **Ethernet (GMAC)** — GCS UDP link on custom board

## Comparison to CA70 Port

| Feature | CA70 | CA90 |
|---------|------|------|
| USB | FS (12 Mbps) | **HS (480 Mbps)** ✅ |
| SQI param storage | Working | Working ✅ |
| SD card logging | Working | Working ✅ |
| GPS | Working | Working ✅ (SERCOM0, 115200 baud) |
| Telemetry (SiK radio) | Working | Working ✅ (SERCOM4) |
| RC Input (SBUS) | Working | Working ✅ (SERCOM7, hardware RXINV) |
| IMU (ICM45686) | Working | Working ✅ |
| Mag (BMM150) | Working | Working ✅ |
| PWM outputs | Working | Working ✅ (4ch TCC1+TCC7) |
| ADC / Battery | Working | 🔲 Pending |
| Watchdog | Working | 🔲 Pending |
| CAN-FD | Not started | 🔲 Pending (CA90 has 6× MCAN) |
| Ethernet | N/A | 🔲 Pending (CA90 has GMAC) |

## Documentation

| Document | Description |
|----------|-------------|
| [CLAUDE.md](CLAUDE.md) | Architecture, clock tree, key bugs fixed, pending driver work, session protocol |

---

*Branch: `pic32czca90-port`*

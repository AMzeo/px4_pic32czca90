# Quick Start — PIC32CZ CA90 PX4 Port

## 1. Clone

```bash
git clone --recursive https://github.com/AMzeo/px4_pic32czca90.git
cd px4_pic32czca90
git submodule update --init --recursive
```

## 2. Install Prerequisites

```bash
# ARM cross-compiler
sudo apt install gcc-arm-none-eabi

# Python dependencies
pip3 install -r requirements.txt
```

## 3. Build

```bash
make microchip_czca90curiosity_default
```

Build outputs in `build/microchip_czca90curiosity_default/`:
- `.elf` — for GDB/OpenOCD debugging
- `.bin` — raw binary
- `.px4` — PX4 upload format
- `.hex` — Intel HEX (for MPLAB X / PKOB4 programmer)

Generate HEX manually if needed:

```bash
arm-none-eabi-objcopy -O ihex \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.hex
```

## 4. Flash

Use **MPLAB X IDE** or **MPLAB IPE** with the `.hex` file and the on-board PKOB4 programmer
(J700 USB connector on the PIC32CZ CA90 Curiosity Ultra board).

## 5. Connect to Console

```bash
minicom -D /dev/ttyACM0 -b 115200
```

Expected output:

```
ABDE
NuttShell (NSH) NuttX-12.x
nsh>
```

| Marker | Meaning |
|--------|---------|
| A | Clocks (PLL0) + UART initialized |
| B | Early serial init done |
| D | Board GPIO (LEDs) initialized |
| E | Caches enabled, entering nx_start() |

## 6. Hardware

| Connector | Purpose | Linux device |
|-----------|---------|--------------|
| J700 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` |
| J200 — Target USB | MAVLink CDC/ACM (when USB working) | `/dev/ttyACM1` |

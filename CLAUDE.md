# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Initialize submodules after cloning
git submodule update --init --recursive

# Build CA90 firmware
make microchip_pic32czca90-curiosity_default

# Generate Intel HEX for MPLAB IPE / PKOB4 programmer
arm-none-eabi-objcopy -O ihex \
  build/microchip_pic32czca90-curiosity_default/microchip_pic32czca90-curiosity_default.elf \
  build/microchip_pic32czca90-curiosity_default/microchip_pic32czca90-curiosity_default.hex

# Clean build
make clean
```

Build outputs land in `build/microchip_pic32czca90-curiosity_default/`:
- `.elf` — ELF (for GDB/OpenOCD debugging)
- `.bin` — raw binary
- `.px4` — PX4 upload format
- `.hex` — Intel HEX (for MPLAB X / PKOB4)

Current build size: ~1352 KB flash (64.5% of 2 MB), ~53 KB SRAM (11.7% of 512 KB).

## Flash & Debug

**Flash:** Use MPLAB X IDE or MPLAB IPE with the `.hex` file and the on-board PKOB4 programmer.

**USB connectors on the PIC32CZ CA90 Curiosity board:**

| Connector | Purpose | Linux device |
|-----------|---------|--------------|
| J700 — PKOB4 debug USB | NSH console (UART1, fixed 115200 baud) | `/dev/ttyACM0` |
| J200 — Target USB | MAVLink CDC/ACM | `/dev/ttyACM1` |

```bash
# Open NSH console
minicom -D /dev/ttyACM0 -b 115200
```

**Critical known bug — USB enumeration failure on every other boot:**
The USBHS peripheral is not reset by a software reset, so Linux does not detect a
disconnect/reconnect when the TARGET USB cable stays plugged. Workaround: physically unplug J200
(Target USB), reset the board, wait for full boot, then replug J200. Verify `/dev/ttyACM1`
appears before starting jMAVSim. Root cause and proper fix are tracked in
`docs/pic32czca90_px4_port_status.md` §8.4.

## Architecture

### NuttX + PX4 Layering

This firmware stacks two projects:

```
PX4 flight stack
  └── boards/microchip/pic32czca90-curiosity/   ← PX4 board definition
        ├── default.px4board                    ← toolchain, serial map, enabled modules
        ├── nuttx-config/                       ← NuttX Kconfig snapshot + linker script
        │     ├── include/board.h               ← clock frequencies, GPIO mux, console config
        │     ├── nsh/defconfig                 ← NuttX Kconfig options (drivers, USB, QSPI)
        │     └── scripts/script.ld             ← linker script (2 MB flash, 512 KB SRAM)
        ├── init/                               ← PX4 ROMFS startup scripts (rc.board_*)
        └── src/                               ← board drivers
              ├── board_config.h               ← GPIO macros, ADC config, QSPI MTD layout
              ├── init.c                       ← board_initialize(): MPU, DMA, SD, I2C, QSPI
              ├── qspi.c                       ← SST26VF032B flash + 3 MTD partitions
              └── usb.c / led.c / spi.cpp / i2c.cpp / timer_config.cpp

NuttX RTOS (git submodule, forked)
  └── platforms/nuttx/NuttX/nuttx/
        └── arch/arm/src/samv7/               ← chip layer reused by CA90
              ├── hardware/                   ← register headers
              ├── sam_clockconfig.c           ← PLL / clock tree setup
              ├── sam_serial.c / sam_qspi_spi.c / sam_usbdevhs.c / ...
              └── boards/arm/samv7/pic32czca90-curiosity/  ← board-side NuttX stubs
```

The CA90 has identical peripheral IP blocks to the SAMV71 (USBHS, SPI, TWIHS, QSPI, PWMC, TC,
AFEC), so it reuses the NuttX `samv7` chip layer with targeted patches. The NuttX submodule
is forked at [Vigneshjr1/NuttX](https://github.com/Vigneshjr1/NuttX) (branch `pic32cz-ca70-port`)
with 5 commits adding CA90 chip support:

| Commit | Change |
|--------|--------|
| `7a62af7` | irq.h PIC32CZ fix |
| `991448a` | chip.h peripheral counts |
| `be76bfe` | Full CA90 chip support |
| `9b04941` | QSPI driver fix |
| `6f96617` | HSMCI, progmem, EEFC fixes |

### Key Configuration Wiring

- Serial port map is set in `default.px4board` (`CONFIG_BOARD_SERIAL_*`) and must match device
  nodes registered in NuttX `defconfig`.
- `board.h` defines `BOARD_MCK_FREQUENCY`, all GPIO peripheral functions, and console UART.
- MTD partitions (params, caldata, waypoints) are carved in `qspi.c`; sizes must match
  `CONFIG_BOARD_PARAM_FILE` and dataman expectations.
- PWM output requires `PWM_MAIN_FUNCn` parameters to be set on first boot (see
  `docs/pic32czca90_px4_port_status.md` §7.4).

### QSPI Flash Layout

| Partition | Mount point | Size |
|-----------|-------------|------|
| params | `/fs/mtd_params` | 128 KB |
| caldata | `/fs/mtd_caldata` | 64 KB |
| waypoints | `/fs/mtd_waypoints` | 512 KB |

## HITL Simulation (jMAVSim)

Set these parameters once via NSH (`param set <NAME> <VALUE>`):

```
SYS_HITL=1
SYS_AUTOSTART=1001
CBRK_IO_SAFETY=22027
CBRK_SUPPLY_CHK=894281
COM_ARM_WO_GPS=1
COM_DISARM_PRFLT=-1
SDLOG_MODE=-1
```

Run jMAVSim (requires Java 11):
```bash
Tools/simulation/jmavsim/jmavsim_run.sh -d /dev/ttyACM1 -b 57600 -q
```

Connect QGC via UDP `localhost:14550`. Apply the USB enumeration workaround before each boot.

## PIC32CZ CA90 Port (In Progress)

A new port targeting the PIC32CZ CA90 (Cortex-M7, 300 MHz, 8 MB flash, 1 MB SRAM) is being
developed for the Curiosity Ultra board (EV16W43A). All port files currently live outside this
repo at:

```
C:\Users\husai\Downloads\pic32czca90_port\pic32czca90_port\
  nuttx/
    arch/arm/src/pic32czca90/       ← new chip layer (NOT samv7 reuse)
    boards/arm/pic32czca90/curiosity/
  px4/
    boards/microchip/czca90curiosity/
  openocd/
    pic32czca90.cfg
  docs/
    HANDOFF.md                      ← full design rationale and next steps
```

### CA90 vs CA90 Key Differences

| Item | CA90 | CA90 |
|------|------|------|
| Core | Cortex-M7, 150 MHz | Cortex-M7, 300 MHz |
| Flash | 2 MB internal | 8 MB dual-panel (0x0C000000) |
| SRAM | 512 KB | 1 MB (DTCM + SRAM) |
| NuttX chip layer | Reuses `samv7` | New `pic32czca90` directory |
| Clock source | Crystal | MEMS oscillator (XTALEN=0 on XOSC0) |
| DPLL ratio | — | LDR=49 (Fin=6 MHz after divider, 6×50=300 MHz) |
| SERCOM4 APB bus | — | APB **E** (not D) — APBEMASK.SERCOM4 |
| Flash base address | 0x80000000 | 0x0C000000 |

### CA90 Status

Complete and verified: linker script, all hardware register headers, clock bring-up sequence,
CM7 reset handler, polled UART (SERCOM4), NVIC init, interrupt-driven UART, board.h, NSH
defconfig, PX4 board definition, OpenOCD config.

Stubs present (bodies needed): `pic32czca90_leds.c`, `pic32czca90_bringup.c`,
`pic32czca90_appinit.c`.

Not yet created: GPIO driver, CAN-FD driver (port from `samv7/sam_mcan.c`, same IP),
Ethernet MAC driver.

### CA90 Immediate Next Steps

1. **Fix vector alignment bug** — In `pic32czca90_vectors.S` change `.align 9` → `.align 10`.
   CM7 VTOR requires 1024-byte alignment for 256 entries (2^10); current value causes random hard
   faults.

2. **Apply 5 NuttX integration patches** — These edits to existing NuttX files are not yet done:
   - `arch/arm/Kconfig` — add `ARCH_CHIP_PIC32CZCA90` entry
   - `arch/arm/src/Makefile` — add `pic32czca90` chip case
   - `arch/arm/src/CMakeLists.txt` — add `pic32czca90` subdirectory
   - `boards/arm/CMakeLists.txt` — add `pic32czca90` board directory
   - `boards/Kconfig` — add `ARCH_BOARD_PIC32CZCA90_CURIOSITY`

3. **First compile:** `./tools/configure.sh pic32czca90-curiosity:nsh && make`

4. **Flash and verify console:**
   ```bash
   openocd -f interface/cmsis-dap.cfg -f openocd/pic32czca90.cfg \
     -c "program nuttx.bin 0x0C000000 verify reset exit"
   minicom -D /dev/ttyACM0 -b 115200
   ```

5. **Complete stubs** — pattern from `boards/arm/samd5e5/` equivalents in NuttX.

6. **Port CAN-FD** — `samv7/sam_mcan.c` is near-identical IP; change base addresses, GCLK
   channel IDs, and MCLK mask bits.

7. **Upstream PRs** — Run `nxstyle`, add `SPDX-License-Identifier: Apache-2.0` headers, add RST
   docs page, submit to NuttX mainline, then PX4 mainline.

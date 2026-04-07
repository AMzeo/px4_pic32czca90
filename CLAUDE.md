# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Initialize submodules after cloning
git submodule update --init --recursive

# Build CA90 firmware
make microchip_czca90curiosity_default

# Generate Intel HEX for MPLAB IPE / PKOB4 programmer
arm-none-eabi-objcopy -O ihex \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.hex

# Clean build
make clean
```

Build outputs land in `build/microchip_czca90curiosity_default/`:
- `.elf` — ELF (for GDB/OpenOCD debugging)
- `.bin` — raw binary
- `.px4` — PX4 upload format
- `.hex` — Intel HEX (for MPLAB X / PKOB4)

Current build size: ~911 KB flash (10.9% of 8 MB), ~46 KB SRAM (5.5% of 832 KB).

## Flash & Debug

**Flash:** Use MPLAB X IDE or MPLAB IPE with the `.hex` file and the on-board PKOB4 programmer
(J700 USB connector).

**USB connectors on the PIC32CZ CA90 Curiosity Ultra board (EV16W43A):**

| Connector | Purpose | Linux device |
|-----------|---------|--------------|
| J700 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` |
| J200 — Target USB | MAVLink CDC/ACM (when USB working) | `/dev/ttyACM1` |

```bash
# Open NSH console
minicom -D /dev/ttyACM0 -b 115200
```

Expected boot output on `/dev/ttyACM0` (with `CONFIG_DEBUG_FEATURES=y`):
```
AB DE
NuttShell (NSH) NuttX-12.x
nsh>
```
Characters A/B/D/E are `showprogress()` markers from `sam_start.c`:
- `A` — clocks + UART initialized
- `B` — early serial init done
- `D` — board GPIO init done
- `E` — caches enabled, entering `nx_start()`

## Architecture

### NuttX + PX4 Layering

There are **three distinct layers** — all files are in this repo:

```
PX4 flight stack
  └── boards/microchip/czca90curiosity/         ← PX4 board definition
        ├── default.px4board                    ← toolchain, serial map, enabled modules
        ├── nuttx-config/                       ← NuttX Kconfig snapshot used by PX4 build
        │     ├── include/board.h               ← clock frequencies, GPIO mux, console config
        │     ├── nsh/defconfig                 ← NuttX Kconfig options (drivers, USB, QSPI)
        │     └── scripts/script.ld             ← linker script (8 MB flash, 832 KB SRAM)
        ├── init/                               ← PX4 ROMFS startup scripts (rc.board_*)
        └── src/                               ← PX4 board drivers
              ├── board_config.h               ← GPIO macros, ADC config
              ├── init.c                       ← board_initialize(): MPU, DMA, I2C
              └── led.c                        ← LED driver

NuttX RTOS chip layer (custom — NOT samv7 reuse)
  └── platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/
        ├── hardware/                          ← CA90 register headers (verified vs DFP)
        │     ├── pic32czca90_memorymap.h      ← peripheral base addresses
        │     ├── pic32czca90_pinmap.h         ← SERCOM/PORT pin assignments
        │     ├── sam_oscctrl.h                ← OSCCTRL: DFLL, PLL0 registers
        │     ├── sam_gclk.h                   ← GCLK generator/channel defines
        │     ├── sam_mclk.h                   ← MCLK clock dividers
        │     ├── sam_supc.h                   ← SUPC voltage regulator
        │     ├── sam_usart.h                  ← SERCOM USART registers
        │     └── sam_port.h                   ← PORT (GPIO) registers
        ├── sam_start.c                        ← __start(): BSS clear, data copy, boot seq
        ├── sam_clockconfig.c                  ← sam_pll0_init(): PLL0 bring-up
        ├── sam_gclk.c                         ← GCLK generator configure + channel enable
        ├── sam_lowputc.c                      ← sam_lowsetup(): early UART init
        ├── sam_serial.c / sam_usart.c         ← interrupt-driven UART driver
        ├── sam_port.c                         ← GPIO pin mux
        ├── sam_sercom.c                       ← SERCOM APB clock + GCLK channel enable
        ├── sam_irq.c                          ← NVIC init
        └── sam_timerisr.c                     ← SysTick

NuttX RTOS board layer
  └── platforms/nuttx/NuttX/nuttx/boards/arm/pic32czca90/pic32czca90-curiosity/
        ├── include/board.h                    ← clock/SERCOM config (mirrors PX4 board.h)
        ├── configs/nsh/defconfig              ← standalone NuttX config (not used by PX4)
        ├── scripts/linker.ld                  ← standalone NuttX linker script (not used by PX4)
        └── src/
              ├── pic32czca90_boot.c           ← sam_board_initialize(): LED GPIO, ARCH_LEDS
              ├── pic32czca90_autoleds.c        ← NuttX LED state machine
              ├── pic32czca90_appinit.c         ← NSH application init hook
              └── pic32czca90_bringup.c         ← filesystem / device mount (stub)
```

**Important:** The PX4 build uses `nuttx-config/` from the PX4 board directory, NOT the
standalone NuttX `configs/nsh/defconfig` or `scripts/linker.ld`. The NuttX build integration
goes through `Make.defs` in the chip layer, which is referenced by `CONFIG_ARCH_CHIP=pic32czca90`.

### Clock Tree

```
DFLL48M (48 MHz, enabled at reset, RESETVALUE=0x82)
  |
  └── PLL0 (REFSEL=2, REFDIV=12, FBDIV=225, POSTDIV=3)
        = 48 MHz / 12 = 4 MHz ref → × 225 = 900 MHz VCO → / 3 = 300 MHz
        |
        ├── GCLK0 (SRC=6=PLL0_1, DIV=1) → 300 MHz → CPU (MCLK.CLKDIV[1]=2 before switch)
        └── GCLK1 (SRC=6=PLL0_1, DIV=2) → 150 MHz → SERCOM1 core clock
              └── SERCOM1 BAUD=64730 → 115200 baud  (matches Harmony exactly)

OSCULP32K → GCLK3 (SRC=3, DIV=1) → 32.768 kHz → SERCOM slow clock
```

This clock tree is identical to the Harmony `usart_echo_blocking` example for the CA90 board,
which is the ground truth for what works on hardware.

**Key clock facts:**
- DFLL48M is NOT configured in software — it is already running from hardware reset
- XOSC0 (MEMS oscillator) is NOT used — PLL0 references DFLL directly
- `sam_dfll_configure()` must NOT be called — Harmony never calls it; CA90 has no DFLLSYNC
  register (corrected: DFLLMUL at 0x003C per DFP; DFLLSYNC removed from header)
- `sam_pll0_init()` in `sam_clockconfig.c` implements the exact Harmony PLL0_Initialize() sequence

### Console UART (SERCOM1)

Pins confirmed from Harmony and board schematic:
- PC04 = PAD0 = TX (function D, PMUX=3) → PKoB4 VCP J700
- PC07 = PAD3 = RX (function D, PMUX=3) → PKoB4 VCP J700
- TXPO=0 (PAD0, no flow control), RXPO=3 (PAD3)
- Core clock: GCLK1 = 150 MHz → BAUD register = 64730 → 115200 baud

### File Synchronization Requirements

**CRITICAL: These file groups must always be updated together. Changing one without the others causes drift that is hard to detect.**

#### LED / Button hardware group
When any LED pin, polarity, count, or button pin changes — update ALL of:

| File | What to update |
|------|----------------|
| `hardware/pic32czca90_pinmap.h` | `PORT_LED0/1`, `PORT_SW0/1` pin definitions |
| `nuttx-config/include/board.h` (PX4) | `BOARD_NLEDS`, `BOARD_LED0/1_BIT`, `NUM_BUTTONS`, `BUTTON_SW0/1_BIT` |
| `boards/arm/pic32czca90/.../include/board.h` (NuttX) | same as above — keep identical to PX4 board.h |
| `boards/arm/pic32czca90/.../src/pic32czca90_autoleds.c` | `board_autoled_initialize/on/off` — must cover all BOARD_NLEDS |
| `boards/arm/pic32czca90/.../src/pic32czca90_userleds.c` | `g_ledpins[]`, `board_userled`, `board_userled_all` |
| `boards/microchip/czca90curiosity/src/board_config.h` (PX4) | `GPIO_nLED_BLUE/GREEN`, `BOARD_ARMED_STATE_LED` |
| `boards/microchip/czca90curiosity/src/led.c` (PX4) | `g_ledmap[]`, `phy_set_led`, `phy_get_led` |
| `boards/arm/pic32czca90/.../src/pic32czca90_boot.c` | `sam_board_initialize()` — configure all LED GPIOs |

**Current board hardware (DS70005522C Table 2-11, Harmony-verified):**
- LED0: PB21, active LOW (yellow) — `PORT_LED0` → `GPIO_nLED_BLUE` → `g_ledmap[0]`
- LED1: PB22, active LOW (yellow) — `PORT_LED1` → `GPIO_nLED_GREEN` → `g_ledmap[1]`
- SW0: PB24, active LOW, pullup — `PORT_SW0`
- SW1: PC23, active LOW, pullup — `PORT_SW1`

#### Clock / SERCOM configuration group
When any clock frequency, GCLK source, SERCOM pin, or baud rate changes — update ALL of:

| File | What to update |
|------|----------------|
| `nuttx-config/include/board.h` (PX4) | `BOARD_GCLK*`, `BOARD_SERCOM*`, `BOARD_CPU_FREQUENCY` |
| `boards/arm/pic32czca90/.../include/board.h` (NuttX) | keep identical to PX4 board.h |
| `hardware/pic32czca90_pinmap.h` | `PORT_SERCOM*_PAD*` pin definitions |
| `sam_clockconfig.c` | `sam_pll0_init()` register values |
| `CLAUDE.md` | Clock tree diagram and "Console UART" section |

**Current console (Harmony-verified):** SERCOM1, PC04 PAD0 TX / PC07 PAD3 RX, func D, GCLK1=150 MHz, BAUD=64730 → 115200

### Key Bug History (Fixed)

| Bug | File | Symptom | Fix |
|-----|------|---------|-----|
| BSS/data init wrong pointer | `sam_start.c` | All globals garbage on boot; random crashes | `(uint32_t *)_sbss` → `&_sbss` |
| DFLL configure called | `sam_clockconfig.c` | Disabled DFLL before PLL0 init | Removed `sam_dfll_configure()` call |
| GCLK1 wrong source/freq | `board.h` | SERCOM1 BAUD wrong (63020 not 64730) | Changed GCLK1: SRC=5→6, DIV=1→2 (150 MHz) |
| SERCOM1 TXPO CTS mode | `board.h` | TX suppressed by floating CTS pin | Changed TXPO from 2 (CTS) to 0 (no flow control) |
| DFLL WAITLOCK in open-loop | `board.h` | GCLK1 output gated, starved SERCOM1 | Set `BOARD_DFLL_WAITLOCK=FALSE` |
| DFLLMUL wrong offset | `sam_oscctrl.h` | sam_dfll_configure() would write wrong reg | Corrected to 0x003C (DFP); removed nonexistent DFLLSYNC |
| sam_dfll_configure DFLLSYNC refs | `sam_clockconfig.c` | Build error after DFLLSYNC removed | Replaced with OSCCTRL_SYNCBUSY_* (SYNCBUSY is the CA90 register) |
| LED active polarity wrong | `led.c`, `pic32czca90_autoleds.c`, `pic32czca90_userleds.c` | LED on when should be off, vice versa | Inverted polarity: LED0/1 are active LOW (PB21/PB22, DS70005522C) |
| LED initial state undefined | `pic32czca90_pinmap.h`, `sam_port.c` | LED glitch on init (could start lit) | Added PORT_FLAG_OUTVAL_HIGH; sam_portconfig sets OUTSET before DIRSET (Harmony sequence) |
| BOARD_CPU_FREQUENCY 300 MHz | `board.h` (both) | SysTick 2× too slow (MCLK.CLKDIV[1]=2 permanent) | Changed to DPLL0_FREQUENCY/2 = 150 MHz |
| VTOR not set explicitly | `sam_start.c` | Early HardFault routed to BootROM vectors | Added explicit NVIC_VECTAB write first in `__start()` |
| chip.h ITCM_SIZE wrong | `arch/arm/include/pic32czca90/chip.h` | 64 KB reported but CA90 has 128 KB ITCM | Changed to `128*1024` |

### NuttX Integration

The NuttX build integration works through `Make.defs` (not CMakeLists.txt — NuttX uses make here).
The following files wire pic32czca90 into the NuttX build:
- `arch/arm/Kconfig` — `config ARCH_CHIP_PIC32CZCA90` entry present ✓
- `boards/Kconfig` — sources `pic32czca90-curiosity/Kconfig` ✓
- `arch/arm/src/pic32czca90/Make.defs` — lists all chip C sources ✓
- `arch/arm/src/pic32czca90/Kconfig` — chip Kconfig options ✓

### Boot Sequence and VTOR

The CA90 boot flow is:
1. CPU resets with VTOR=0x00000000.  At reset, 0x00000000 is aliased to **BootROM** (0x04000000) — ITCM is disabled at reset so there is no collision.
2. BootROM runs.  Under normal conditions (no ERASE pin, default BOCOR fuses) it reads SP from `[0x0C000000]` and PC from `[0x0C000004]`, sets VTOR=0x0C000000, then jumps to `__start`.
3. `__start` **immediately** writes VTOR=`_vectors` (= 0x0C000000) as its very first instruction.  This makes any early exception (HardFault during clock init) be handled by our own flash handlers rather than BootROM's — matching Harmony `startup_xc32.c: SCB->VTOR = &__svectors`.

**Without the explicit VTOR write:** if BootROM leaves VTOR=0x00000000 or some intermediate value, a HardFault during `sam_pll0_init()` or `sam_lowsetup()` would vector into BootROM code, potentially starting SAM-BA mode and manifesting as "no console activity".

### ITCM / DTCM (Cortex-M7 TCM)

The PIC32CZ CA90 is a Cortex-M7 with:
- **ITCM**: 128 KB at 0x00000000 — disabled at reset, kept disabled
- **DTCM**: 128 KB at 0x20000000 — disabled at reset, kept disabled

TCM is kept disabled because:
- No code or data is linked into TCM regions (linker script uses 0x0C000000 / 0x20020000)
- Enabling ITCM would replace the BootROM alias at 0x00000000; safe only after VTOR is set
- The SAMV7 port has `sam_tcmenable()` but with `#warning Missing logic` — TCM use for performance is a future optimisation
- With VTOR set to 0x0C000000 (flash), ITCM/DTCM state is irrelevant to interrupt handling

Future: if critical ISRs need deterministic timing, they can be linked into ITCM and TCM enable added to `__start` after the VTOR write.

### Cortex-M7 Cache Configuration

Defconfig settings:
```
CONFIG_ARMV7M_ICACHE=y         → up_enable_icache() called in __start
CONFIG_ARMV7M_DCACHE=y         → up_enable_dcache() called in __start
CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y
CONFIG_ARM_MPU=y               → 16 regions, early reset
CONFIG_ARM_MPU_NREGIONS=16
```

D-Cache is write-through (safe for DMA without explicit flush/invalidate).  MPU is enabled but regions are not yet configured for DMA nocache area — required before enabling DMA drivers.

### Comparison with Known-Working NuttX Cortex-M7 Port (SAMV7)

| Aspect | SAMV7 (Cortex-M7, flash at 0x00400000) | CA90 (Cortex-M7, flash at 0x0C000000) |
|--------|----------------------------------------|---------------------------------------|
| VTOR at reset | 0x00000000 = flash alias → no issue | 0x00000000 = BootROM → must set explicitly ✓ fixed |
| TCM enable | sam_tcmenable() (partial, #warning) | Not needed, no TCM sections in linker |
| Cache enable | Unconditional in __start | Config-gated, both on in defconfig ✓ |
| MPU init | sam_mpu_initialize() from __start | Enabled by NuttX common MPU code via CONFIG_ARM_MPU ✓ |
| Clock | PMC/PLLA (different IP) | GCLK/PLL0 via sam_pll0_init() ✓ |
| GPIO init | sam_gpioinit() | sam_portconfig() per-pin; no bulk init needed |
| Port files | 70+ | 26 (minimal, sufficient for current peripherals) |

### Microchip Forum PLL0 Known Issue (t400049) — FIXED ✓

Forum: `https://forum.microchip.com/s/topic/a5CV400000033nlMAA/t400049`
Fixed in: Harmony CSP package v3.21.0

**The issue:** PLL0CTRL was not cleared before configuring PLL0.  If the register holds a
stale value from a previous boot or debugger session, PLL0 can fail to lock.  Same issue
exists for PLL1.

**The fix** added in CSP v3.21.0:
```c
/* Disable PLL0 and clear PLL0 control register */
OSCCTRL_REGS->OSCCTRL_PLL0CTRL = 0U;          ← THIS LINE was missing before v3.21.0
OSCCTRL_REGS->OSCCTRL_PLL0REFDIV = ...;
```

**Status in this repo: ALREADY CORRECT.**
`sam_pll0_init()` (`sam_clockconfig.c` step 2) writes `PLL0CTRL = 0` before REFDIV:
```c
putreg32(0, SAM_OSCCTRL_PLL0CTRL);   /* step 2: clear before config */
putreg32(12, SAM_OSCCTRL_PLL0REFDIV); /* step 3: REFDIV */
```

### Include Headers (arch/arm/include/pic32czca90/)

Three files that NuttX core includes everywhere — critical to get right:

| File | Purpose |
|------|---------|
| `chip.h` | Hardware resource counts (SERCOM×8, TC×8, TCC×5, CAN×2, DMA×32, ADC×2 with 16ch each, GCLK×12); NVIC priority levels (3 bits → 8 levels, step=0x20) |
| `irq.h` | Cortex-M7 exception vectors (NMI=2…SysTick=15) + base offset `SAM_IRQ_EXTINT=16` |
| `pic32czca90_irq.h` | 137 peripheral IRQ numbers (SERCOM0-7 ×4 each, CAN0/1, USB×4, GMAC, TCC0-4, TC0-7, ADC0/1, DMA, EIC, SDHC0/1, QSPI, etc.) |

**Known issues in these files (need DFP verification before enabling interrupts):**
- `pic32czca90_irq.h` line 3 defines `SAM_IRQ_XOSC1` — CA90 has **one** XOSC (XOSCCTRLA only); XOSC1 IRQ is SAMD5x-specific and likely wrong
- IRQ numbers were derived from SAMD5x ordering, not the CA90 DFP — must cross-check the full table against `PIC32CZ8110CA80208_DFP/include/pic32czca90/component/` before enabling any interrupt-driven driver

### CA90 Port Status

**Done (build verified, Harmony-matched):**
- Vector table at `0x0C000000`, reset vector → `__start` ✓
- VTOR set explicitly at the very start of `__start` ✓
- BSS clear and .data copy loops correct ✓
- Clock tree: PLL0→300 MHz, GCLK1→150 MHz, GCLK3→32 kHz — matches Harmony ✓
- SERCOM1 pin mux (PC04/PC07, func D) and baud rate (BAUD=64730→115200) match Harmony ✓
- LED0/LED1 active-LOW on PB21/PB22, initial state HIGH (LED off) — matches Harmony plib_port.c ✓
- BOARD_CPU_FREQUENCY = 150 MHz (correct after permanent MCLK.CLKDIV[1]=2) ✓
- I-cache and D-cache enabled (CONFIG_ARMV7M_ICACHE/DCACHE=y) ✓
- MPU enabled (CONFIG_ARM_MPU=y, 16 regions) ✓
- chip.h ITCM_SIZE corrected to 128 KB ✓
- Register headers (sam_oscctrl.h, sam_mclk.h, sam_gclk.h) verified vs DFP ✓

**Pending — by priority for a flying PX4 system:**

#### P0 — Boot verification (blocked on programmer; do first when hardware available)
1. Flash `.hex` via MPLAB X / PKOB4 (J700)
2. Verify `showprogress` A→B→D→E on `/dev/ttyACM0` at 115200
3. Verify NSH prompt `nsh>` is responsive and `free` / `uname` work

#### P1 — IRQ table audit (prerequisite for ALL interrupt-driven drivers)
4. Cross-check every line of `pic32czca90_irq.h` against the CA90 DFP interrupt vector table
   (`PIC32CZ8110CA80208_DFP/include/pic32czca90/pic32czca90.h` or `startup_*.c`)
   - Confirm/fix `SAM_IRQ_XOSC1` (likely wrong — CA90 has one XOSC)
   - Confirm SERCOM0-7 vector numbers (currently offset +46…+77)
   - Confirm CAN0/1, USB, GMAC, TCC, TC, ADC numbers
   - Confirm `SAM_IRQ_NEXTINT = 137` (SAMD5x has 145; CA90 may differ)

#### P2 — DMA (prerequisite for high-bandwidth I2C/SPI/USB)
5. Add missing register header `hardware/sam_dmac.h` (DMAC base, channel regs, descriptor layout)
6. Implement `sam_dmac.c` — 32-channel DMA driver (port from samd5e5/sam_dmac.c, update regs)
7. Configure MPU nocache region for DMA descriptors (linker already has 64 KB at 0x200F0000)

#### P3 — I2C master (prerequisite for IMU, mag, baro)
8. Add missing register header `hardware/sam_sercom_i2c.h` (I2C mode CTRLA/B, BAUD, STATUS)
9. Implement `sam_i2c_master.c` (port from samd5e5/sam_i2c_master.c)
10. Fix Kconfig wiring: `PIC32CZCA90_SERCOM*_ISI2C` must `select PIC32CZCA90_HAVE_I2C_MASTER`
    (currently Make.defs compiles sam_i2c_master.c on `HAVE_I2C_MASTER` but Kconfig never sets it)
11. Wire up I2C bus in `boards/microchip/czca90curiosity/src/init.c` (currently stub returns -1)

#### P4 — SPI master (prerequisite for SPI sensors / external flash)
12. Add missing register header `hardware/sam_sercom_spi.h`
13. Implement `sam_spi.c` (port from samd5e5/sam_spi.c)
14. Fix Kconfig wiring: `PIC32CZCA90_SERCOM*_ISSPI` must `select PIC32CZCA90_HAVE_SPI`

#### P5 — Timer/PWM (prerequisite for motor output)
15. Add missing register headers `hardware/sam_tc.h`, `hardware/sam_tcc.h`
16. Implement `sam_tc.c` — TC0-TC7 basic timer driver (16/32-bit, port from samd5e5)
17. Implement `sam_oneshot.c`, `sam_oneshot_lowerhalf.c`, `sam_freerun.c` (timer abstractions)
18. Implement TCC PWM driver — TCC0 has 6 match/capture channels → 6 PWM outputs for motors
19. Implement PX4 `io_timer` abstraction over TCC (in `boards/microchip/czca90curiosity/src/`)
20. Add TCC GCLK channel assignments to `sam_sercom.c` / board clock init

#### P6 — USB CDC-ACM (MAVLink over J200)
21. Add missing register header `hardware/sam_usb.h` (USB FS device controller regs)
22. Implement `sam_usb.c` — USB full-speed device driver (CA90 has FS USB, not HS)
23. Remove `-ENODEV` stubs in `boards/microchip/czca90curiosity/src/init.c`
24. Enable `CONFIG_USBDEV`, `CONFIG_CDCACM` in defconfig

#### P7 — ADC / battery monitoring
25. Add missing register header `hardware/sam_adc.h`
26. Implement ADC driver (ADC0/ADC1, 16 channels each, 12-bit SAR)
27. Wire battery voltage divider channel to PX4 battery_status module

#### P8 — CAN-FD
28. Add missing register header `hardware/sam_mcan.h` (MCAN — ISO 11898-1 CAN-FD)
29. Implement `sam_mcan.c` (port from SAMV7 mcan driver — same IP block)
30. Enable CAN0/CAN1 in defconfig and board init

#### P9 — Safety / misc
31. Implement `sam_wdt.c` — watchdog timer (flight safety requirement)
32. Implement `sam_eic.c` — external interrupt controller (needed for sensor data-ready IRQs)
33. Add `hardware/sam_eic.h`, `hardware/sam_wdt.h` register headers
34. Implement SDHC driver (`hardware/sam_sdhc.h` + driver) for SD card data logging
35. Implement QSPI driver (`hardware/sam_qspi.h` + driver) if external flash needed

**Missing register headers summary** (drivers blocked until these exist):

| Header needed | Blocks |
|---------------|--------|
| `hardware/sam_sercom_i2c.h` | I2C driver |
| `hardware/sam_sercom_spi.h` | SPI driver |
| `hardware/sam_dmac.h` | DMA driver |
| `hardware/sam_tc.h` | TC timer driver |
| `hardware/sam_tcc.h` | TCC PWM driver |
| `hardware/sam_usb.h` | USB driver |
| `hardware/sam_adc.h` | ADC driver |
| `hardware/sam_mcan.h` | CAN-FD driver |
| `hardware/sam_eic.h` | EIC driver |
| `hardware/sam_wdt.h` | Watchdog driver |
| `hardware/sam_sdhc.h` | SDHC driver |
| `hardware/sam_qspi.h` | QSPI driver |
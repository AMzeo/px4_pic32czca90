# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## SESSION PROTOCOL — READ THIS FIRST, EVERY SESSION

This section exists because one session wasted 8+ hours chasing problems introduced by
our own changes while trying to fix the original issue (QGC param writes). These rules
prevent that from ever happening again.

### Step 1 — Repo health check (do this before anything else)

```bash
# Parent repo — must print nothing (clean working tree)
git diff HEAD --name-only

# NuttX submodule — must print nothing
cd platforms/nuttx/NuttX/nuttx && git status --short && cd -

# If either is dirty: stop and understand WHY before touching anything.
# Do NOT assume previous session left things in a good state.
```

### Step 2 — Board health check (do this before any test)

From NSH console (J200, /dev/ttyACM0, 115200):
```
# Check LED1 is blinking (= scheduler alive)
# Check SQI params are not stale from previous session
param show SYS_USB_AUTO    # must be 2 for USB to work
param show GPS_1_CONFIG    # must be 201 (SERCOM0/ttyS1)
param show GPS_2_CONFIG    # must be 0
```

If USB doesn't enumerate after flashing new firmware — **do this first, before any code investigation:**
```
param reset_all
param save
reboot
```

`param reset_all` is the nuclear option: clears every saved param from SQI. After reboot,
`rc.board_defaults` re-applies all board-specific values from scratch. Use this whenever
USB or any subsystem stops working after a firmware change — it has fixed the issue every time.

**Critical:** SQI flash is NOT erased when firmware is reflashed via PKOB4.
Stale saved params silently override compiled-in defaults after every firmware update.
The most common symptom: USB doesn't enumerate despite sercon succeeding. Root cause:
SYS_USB_AUTO (or another param) stored as wrong value in SQI from a previous session.
Always `param reset_all; param save; reboot` before suspecting hardware or code.

### Step 3 — Before making any change to any file

State all three of these before touching a file:

1. **Hypothesis:** What specific behavior is wrong and why do I believe this file causes it?
2. **Change:** Exactly one file, exactly one change. Never multiple files at once.
3. **Test:** How will I confirm the change helped or didn't help? What is the expected output?

If you cannot state all three clearly → do not make the change yet. Investigate further first.

### Step 4 — After making a change

```bash
# Confirm only the intended file changed
git diff HEAD --name-only
```

If extra files appear → understand why before building.

### Step 5 — Before flashing any firmware

```bash
# 1. Verify repo is in the expected state
git diff HEAD --name-only

# 2. Verify NuttX submodule is clean
cd platforms/nuttx/NuttX/nuttx && git status --short && cd -

# 3. Check for debug prints left in shared files
grep -n "PX4_INFO\|syslog\|printf" src/modules/mavlink/mavlink_parameters.cpp | grep -i "debug\|param_set\|param_req"
# If any are found → decide: ship them or revert them first

# 4. Generate hex
arm-none-eabi-objcopy -O ihex \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf \
  build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.hex
```

### Step 6 — After reflashing

From NSH (within 10 seconds of boot):
```
# Verify heartbeat: LED1 must blink
# Then:
param show SYS_USB_AUTO    # confirm = 2
mavlink status             # after ~5s, confirm mavlink started
```

---

## INVESTIGATION DISCIPLINE — Never violate these

### Rule: One change per test cycle

Change one thing → build → flash → test → document result → then decide next change.
Never queue up "I'll change A, B, and C and see if it helps." If it helps you don't know
which one fixed it. If it breaks you don't know which one broke it.

### Rule: Revert before pivoting

If a change did not help → `git checkout HEAD -- <file>` before trying something else.
Never leave a "harmless" change in place while testing a different hypothesis. There is no
such thing as a harmless untested change.

### Rule: Never modify these files speculatively

These files are known-working on hardware. Modifying them without DFP register evidence
or a confirmed hardware bug report is prohibited:

| File | Reason |
|---|---|
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_usb.c` | USB HS driver — hardware-verified working at 480 Mbps |
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_i2c_master.c` | I2C driver — 1.6M+ DMA completions verified |
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_spi.c` | SPI driver — 307k+ ICM45686 transfers verified |
| `boards/microchip/czca90curiosity/src/qspi.c` | SQI param storage — persistence verified |
| `boards/microchip/czca90curiosity/src/sam_sqi.c` | SQI BD-DMA engine — verified |

Any modification to these files requires a git commit message explaining the confirmed bug
and DFP/hardware evidence. No speculative changes.

### Rule: Shared PX4/NuttX files get separate PRs

Files not under `boards/microchip/czca90curiosity/` or `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/`
are shared with other boards. Adding debug prints is OK for a session but MUST be reverted
before committing. Never commit shared file changes in the CA90 branch.

### Rule: Defconfig changes must be intentional

`boards/microchip/czca90curiosity/nuttx-config/nsh/defconfig` — every `=y`/`=n` change
must have a stated reason. Never "try" a Kconfig option speculatively. The known-good
values are in the **Protected defconfig values** table below.

### Rule: When USB stops working — check SQI params first

Before doing anything else: `param show SYS_USB_AUTO`. If not 2, that is the cause.
Do not chase USB hardware/driver issues until params are confirmed correct.

---

## PROTECTED DEFCONFIG VALUES — never change without proof

```
CONFIG_CDCACM_IFLOWCONTROL is not set   # DO NOT enable — breaks QGC connection
CONFIG_CDCACM_RXBUFSIZE=600             # increasing to 4096 not tested; do not change
CONFIG_CDCACM_TXBUFSIZE=12000           # known good; do not change
CONFIG_SERIAL_REMOVABLE=y               # forced by Kconfig select — cannot be removed
CONFIG_PIC32CZCA90_SDMMC0_WIDTH_D1_D4  # is NOT set — hardware constraint (1-bit SDMMC0)
```

---

## FIXED — QGC param writes (QGC→PX4) — FIXED 2026-08-30

Root cause: `sam_fifo_read()` in `sam_usb.c` used repeated `*fifo8` pointer reads
(always lane 0) instead of `(i & 3U)` byte-lane rotation. Every 4th byte was CRC byte
read as data → PARAM_SET payload corrupted → `param_find()` returned PARAM_INVALID.
Fix: Harmony `(i & 3U)` pattern in all FIFO reads. See Key Bug History table.

---

## NSH Command Registration — Rules (read before adding any new command)

There are exactly **two mechanisms** for making a command available in NSH. Confusing them is the most common "command not found" mistake in this repo.

### Mechanism A — Board-local test/utility commands

Place `px4_add_module` directly in `boards/microchip/czca90curiosity/src/CMakeLists.txt`.
No entry in `default.px4board` is needed.

```cmake
# boards/microchip/czca90curiosity/src/CMakeLists.txt
px4_add_module(
    MODULE board_hrt_test
    MAIN hrt_test          # becomes the NSH command name
    SRCS
        hrt_test.c
)
```

The PX4 build system automatically:
1. Compiles the module into a static library.
2. Writes an entry into `platforms/nuttx/NuttX/apps/builtin/registry/px4.bdat` and `px4.pdat`.
3. NuttX make regenerates `builtin_list.h` / `builtin_proto.h` from the registry.
4. `g_builtins[]` in the final ELF contains the command name → function pointer.

**Rule:** Any `px4_add_module` in the board CMakeLists.txt is unconditionally included for that board. You do NOT need `CONFIG_BOARD_*=y` in `default.px4board` for board-local commands.

### Mechanism B — Shared PX4 modules (systemcmds, drivers, flight stack)

These live in `src/modules/`, `src/systemcmds/`, `src/drivers/` and have a `Kconfig` file.
They are **only built and linked when** the corresponding `CONFIG_*=y` line exists in `default.px4board`.

```
# default.px4board
CONFIG_SYSTEMCMDS_PERF=y       # enables perf command
CONFIG_MODULES_COMMANDER=y     # enables commander
CONFIG_MODULES_SENSORS=y       # enables sensors
```

Without the `CONFIG_*=y` entry the module library is never compiled, never registered,
and the command is not in the ELF.

### Verification (before flashing)

Always verify a new command is in the ELF before flashing:

```bash
# Must print the command name — if empty, it is not registered
arm-none-eabi-strings build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf   | grep "^hrt_test$"

# Must show a valid flash address (0c......)
arm-none-eabi-nm build/microchip_czca90curiosity_default/microchip_czca90curiosity_default.elf   | grep hrt_test_main
```

### "command not found" diagnosis checklist

1. **Is it in the ELF?** Run the `strings` + `nm` checks above on the **latest build**.
2. **Was the board reflashed after the last build?** The running image is the previous flash — not the latest build.
3. **Is it a shared module missing from `default.px4board`?** Add `CONFIG_*=y`.
4. **Is it a board-local module missing from board CMakeLists.txt?** Add `px4_add_module`.

**The most common cause of "command not found": board has not been reflashed after the new module was added.**

---

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

| Connector | Purpose | Host device |
|-----------|---------|-------------|
| J200 — PKOB4 debug USB | NSH console (SERCOM1, 115200 baud) | `/dev/ttyACM0` (Linux) |
| J102 — Target USB | MAVLink CDC-ACM (USBHS0, 480 Mbps HS) | COM port (Windows) |

```bash
# Open NSH console
minicom -D /dev/ttyACM0 -b 115200
```

Expected boot output on `/dev/ttyACM0`:
```
NuttShell (NSH) NuttX-12.x
nsh>
```

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
DFLL48M (48 MHz, enabled at reset, not configured in software)
  |
  └── PLL0 (REFSEL=DFLL, REFDIV=12, FBDIV=225, POSTDIV=3)
        = 48 MHz / 12 = 4 MHz ref → × 225 = 900 MHz VCO → / 3 = 300 MHz
        |
        ├── GCLK0 (SRC=6=PLL0_1, DIV=0→1) → 300 MHz → CPU
        │     MCLK.CLKDIV[0] = 1 (CPU divider, no division) → CPU = 300 MHz
        │     MCLK.CLKDIV[1] = 2 (second domain divider, matches Harmony; NOT CPU)
        │
        └── GCLK1 (SRC=6=PLL0_1, DIVSEL=1, DIV=0 → /2) → 150 MHz
              ├── SERCOM1 (console UART): BAUD=64730 → 115200 baud
              └── TCC0 (HRT): 6.67 ns/tick, 32-bit free-run, wraps ~28.6 s

OSCULP32K → GCLK3 (SRC=3, DIV=1) → 32.768 kHz → SERCOM slow clock
```

**Full GCLK generator map (do not put new peripherals on GCLK1 — console+HRT only):**

| Generator | Source | DIV | Output | Assigned peripherals |
|-----------|--------|-----|--------|----------------------|
| GCLK0 | PLL0 | 1 | 300 MHz | CPU |
| GCLK1 | PLL0 | /2 (DIVSEL=1) | 150 MHz | SERCOM1 (console), TCC0 (HRT) — **reserved, do not add** |
| GCLK2 | PLL0 | 3 | 100 MHz | SQI1 (`GCLK_PCHCTRL[57] = GEN(2)\|CHEN`) |
| GCLK3 | OSCULP32K | 1 | 32.768 kHz | SERCOM slow, WDT, EIC |
| GCLK4 | PLL0 | 3 | 100 MHz | SDMMC0 main (`GCLK_PCHCTRL[58] = GEN(4)\|CHEN`) |
| GCLK5 | PLL0 | 25 | 12 MHz | SDMMC0 slow (`GCLK_PCHCTRL[59] = GEN(5)\|CHEN`) |

GCLK2/GCLK4/GCLK5 are not yet configured in board.h — must be enabled at Stage 1.

**Peripheral frequencies (hardware verified):**

| Peripheral | Clock | Frequency | Notes |
|-----------|-------|-----------|-------|
| CPU (Cortex-M7) | GCLK0 / CLKDIV[0]=1 | **300 MHz** | cross-test confirmed |
| SERCOM1 (console UART) | GCLK1 | **150 MHz** | BAUD=64730 → 115200 baud |
| TCC0 (HRT) | GCLK1 | **150 MHz** | 6.67 ns/tick, 64-bit µs via software |
| SysTick | CPU | **100 Hz** | RELOAD=2,999,999; CLK_TCK=100 Hz |
| GCLK3 (slow clock) | OSCULP32K | **32.768 kHz** | SERCOM slow, WDT |

**Key clock facts:**
- DFLL48M is not configured in software — it is already running from hardware reset
- XOSC0 (MEMS oscillator) is not used — PLL0 references DFLL directly
- `sam_dfll_configure()` must NOT be called — CA90 has no DFLLSYNC register
- **MCLK.CLKDIV[0]** (offset 0x0C) = CPU Clock Divider. Reset default = 1 (no division → CPU = 300 MHz).
  **READ-ONLY** — PAC write-protected. Writing causes BusFault during boot. `SAM_MCLK_CPUDIV` is
  intentionally NOT defined in `sam_mclk.h` to prevent accidental writes.
  `BOARD_CPU_FREQUENCY = BOARD_DPLL0_FREQUENCY = 300 MHz`
- **MCLK.CLKDIV[1]** (DFP offset 0x10) — Harmony GCLK0_Initialize writes 2 here then polls CKRDY.
  The CKRDY poll is the clock-domain barrier before switching GCLK0 source to PLL0; skipping it
  causes early boot hang. DS70005522C §21.6 claims CLKDIV1 is at 0x14 — **hardware-confirmed wrong**:
  reading 0x44052014 causes a bus stall. DFP stride-4 layout (0x10) matches hardware.
- GCLK1 uses DIVSEL=1 (power-of-two) mode: raw GENCTRL=0x00001106, DIV=0 →
  output = 300 MHz / 2^(0+1) = 150 MHz. TCC0/SERCOM1 are unaffected by MCLK.

### Console UART (SERCOM1)

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

**Current board hardware (DS70005522C Table 2-11):**
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

**Current console:** SERCOM1, PC04 PAD0 TX / PC07 PAD3 RX, func D, GCLK1=150 MHz, BAUD=64730 → 115200

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
| BOARD_CPU_FREQUENCY set to wrong value | `board.h` (both), `sam_mclk.h` | SysTick fired at 200 Hz instead of 100 Hz; `hrt_test cross` showed -50% skew | Root cause: `BOARD_CPU_FREQUENCY=150 MHz` made `SYSTICK_RELOAD=1,499,999`; actual CPU=300 MHz → SysTick at 200 Hz (2× fast). Fix: set `BOARD_CPU_FREQUENCY = BOARD_DPLL0_FREQUENCY = 300 MHz`. Also found: `SAM_MCLK_CPUDIV` was aliased to `CLKDIV[1]` (offset 0x10) — NOT the CPU divider. CPU divider is `CLKDIV[0]` (offset 0x0C), always at reset default 1 → CPU = 300 MHz. `SAM_MCLK_CPUDIV` removed entirely to prevent accidental writes to PAC-protected register. |
| MCLK CLKDIV[0] write causes BusFault on boot | `sam_mclk.h`, `sam_clockconfig.c` | Writing to 0x4405200C (CLKDIV[0]) during `sam_clock_configure()` caused BusFault before UART init — board appeared completely dead | CLKDIV[0] is PAC write-protected (hardware confirmed). CPU divider stays at reset default 1 = 300 MHz. Fix: removed `SAM_MCLK_CPUDIV` define; `sam_clockconfig` only writes `SAM_MCLK_CLKDIV1` (0x10) and polls CKRDY — matching Harmony `GCLK0_Initialize` exactly. |
| DS70005522C §21.6 CLKDIV1 offset incorrect | `sam_mclk.h` | Reading 0x44052014 (DS-claimed CLKDIV1 offset) causes CPU bus stall — board hangs | Hardware-confirmed: 0x44052014 is not accessible on CA90 (bus stall, no response). DFP stride-4 array places CLKDIV[1] at 0x10 — matches hardware. DS §21.6 register map is wrong at this offset. DO NOT access 0x44052014. |
| VTOR not set explicitly | `sam_start.c` | Early HardFault routed to BootROM vectors | Added explicit NVIC_VECTAB write first in `__start()` |
| chip.h ITCM_SIZE wrong | `arch/arm/include/pic32czca90/chip.h` | 64 KB reported but CA90 has 128 KB ITCM | Changed to `128*1024` |
| `.vectors` placed at PFM (0x0C000000) | `script.ld`, `linker.ld` | No console output after programming — BootROM reads SP/PC from BFM (0x08000000) which was unprogrammed (0xFF) | Added `boot_rom` region at 0x08000000; `.vectors` → `boot_rom`, `.text` → `flash` (VECTOR_REGION=boot_rom convention) |
| `my_hardfault` silent loop in vector table | `arch/arm/src/armv7-m/arm_vectors.c` | NMI [2], reserved [7..10], DebugMon [12..13] silently looped — any unexpected exception froze the system with zero output | Removed `my_hardfault`; all vectors now route to `exception_common`, which dispatches to registered PANIC handlers |
| SysTick direct cast to `nxsched_process_timer` | `arch/arm/src/pic32czca90/sam_timerisr.c` | `(xcpt_t)nxsched_process_timer` signature mismatch (void vs int return) — undefined behavior in practice | Added proper `static int sam_timerisr(int irq, FAR uint32_t *regs, FAR void *arg)` wrapper |
| Missing NVIC debug handlers | `arch/arm/src/pic32czca90/sam_irq.c` | Unexpected NMI/BusFault/UsageFault/PendSV/DebugMon produced no output even with `CONFIG_DEBUG_FEATURES=y` | Added `sam_nmi`, `sam_busfault`, `sam_usagefault`, `sam_pendsv`, `sam_dbgmonitor`, `sam_reserved` handlers (SAMV7 pattern) — each calls `PANIC()` with diagnostic message |
| NVIC init used static count instead of NVIC_ICTR | `arch/arm/src/pic32czca90/sam_irq.c` | Priority register count `(SAM_IRQ_NEXTINT+3)/4` correct but brittle | Switched to `nintlines = (NVIC_ICTR & INTLINESNUM_MASK) + 1`; loops use NVIC_ICTR-derived count following SAMV7 pattern |
| NVIC_VECTAB not written in `up_irqinitialize` | `arch/arm/src/pic32czca90/sam_irq.c` | Vector table address confirmed only in `__start`; `up_irqinitialize` didn't re-affirm it | Added `putreg32((uint32_t)_vectors, NVIC_VECTAB)` following SAMV7 pattern |
| USART CTRLA missing IBON bit | `arch/arm/src/pic32czca90/sam_lowputc.c` | Immediate Buffer Overflow Notification disabled; BUFOVF detected late. Harmony example sets IBON | Added `USART_CTRLA_IBON` to ctrla initialization in `sam_usart_configure()` |
| IRQ table SAMD5x-derived (137 IRQs) | `arch/arm/include/pic32czca90/pic32czca90_irq.h` | SERCOM1 DRE at wrong offset (50 vs 64); interrupt-driven drivers would bind wrong vectors | Complete rewrite from Harmony `device_vectors.h` (PIC32CZ8110CA80208 DFP): 222 IRQs, SERCOM0-9 ×7 each, TCC0-9, CAN0-5 |
| chip.h peripheral counts SAMD5x-derived | `arch/arm/include/pic32czca90/chip.h` | NSERCOM=8, NTC=8, NTCC=5, NCAN=2 — all wrong for CA90 | Updated to NSERCOM=10, NTC=0 (CA90 has no TC, only TCC), NTCC=10, NCAN=6 |
| DSU DID address wrong (SAMD5x value) | `board_mcu_version.c`, `board_identity.c` | `ver mcu` / `ver px4guid` / `ver all` hang → Hard Fault (unmapped read at 0x41002018) | CA90 DSU at 0x44000000 (APB A), DID offset = 0x0120 (DFP-verified) → `CA90_DSU_DID = 0x44000120` |
| DSU reads cause CPU bus stall | `board_mcu_version.c`, `board_identity.c` | `ver mcu`, `ver px4guid`, `ver all` hang system completely — CPU stalls waiting for AHB response | DSU APB clock not enabled. Final fix: enable MCLK_ID_APB_DSU (=1) in CLKMSK[0] before reading DID. UID reads from FUSES_CALOTP (0x0A0071E0) need no clock enable. |
| rcS `ver all` in startup script blocked NSH | `ROMFS/px4fmu_common/init.d/rcS` | System printed 3 lines of ver output then hung — remaining lines in TX buffer couldn't drain because CPU was stalled at DSU access | Removed `ver all` from rcS; DSU stall also fixed by stubbing board_identity/mcu_version |
| `param set-default` doesn't set active in-memory value | `rc.board_defaults` | `cdcacm_autostart` runs despite `SYS_USB_AUTO -1` set via `set-default`; ~5 s after boot idle: "Device /dev/ttyACM0 does not exist" spam, system appears stuck | `param set-default` only updates stored default — when param import fails (no storage), in-memory value stays at compiled-in default. Changed to `param set SYS_USB_AUTO -1` |
| LED1 heartbeat never fired | `boards/microchip/czca90curiosity/src/init.c` | LED1 stayed off — heartbeat was wired into `pic32czca90_bringup()` (NuttX board layer) which is never called in a PX4 build; PX4 `board_app_initialize` in `init.c` overrides it | Moved heartbeat `work_queue(LPWORK, ...)` into PX4 `board_app_initialize` in `init.c` |
| Wrong GCLK channel IDs for TCC and CAN | `hardware/sam_gclk.h` | TCC GCLK channels (31-40) were SAMD5x-derived shared groups; CAN channels placed at 31-36 (wrong). Would route TCC0 to wrong GCLK channel, breaking HRT and any TCC peripheral | CA90 DFP: each TCC has individual channel — TCC0=31…TCC9=40; CAN0=46…CAN5=51; GMAC=52 (verified from Harmony plib_clock.c + DFP instance/tcc0.h, instance/can0.h) |
| Missing TCC MCLK APB clock IDs | `hardware/sam_mclk.h` | `MCLK_ID_APB_TCC0` through `MCLK_ID_APB_TCC9` not defined (41-50) — hrt_init() would fail to enable TCC0 APB clock, leaving TCC0 registers inaccessible | Added all 10 TCC MCLK IDs (41-50) after SERCOM9 definition |
| HRT callouts never fired — all PX4 tasks stall | `platforms/nuttx/src/px4/microchip/pic32czca90/hrt/hrt.c` | `hrt_call_every()` queued work items that sat in queue forever; all PX4 `ScheduledWorkItem` modules (commander, sensors, nav) never executed their periodic work; `ps` showed every task `Waiting` | Complete rewrite: TCC0 free-running at 150 MHz (GCLK1, DIV1), CC[0] compare match ISR fires `hrt_call_invoke()` at each deadline, `hrt_reschedule()` programs next CC[0]; READSYNC protocol for COUNT reads (Harmony-verified) |
| SQI param save: value not persisting across reboot | `boards/microchip/czca90curiosity/src/qspi.c` | `param set X 99999; param save; reboot; param show X` → value reverted to default. BSON import showed empty file (5 bytes). No error messages on save. | Root cause: D-cache coherency. FTL read-modify-write cycle read stale cached XIP data (old flash contents) and wrote it back, overwriting the just-saved param. Fix: (1) D-cache invalidate before every XIP read; (2) remove unnecessary XIP re-entry between erase/write; (3) mutex for concurrency; (4) read-back verification with 3 retries; (5) static buffers → stack-local |
| I2C ISR missing SYNCBUSY waits | `sam_i2c_master.c` | I2C probes work (init context) but ALL work-queue periodic transfers timeout (50ms). System crashes at timeout syslog print → looks like "SYNCBUSY caused hang" but it's actually stack overflow on 2336-byte wq:I2C5 stack from syslog() with 9 format args. Root cause masked for days. | Fix: (1) SYNCBUSY wait after EVERY write to ADDR, DATA, CTRLB in ISR — matching Harmony `plib_sercom0_i2c_master.c` exactly; (2) Replace `syslog()` with `_alert()` in timeout path (stack-safe polled output); (3) Sem drain `while(nxsem_trywait()==OK)` before each transfer prevents stale post from causing immediate return mid-bus-activity |
| I2C bus stuck after software reboot | `sam_i2c_master.c` | All I2C sensors fail to probe after `reboot` command (but work on fresh flash). Intermittent — ~50% of reboots fail. No timeout message printed (system hangs at probe). | Root cause: software reboot resets CPU but NOT I2C slaves. If slave was mid-byte (SDA low), it stays low forever waiting for SCL clocks. SERCOM init forces BUSSTATE=IDLE in STATUS register, but that only affects the master's state machine — doesn't fix physical SDA held low. Fix: I2C bus recovery (9 SCL clock pulses as GPIO) before SERCOM init. Standard procedure per I2C spec §3.1.16. |
| USB HS: TXCSRH.MODE not set | `sam_usb.c` | EP1+ TX interrupt (INTRTX) never fires — `epn_tx` increments but `tx_done` stays 0; host never receives bulk data | DFP confirms: TXCSRH.MODE (bit 5) = "Enable Endpoint Direction As TX". Must be set for all TX endpoints in device mode. Without it MUSB core doesn't generate INTRTX for EP1+. |
| USB HS: HSENABLE not set in POWER | `sam_usb.c` | Device enumerates at Full Speed only (12 Mbps); HSMODE=0 in POWER register | POWER.HSENABLE (bit 5) must be set before SOFTCONN to enable HS chirp signaling during bus reset |
| USB HS: H2D data phase ordering wrong | `sam_usb.c` | QGC "device not functioning" at HS; GET_LINE_CODING returns baud=8225/databits=7 (SETUP packet bytes leaked into linecoding struct); Windows CDC driver rejects device | Root cause: CLASS_SETUP called for SET_LINE_CODING BEFORE receiving the 7-byte OUT data phase. NuttX CDC immediately reads `dataout` (= ep0buf still containing SETUP bytes) into `priv->linecoding`. Fix: receive OUT data into ep0buf first, THEN call CLASS_SETUP with real data. |
| USB HS: No DISCON handler | `sam_usb.c` | Board hangs completely on cable unplug — CPU stuck in ISR from PHY oscillation | INTRUSB.DISCON fires on cable removal; without handler, continuous interrupts flood NVIC. Fix: disable SOFTCONN, mask EPn interrupts, call CLASS_DISCONNECT. |
| GPS UART baud rate never changed despite GPS_1_BAUD param | `sam_serial.c` | `gps status` shows `rate reading: 0 B/s`; setting `SER_GPS1_BAUD` or `GPS_1_BAUD` params had no effect | `sam_ioctl()` returned `-ENOTTY` for TCGETS/TCSETS → `tcsetattr()` always failed silently → SERCOM0 hardware stayed at `CONFIG_USART0_BAUD=9600` → GPS module at 115200 sent data that SERCOM0 decoded as garbage. Fix: (1) `CONFIG_USART0_BAUD=115200`; (2) added TCGETS/TCSETS to `sam_ioctl` using enable-protected BAUD register write (disable SERCOM → write BAUD → re-enable, each with SYNCBUSY poll). Verified 2026-09-08: status OK, 1594 B/s. |
| SQI partial Page Program with AHB DMA contention | `sam_sqi.c` | `param save` silently fails after SDMMC0+DMA enabled — BWRITE verify shows `got=ff` from byte 71 onward, consistent across all retries | Root cause: `sam_sqi_flash_cmd_write` waited only for `PKTCOMP` before clearing `BDCON`. `PKTCOMP` fires when the BD DMA finishes **loading** bytes into the TX FIFO, NOT when they are shifted out. With AHB DMA contention (SDMMC0 ADMA2 + I2C/SPI system DMA), the TX FIFO only held ~70 bytes when PKTCOMP fired; `BDCON=0` terminated the SQI clock mid-page, committing only 70 bytes. TXBUFFREE showed 186 free slots confirming 186 bytes still in FIFO. Fix: after PKTCOMP, poll `STAT1.TXBUFFREE == 256` (full FIFO free = shift register also empty = last bit out) before clearing BDCON. Also fixed: SE command buffer was 256 bytes (an old pre-TXEMPTY artifact); the SST26 received 4 cmd + 252 garbage bytes. Fixed to 4 bytes. |
| PROGMEM `g_row_buf` alignment — FCW ROWP rounds SRCADDR down | `sam_progmem.c` | FCW ROWP rounds SRCADDR to the nearest 4-byte boundary. If `g_row_buf` is declared as `uint8_t g_row_buf[1024]` without alignment, GCC may place it at an odd address; ROWP then DMA-reads from the rounded-down address, writing wrong data into flash. | Added `__attribute__((aligned(4)))` to `g_row_buf`. FCW SRCADDR is now always 4-byte aligned. |
| PROGMEM `up_progmem_write` required ROW_SIZE alignment | `sam_progmem.c` | `progmem_dump.c` calls `up_progmem_write` with offsets that are not a multiple of 1024 (e.g., `write_user_stack` seeks 1716 bytes into a 1024-byte file). Write starts mid-row; naive ROWP starting at the aligned row_base overwrites the already-written header. | Changed to sub-row read-modify-write: determine the 1024-byte row containing `addr`, load current flash content into `g_row_buf`, overlay only the changed bytes, then ROWP the full row. Handles any `addr`/`count` combination. |
| PROGMEM NPAGES=2 truncates crash dump | `defconfig`, `board_config.h` | With NPAGES=2, the region is 8192 bytes. File layout: files 0-2 use 3×2048 = 6144 bytes; file 3 header at offset 6144 leaves only 2048 bytes for data. `write_user_stack` in `progmem_dump.c` writes `PROGMEM_DUMP_STACK_SIZE=3072` bytes, seeking to offset 1716 which is beyond the 1024-byte file 3 allocated data area. `up_progmem_write` returned -EFAULT; crash dump silently truncated. | Changed `CONFIG_PIC32CZCA90_PROGMEM_NPAGES=3` (12 KB region). File 3 now has 5120 bytes of data space, comfortably holding `fullcontext_s` (3072 bytes). |
| ICM-45686 MISO bit-drop (0x9F→0x8F pattern) | `hardware/pic32czca90_pinmap.h` | Mikroe Click SDO pull-down fights MISO rising edge on jumper wires. At 1 MHz, first HIGH after a run of LOWs sampled as LOW → register reads corrupted (bad_register events); `sam_fifo_read` for WHO_AM_I returned wrong value intermittently. | Added `PORT_FLAG_PULLEN \| PORT_FLAG_OUTVAL_HIGH` to PORT_SERCOM8_PAD3 (PD27 MISO). CA90 internal pull-up counteracts Click pull-down → faster rising edge → clean sampling. bad_register: 0 verified. Also added `PORT_FLAG_DRVSTR` to PAD0/PAD1 (MOSI/SCK) for drive strength on jumper wire capacitance. |
| SPI8 cap at 1 MHz caused FIFO overflow | `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_spi.c` | 1 MHz SPI too slow to drain ICM FIFO at 800 Hz ODR → ~4 overflows/sec, each discarding accumulated samples. Harmless for arming but undesirable. | Raised cap to 2 MHz after MISO pull-up confirmed bad_register=0 at 2 MHz. FIFO overflow drops to near-zero (startup transient only). BAUD register: 24 (100 MHz / (2×25) = 2 MHz). |

### I2C Driver — Production Rules for CA90 SERCOM

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_i2c_master.c`

These rules are hard-won from weeks of debugging. Violating any one causes intermittent failures that are extremely difficult to diagnose.

1. **SYNCBUSY after EVERY register write** to ADDR, DATA, CTRLB, STATUS — even in ISR context. Harmony does this. Without it, writes don't propagate at 300 MHz CPU / 100 MHz peripheral clock → ISR state machine gets confused → transfer never completes.

2. **Never use `syslog()` from work queue error paths.** The wq:I2C5 stack is 2336 bytes. `syslog()` with multiple format args overflows it. Use `_alert()` (polled lowputc, minimal stack). A stack overflow during error reporting masks the actual error — you see "system hangs" instead of the diagnostic that would explain the root cause.

3. **Semaphore drain before each transfer** (`while (nxsem_trywait(&waitsem) == OK);`). If a previous transfer timed out AND an ISR fires after the timeout (stale NVIC pending), the sem gets a stale post. Next transfer's sem_wait returns immediately while ADDR is already on the bus → bus stuck in OWNER with half-sent address. Drain prevents this.

4. **I2C bus recovery on init** (9 SCL clocks as GPIO). After software reboot, slaves may hold SDA low from an interrupted transfer. The CPU resets but slaves don't. Without recovery, probes fail intermittently after reboot. This is standard I2C practice (I2C spec §3.1.16, NXP AN10216).

5. **`SEM_PRIO_NONE` on waitsem** — any semaphore posted from ISR MUST have this. Without it, NuttX priority inheritance corrupts scheduler state when called from ISR context with high-priority waiters.

6. **All 7 SERCOM NVIC vectors → same handler.** DFP `SERCOM5_I2C_1_INT_SRC = 94` means SB fires on vector EXTINT+94 = `SAM_IRQ_SERCOM5_2`, NOT `SAM_IRQ_SERCOM5_1`. The `_N` suffix is position-in-vector-table, not function. Always enable all 7 and let the handler read INTFLAG.

7. **Never return to task context between I2C messages.** ISR must handle the full multi-message sequence (write reg addr → repeated START → read data) via `i2c_start_next_msg()`. Returning to task between messages corrupts SERCOM state.

8. **Force BUSSTATE=IDLE before every new transfer.** CA90 STOP doesn't reliably auto-transition OWNER→IDLE. If OWNER from prior STOP, force IDLE.

### NuttX Integration

The NuttX build integration works through `Make.defs` (not CMakeLists.txt — NuttX uses make here).
The following files wire pic32czca90 into the NuttX build:
- `arch/arm/Kconfig` — `config ARCH_CHIP_PIC32CZCA90` entry present ✓
- `boards/Kconfig` — sources `pic32czca90-curiosity/Kconfig` ✓
- `arch/arm/src/pic32czca90/Make.defs` — lists all chip C sources ✓
- `arch/arm/src/pic32czca90/Kconfig` — chip Kconfig options ✓

### Boot Sequence and VTOR

**CA80/CA90 memory regions** (identical; CA90 adds HSM only):
- **BootROM**: 0x04000000, 64 KB — silicon ROM (read-only hardware bootloader)
- **BFM (Boot Flash Memory)**: 0x08000000, 128 KB — writable flash; **vector table lives here**
- **PFM (Program Flash Memory)**: 0x0C000000, 8 MB — writable flash; code lives here

Source of truth: Harmony `PIC32CZ8110CA80208.ld` (`VECTOR_REGION = boot_rom`, `boot_rom` ORIGIN=0x08000000).

The boot flow is:
1. CPU resets with VTOR=0x00000000.  At reset, 0x00000000 is aliased to **BootROM** (0x04000000) — ITCM is disabled at reset so there is no collision.
2. BootROM runs.  Under normal conditions (no ERASE pin, default BOCOR fuses) it reads SP from `[0x08000000]` (BFM[0]) and PC from `[0x08000004]` (BFM[1] = `__start` in PFM), sets VTOR=0x08000000, then jumps to `__start`.
3. `__start` **immediately** writes VTOR=`_vectors` (= 0x08000000, BFM start) as its very first instruction.  This re-affirms the vector table location — matching Harmony `startup_xc32.c: SCB->VTOR = &__svectors`.

**Linker placement**: `.vectors` section → `boot_rom` region (0x08000000); `.text` → `flash` (0x0C000000).
This is the `VECTOR_REGION=boot_rom` convention from the Harmony XC32 linker scripts.

**Without correct vector placement**: placing `.vectors` at 0x0C000000 means BFM (0x08000000) is
unprogrammed flash (all 0xFF), so BootROM reads SP=0xFFFFFFFF and PC=0xFFFFFFFF → CPU dies before
`__start` ever runs → no console output. **This was the root cause of "no output after programming".**

### ITCM / DTCM (Cortex-M7 TCM)

The PIC32CZ CA90 is a Cortex-M7 with:
- **ITCM**: 128 KB at 0x00000000 — disabled at reset, kept disabled
- **DTCM**: 128 KB at 0x20000000 — disabled at reset, kept disabled

TCM is kept disabled because:
- No code or data is linked into TCM regions (linker script uses 0x0C000000 / 0x20020000)
- Enabling ITCM would replace the BootROM alias at 0x00000000; safe only after VTOR is set
- The SAMV7 port has `sam_tcmenable()` but with `#warning Missing logic` — TCM use for performance is a future optimisation
- With VTOR set to 0x08000000 (BFM), ITCM/DTCM state is irrelevant to interrupt handling

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

| Aspect | SAMV7 (Cortex-M7, flash at 0x00400000) | CA80/CA90 (Cortex-M7, BFM=0x08000000, PFM=0x0C000000) |
|--------|----------------------------------------|--------------------------------------------------------|
| VTOR at reset | 0x00000000 = flash alias → no issue | 0x00000000 = BootROM → BFM[0]/[4] must be valid; __start re-affirms VTOR=0x08000000 ✓ |
| Vector placement | .vectors at flash origin | .vectors → boot_rom (0x08000000 BFM); .text → flash (0x0C000000 PFM) ✓ |
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
| `chip.h` | Hardware resource counts (SERCOM×10, NTC=0, TCC×10, CAN×6, DMA×32, ADC×2 with 16ch each, GCLK×12); NVIC priority levels (3 bits → 8 levels, step=0x20) |
| `irq.h` | Cortex-M7 exception vectors (NMI=2…SysTick=15) + base offset `SAM_IRQ_EXTINT=16` |
| `pic32czca90_irq.h` | 222 peripheral IRQ numbers — from Harmony `device_vectors.h` (PIC32CZ8110CA80208 DFP): SERCOM0-9 ×7 each, TCC0-9 ×10 each, CAN0-5, USBHS0/1, ETH×6 priority queues, SQI0/1, SDMMC0/1 |

### CA90 Port Status

**Done (build verified, Harmony-matched):**
- Vector table at BFM `0x08000000` (boot_rom region), reset vector → `__start` in PFM ✓
- VTOR set explicitly at the very start of `__start` ✓
- BSS clear and .data copy loops correct ✓
- Clock tree: PLL0→300 MHz, GCLK1→150 MHz, GCLK3→32 kHz — matches Harmony ✓
- SERCOM1 pin mux (PC04/PC07, func D) and baud rate (BAUD=64730→115200) match Harmony ✓
- LED0/LED1 active-LOW on PB21/PB22, initial state HIGH (LED off) — matches Harmony plib_port.c ✓
- BOARD_CPU_FREQUENCY = 300 MHz; CPU = GCLK0 / CLKDIV[0]=1 = 300 MHz. CLKDIV[0] (0x0C) is CPU divider (PAC-protected, never written). CLKDIV[1] at 0x10 (DFP stride-4) written by Harmony for CKRDY barrier; 0x14 (DS-claimed CLKDIV1) causes bus stall — DS map is wrong. Cross-test verified: <0.3% skew ✓
- I-cache and D-cache enabled (CONFIG_ARMV7M_ICACHE/DCACHE=y) ✓
- MPU enabled (CONFIG_ARM_MPU=y, 16 regions) ✓
- chip.h ITCM_SIZE corrected to 128 KB ✓
- chip.h peripheral counts corrected: NSERCOM=10, NTC=0, NTCC=10, NCAN=6 ✓
- Register headers (sam_oscctrl.h, sam_mclk.h, sam_gclk.h) verified vs DFP ✓
- arm_vectors.c: all 16 system vectors → `exception_common`; `my_hardfault` removed ✓
- pic32czca90_irq.h: 222 IRQs from Harmony device_vectors.h (was SAMD5x 137) ✓
- NSH console alive, nx_start() reached ✓
- NSH interactive prompt (`nsh>`) verified on hardware; full keyboard input working ✓
- NSH stable under sustained operation (15+ min, all commands tested) ✓
- `cdcacm_autostart` suppressed via `param set SYS_USB_AUTO -1` (USB driver not yet implemented) ✓
- `my_hardfault` silent loop removed; all vectors → `exception_common` (NMI/BusFault/UsageFault/PendSV/DebugMon now produce PANIC output) ✓
- `sam_timerisr` proper wrapper added (replaces `(xcpt_t)nxsched_process_timer` direct cast) ✓
- NVIC debug handlers added in `sam_irq.c` under `CONFIG_DEBUG_FEATURES` (SAMV7 pattern) ✓
- NVIC_ICTR-based disable/priority loops in `up_irqinitialize` (SAMV7 pattern) ✓
- NVIC_VECTAB re-affirmed in `up_irqinitialize` ✓
- USART CTRLA IBON bit set in `sam_usart_configure()` (matches Harmony plib_sercom1_usart.c) ✓
- LED0 steady on after boot; LED1 blinks at 1 Hz via LPWORK heartbeat in `init.c`; LED1 frozen = scheduler stalled ✓
- `hardware/sam_tcc.h` added: TCC register definitions (CTRLA, CTRLBSET, SYNCBUSY, INTENCLR, INTENSET, INTFLAG, COUNT, WAVE, PER, CC[n]); all bit positions DFP-verified; TCC0 GCLK_ID=31, MCLK_ID_APB_TCC0=41 ✓
- `sam_gclk.h` TCC/CAN channel IDs corrected to DFP values: TCC0-9 = channels 31-40 (individual, not shared); CAN0-5 = 46-51; GMAC=52 ✓
- `sam_mclk.h` TCC MCLK APB IDs added: MCLK_ID_APB_TCC0=41 through MCLK_ID_APB_TCC9=50 ✓
- HRT (hrt.c) rewritten: TCC0 free-running 32-bit counter at GCLK1=150 MHz; CC[0] compare match ISR drives `hrt_call_invoke()`; READSYNC protocol for COUNT reads; 64-bit time extension via `g_base_time_us`; `hrt_reschedule()` programs next deadline; all PX4 `ScheduledWorkItem` periodic tasks now fire ✓
- SQI1 flash parameter storage production-ready: custom MTD ops (direct BD-DMA writes, XIP reads), D-cache invalidation before every read, mutex concurrency protection, read-back verification with 3 retries, erase spot-check. `param save` → reboot → `param show` verified on hardware ✓
- `hardware/sam_sqi.h` added: SQI register definitions, BD descriptor struct, XIP base address, GCLK/MCLK IDs — all DFP-verified ✓
- GCLK2 configured: PLL0/3 = 100 MHz → SQI1 core clock (`GCLK_PCHCTRL[57]`) ✓
- SDMMC0 SD card logging: ADMA2 4-bit mode, GCLK4=100 MHz main (`GCLK_PCHCTRL[58]`) + GCLK5=12 MHz slow (`GCLK_PCHCTRL[59]`), multi-block DMA up to 2.5 MB, 0 dropouts. Pins PC08-PC15 func I (MUX=8). Card detect on PC15 (active-LOW). Mount at `/fs/microsd` ✓
- No pin conflict with SQI1: SDMMC0 uses PC08-PC15; SQI1 uses PC30/PC31/PG00-03 (separate pins) ✓
- SPI master (SERCOM8/SPI8): `sam_spi.c` polled+DMA driver, `hardware/sam_sercom_spi.h` DFP-verified, GCLK2=100 MHz, pins PD24(MOSI)/PD25(SCK)/PD26(CS)/PD27(MISO) func D. 2 MHz cap (eval board — Mikroe Click jumper wires). MISO pull-up (PORT_FLAG_PULLEN+OUTVAL_HIGH on PD27) counteracts Click SDO pull-down. PX4 bus table in `spi.cpp` ✓
- I2C master (SERCOM5): `sam_i2c_master.c` interrupt-driven driver, `hardware/sam_sercom_i2c.h` DFP-verified, GCLK2=100 MHz, 400 kHz bus, pins PC25=SDA/PC26=SCL func D. PX4 bus table in `i2c.cpp` ✓
- DSU DID read: enables APB clock (MCLK_ID=1), reads 0x44000120, extracts REVISION[31:28] → `ver all` shows real silicon rev ✓
- Chip UID (FUSES_CALOTP UNIQID): reads factory-programmed 96-bit serial from 0x0A0071E0 → `ver all` shows unique PX4GUID per board ✓
- EIC driver (`sam_eic.c`): 16-channel async-mode external interrupt controller; GCLK3 for debounce; `px4_arch_gpiosetevent()` wired via `pic32czca90_gpiosetevent.c`; builds and links but NOT hardware-tested (no active DRDY consumer) ✓
- GPS UART (SERCOM0/ttyS1): TCGETS/TCSETS added to `sam_serial.c` `sam_ioctl()`; `CONFIG_USART0_BAUD=115200`; hardware-verified 2026-09-08: status OK, 1594 B/s, 9.94 Hz, u-blox UBX protocol ✓
- Telemetry UART (SERCOM4/ttyS2): SiK radio MAVLink at 57600 baud, MAV_0_CONFIG=101 ✓
- RC Input UART (SERCOM7/ttyS3): SBUS 100000 8E2, RC_PORT_CONFIG=300 ✓
  **SILICON BUG: USART_CTRLA_RXINV (bit 10) is R/W but has NO hardware effect on CA90. External NPN inverter is mandatory for inverted SBUS. PC22/PC21 pull-up (PORT_FLAG_PULLEN|OUTVAL_HIGH) required to prevent NPN collector float during idle.**

**Pending — by priority for a flying PX4 system:**

> **Storage (done):** SD card → flight logs (`/fs/microsd`). SQI1 → params/caldata/dataman.
> **Sensors (verified on hardware):** ICM45686 (SPI8 DMA, SERCOM8), BMI088 accel+gyro (I2C5 DMA), BMM150 mag (I2C5 DMA) — all 0 bad transfers.
>
> **DFP peripheral names (CA90 — do not use SAMD5x names):**
> - SD/MMC controller → `SDMMC` (`component/sdmmc.h`) — not "SDHC" or "HSMCI"
> - System DMA → `DMA` (`component/dma.h`) — not "DMAC"
> - SQI flash → `SQI` (`component/sqi.h`) — has integrated BD-DMA; does **not** use system DMA
> - USB → `USBHS` (`component/usbhs.h`) — high speed; not "USBFS"
>
> **DFP location:** `C:/Users/I74182/.mchp_packs/Microchip/PIC32CZ-CA90_DFP/1.7.168/CA90/include/`

#### P1 — SD Card Logging — ADMA2 4-bit mode (flight logs → /fs/microsd) — DONE ✓
> **Completed.** SDMMC0 ADMA2 4-bit mode working. Multi-block DMA up to 2.5 MB verified, 0 dropouts.
>
> **Hardware:** SDMMC0 (base 0x458A0000). Micro-SD socket wired to SDMMC0 pins:
> PC08=CLK, PC09=DAT0, PC10=DAT1, PC11=DAT2, PC12=DAT3, PC13=CMD, PC14=WP, PC15=CD.
> All func I (MUX=8). No pin conflict with SQI1 (SQI1 uses PC30/PC31/PG00-03).
>
> **GCLK assignment:**
> - SDMMC0 main clock: GCLK4 → 100 MHz (PLL0/3); `GCLK_PCHCTRL[58] = GEN(4)|CHEN`
> - SDMMC0 slow clock: GCLK5 → 12 MHz (PLL0/25); `GCLK_PCHCTRL[59] = GEN(5)|CHEN`
>
> **WARNING:** PC08-PC15 also map to SQI0 (func H = MUX=7). Never configure SQI0 — it conflicts with SDMMC0.
>
- `hardware/sam_sdmmc.h` — DFP-verified; SDMMC0 base `0x458A0000`, `GCLK_ID=58`, `GCLK_ID_SLOW=59`, `MCLK_ID_AHB=69`, `MCLK_ID_APB=70` ✓
- `sam_sdmmc.c` — ADMA2 4-bit mode; NuttX SDIO lower-half ✓
- Board init: card detect on PC15, `mmcsd_slotinitialize()`, mount at `/fs/microsd` ✓
- **Win:** `ls /fs/microsd` succeeds; logger writes with 0 dropouts ✓

#### P2 — SQI Flash Parameter Storage (params/caldata/dataman) — DONE ✓
> **Completed 2026-05-13.** Params persist across reboot (hardware-verified).
> See `docs/sqi_hardware_behavior.md` for full silicon quirk documentation.
>
> **No pin conflict:** SQI1 uses PC30/PC31/PG00-03 (func H = MUX=7). SDMMC0 uses PC08-PC15 (func I = MUX=8). Separate pins — no arbitration needed.
>
> **Architecture:** Custom MTD ops (qspi.c) bypass NuttX sst26.c entirely.
> Reads via XIP (0x90000000) with D-cache invalidation. Writes/erases via
> BD-DMA with read-back verification. Mutex protects all ops.
>
- `hardware/sam_sqi.h` — register defs, BD struct, XIP base, GCLK/MCLK IDs ✓
- `sam_sqi.c` — BD-DMA engine, XIP mode, JEDEC/RDSR/cmd primitives ✓
- `qspi.c` — custom MTD ops, D-cache invalidation, mutex, verification ✓
- **Win:** `param set CBRK_SUPPLY_CHK 894281`; reboot; `param show` returns 894281 ✓

#### P3 — System DMA — DONE ✓
> **Completed.** `sam_dmac.c` BD-based DMA driver. Used by I2C5 RX (1.6M+ transfers confirmed)
> and SPI8 TX+RX dual-channel (ICM45686 FIFO bursts). NOINC flag for NULL-buffer handling.
> MPU nocache region at 0x200F0000 for BD structs.
>
- `hardware/sam_dma.h` — DFP-verified register defs, trigger IDs, BD descriptor struct ✓
- `sam_dmac.c` — BD-based transfer engine, NuttX `sam_dmachannel`/`sam_dmastart`/`sam_dmastop` API ✓
- I2C5: RX DMA for bulk sensor FIFO reads; `i2c_stats` shows `dma_s=1.6M, dma_d=1.6M` ✓
- SPI8: TX+RX dual-channel DMA with NOINC for NULL buffers; `icm45686: bad transfer: 0` ✓

#### P4 — EIC (External Interrupt Controller) — IMPLEMENTED, NOT HARDWARE-TESTED
> **Implemented 2026-06-22.** Full EIC driver + PX4 gpiosetevent glue layer built and linked.
> No active consumer — ICM45686 uses FIFO polling (no DRDY needed). Infrastructure ready
> for future use (e.g. sensor with mandatory DRDY, or button/GPIO wake interrupts).
>
> **Architecture:** 16 EXTINT channels, each with dedicated NVIC vector (no software demux).
> Async mode — edge detection does not require GCLK; GCLK3 (32 kHz) connected for optional debounce/filter.
>
- `hardware/sam_eic.h` — DFP-verified register definitions, base 0x44800000, GCLK_ID=5, MCLK_ID_APB=16 ✓
- `sam_eic.c` — NuttX driver: init (APB+GCLK+SWRST), configure (sense/filter/async), disable, irq_ack ✓
- `sam_eic.h` — public API header ✓
- `pic32czca90_gpiosetevent.c` — PX4 glue: static handler table[16], pin→EXTINT mapping, NVIC attach ✓
- `micro_hal.h` — `px4_arch_gpiosetevent` wired to real implementation (was `-ENOSYS` stub) ✓
- `defconfig` — `CONFIG_PIC32CZCA90_EIC=y` ✓
- `init.c` — `sam_eic_initialize()` called at boot under `#ifdef CONFIG_PIC32CZCA90_EIC` ✓
- `board_config.h` — `GPIO_IMU_DRDY` defined (PA08, func A, EXTINT[8]) but unused in bus table ✓
- **NOT tested on hardware** — no sensor currently triggers EXTINT; will validate when a DRDY-dependent peripheral is added

#### P5 — SPI Master → IMU (SERCOM8) — DONE ✓
> **Completed.** SPI polled+DMA driver on SERCOM8. GCLK2=100 MHz. 2 MHz cap (eval board).
> Pins: PD24=MOSI(PAD0), PD25=SCK(PAD1), PD26=CS(GPIO), PD27=MISO(PAD3), function D.
> MISO pull-up (PORT_FLAG_PULLEN+OUTVAL_HIGH) counteracts Mikroe Click SDO pull-down.
> MOSI/SCK drive strength (PORT_FLAG_DRVSTR) for jumper wire capacitance.
> ICM45686 running at 798.9 Hz, 0 bad transfers, 0 bad registers, FIFO overflow near-zero.
>
- `hardware/sam_sercom_spi.h` — DFP-verified register definitions ✓
- `sam_spi.c` — NuttX `spi_dev_s` with TX+RX DMA (polled fallback for ≤4 bytes) ✓
- `spi.cpp` — PX4 board SPI bus table, chip-select functions, ICM warm-up ✓
- `init.c` — T0 block: clock-free CS LOW + polling WHO_AM_I loop at boot ✓
- **Win:** `icm45686: bad transfer: 0, bad register: 0` at 798.9 Hz ✓

#### P6 — I2C Master → Magnetometer + Barometer (SERCOM5) — DONE ✓
> **Completed.** Interrupt-driven I2C master with RX DMA on SERCOM5. GCLK2=100 MHz, 400 kHz bus.
> Pins: PC25=SDA (PAD0), PC26=SCL (PAD1), function D.
> BMI088 accel+gyro + BMM150 mag running, 0 bad transfers, 1.6M+ DMA completions.
>
- `hardware/sam_sercom_i2c.h` — DFP-verified register definitions ✓
- `sam_i2c_master.c` — NuttX `i2c_master_s` ISR + RX DMA driver ✓
- `i2c.cpp` — PX4 board I2C bus table ✓
- **Win:** `i2c_stats` shows `dma_s=1605923, dma_d=1605922` (1 in-flight); 0 bad transfers ✓

#### P7 — PWM / TCC (motor outputs) — DONE ✓
> **Completed 2026-06-26.** 4 PWM channels verified on oscilloscope at 403 Hz.
> TCC1 WO0/WO1 (PB10/PB11, EXT1) + TCC7 WO0/WO1 (PA22/PA23, EXT2).
> CC direct writes + SYNCBUSY poll — CCBUF causes bus stall (no SYNCBUSY bit).
> DShot future: DMA→CCBUF is hardware-paced (1670ns gap >> 20ns sync), safe.
>
- `io_timer_tcc.c` — full PX4 io_timer API: init, set_ccr, set_pwm_rate, enable/disable ✓
- `io_timer_hw_description.h` — constexpr helpers for TCC1/TCC7 ✓
- `timer_config.cpp` — board pin mapping (TCC1 ch0/1 + TCC7 ch0/1) ✓
- `rc.board_defaults` — `PWM_MAIN_TIM0=400`, `PWM_MAIN_TIM1=400`, FUNC1-4=101-104 ✓
- **Win:** `actuator_test set -m 1 -v 0.5` → 1487µs pulse; all 4 channels respond ✓

#### P8 — USBHS Device Driver (MAVLink + QGroundControl over J102) — DONE ✓
> **Completed 2026-07-08.** USB High Speed 480 Mbps, QGC connected, bidirectional MAVLink,
> txerr=0, gyro calibration done through QGC, disconnect/reconnect stable.
>
> **Architecture:** MUSB-based USBHS0 (base 0x4F010000) with wrapper + core registers.
> Single NVIC vector (SAM_IRQ_USBHS0 = EXTINT+213) handles all events (USB core + DMA).
> USBHS1 (IRQ 214) is a second controller, unused on this board.
>
> **Key bugs fixed:**
> - TXCSRH.MODE (bit 5) must be set for TX endpoints — without it, INTRTX never fires
> - POWER.HSENABLE required before SOFTCONN for 480 Mbps chirp negotiation
> - H2D control transfer data phase: must receive OUT data BEFORE calling CLASS_SETUP
>   (root cause of QGC "device not functioning" — linecoding struct got SETUP packet bytes)
> - DISCON handler: disable SOFTCONN + mask EPn interrupts on cable pull (prevents ISR storm)
> - ISR ordering: read MUSB registers before clearing wrapper INTFLAG (prevents missed interrupts)
> - Removed hardcoded device descriptor bypass (all descriptors via NuttX CLASS_SETUP)
>
- `hardware/sam_usbhs.h` — DFP-verified wrapper + MUSB register defs, DMA channel regs ✓
- `sam_usb.c` — NuttX `usbdev_s` lower-half; EP0 state machine, PIO bulk TX/RX, DMA (disabled) ✓
- `usb_diag.c` — NSH debug command (register dump, EP config, ISR counters, EP0 trace ring) ✓
- defconfig: `CONFIG_USBDEV=y`, `CONFIG_USBDEV_DUALSPEED=y`, `CONFIG_CDCACM=y` ✓
- **Win:** QGC connects at 480 Mbps HS; `mavlink status` shows txerr=0, GCS heartbeat valid ✓

#### P9 — PROGMEM Crash Log (crash survives reboot) — DONE ✓
> **Completed 2026-09-09.** Full CPU context dump (R0-R15, PC, LR, SP, xPSR, CFSR, HFSR,
> task name, git hash, interrupt + user stacks with 0xdeadbeef canaries) saved to last 3 PFM
> pages (12 KB) on fault, displayed on next boot. `rcS` auto-commits to `/fs/microsd/fault_N.txt`
> when SD is available. Three bugs fixed: `g_row_buf` alignment, sub-row RMW for non-aligned
> writes, NPAGES=2→3 to fit `fullcontext_s` (3072 bytes). See Key Bug History.
>
- `sam_progmem.c` — `up_progmem_*` API using `hardware/sam_fcw.h` ✓
- `CONFIG_PIC32CZCA90_PROGMEM_NPAGES=3` → last 12 KB of PFM reserved; 5120 bytes for crash data ✓
- defconfig: `CONFIG_PIC32CZCA90_PROGMEM=y`, `CONFIG_MTD_PROGMEM=y`, `CONFIG_BOARD_CRASHDUMP=y` ✓
- Board config: `HAS_PROGMEM`; board init: `board_hardfault_init(2, true)` ✓
- `rcS` auto-commits crash dump to `/fs/microsd/fault_N.txt` when SD is mounted ✓
- **Win:** deliberate NULL deref → fault → reboot → full register dump on console ✓

#### P10 — ADC / Battery Monitoring
- `hardware/sam_adc.h` from DFP `component/adc.h`; ADC0 `GCLK_ID=41`, `MCLK_ID_APB=51`
- ADC driver (ADC0–ADC3, 12-bit SAR); wire battery voltage divider to PX4 battery_status

#### P11 — CAN-FD (MCAN)
- `hardware/sam_mcan.h` from DFP `component/can.h`; `sam_mcan.c`
- CAN3 (J701) `GCLK_CH=48`, CAN4 (J702) `GCLK_CH=49`; ATA6561 transceivers on-board

#### P12 — Watchdog (WDT) — DONE ✓
> **Completed.** 4-second timeout (OSCULP32K, CYC4096), 2-second early warning ISR, kicked every 1s
> via `wdt_pet_task` kthread in `init.c`. ALWAYSON NOT set (keeps MPLAB programming safe).
> Hardware-verified: LED1 blinks continuously = WDT being kicked; deliberate stall test confirmed
> reboot at ~4s with early warning at 2s.
>
> **DFP register facts:** Base=0x44070000, MCLK_ID_APB=14, no GCLK (OSCULP32K always-on),
> IRQ=SAM_IRQ_EXTINT+14, clear key=0xA5 to WDT_CLEAR, SYNCBUSY after ENABLE write.
>
- `hardware/sam_wdt.h` — DFP-verified register definitions ✓
- `sam_wdt.c` + `sam_wdt.h` — driver: APB clock enable, CONFIG/EWCTRL write, ENABLE+SYNCBUSY, EW ISR ✓
- defconfig: `CONFIG_PIC32CZCA90_WDT=y`, `CONFIG_WATCHDOG=y` ✓
- init.c: `sam_wdt_initialize()` + `wdt_pet_task` kthread every 1s ✓
- **Win:** system reboots in ~4s when stalled; `_alert("WDT: early warning")` printed at 2s ✓

- ~~DSU: un-stub `board_mcu_version()` / `board_get_uuid32()`~~ — **DONE ✓** (DID at 0x44000120, UID at 0x0A0071E0)

**Missing register headers** (drivers blocked until these exist):

| Header needed              | DFP source                  | Blocks              |
|----------------------------|-----------------------------|---------------------|
| `hardware/sam_adc.h`       | `component/adc.h`           | ADC driver (P10)    |
| `hardware/sam_mcan.h`      | `component/can.h`           | CAN-FD driver (P11) |

**rc.board_defaults — production state (restored 2026-09-08)**

MAV_0_CONFIG=0, RC_PORT_CONFIG=101 — Curiosity board config:
- SERCOM7 (PD14/PD15) NOT exposed on Curiosity; RC SBUS uses SERCOM4 (PC21/PC22, EXT2 13/14).
- board.h BOARD_SERCOM4_MUXCONFIG has RXINV=0 (RXINV silicon no-op); external NPN inverter + PC22 pull-up required; defconfig USART4=100000/8E2.
- MAV_0_CONFIG=0: no serial radio needed — USB HS (J102) provides full GCS link at 480 Mbps.
- GPS on SERCOM0 (PC00/PC01) confirmed accessible on Curiosity EXT header.
Full flight-ready param set: EKF2 (GPS ctrl=7), PWM limits, RC mapping, PID tuning,
MPC position control, failsafe, dataman start. GPS enabled (EKF2_GPS_CTRL=7).

**Production PCB differences** (when moving off Curiosity):
- MAV_0_CONFIG=101 (SERCOM4/ttyS2 = SiK telemetry radio, restore RXINV=0 + 57600 baud in board.h)
- RC_PORT_CONFIG=300 (SERCOM7/ttyS3 = SBUS, RXINV in SERCOM7 muxconfig)
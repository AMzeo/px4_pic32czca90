# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
| GCLK4 | PLL0 | 3 | 100 MHz | SDMMC1 main (`GCLK_PCHCTRL[60] = GEN(4)\|CHEN`) |
| GCLK5 | PLL0 | 25 | 12 MHz | SDMMC1 slow (`GCLK_PCHCTRL[61] = GEN(5)\|CHEN`) |

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
| SQI partial Page Program with AHB DMA contention | `sam_sqi.c` | `param save` silently fails after SDMMC0+DMA enabled — BWRITE verify shows `got=ff` from byte 71 onward, consistent across all retries | Root cause: `sam_sqi_flash_cmd_write` waited only for `PKTCOMP` before clearing `BDCON`. `PKTCOMP` fires when the BD DMA finishes **loading** bytes into the TX FIFO, NOT when they are shifted out. With AHB DMA contention (SDMMC0 ADMA2 + I2C/SPI system DMA), the TX FIFO only held ~70 bytes when PKTCOMP fired; `BDCON=0` terminated the SQI clock mid-page, committing only 70 bytes. TXBUFFREE showed 186 free slots confirming 186 bytes still in FIFO. Fix: after PKTCOMP, poll `STAT1.TXBUFFREE == 256` (full FIFO free = shift register also empty = last bit out) before clearing BDCON. Also fixed: SE command buffer was 256 bytes (an old pre-TXEMPTY artifact); the SST26 received 4 cmd + 252 garbage bytes. Fixed to 4 bytes. |

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
- SDMMC1 SD card logging: ADMA2 mode, 4-bit bus, GCLK4=100 MHz main + GCLK5=12 MHz slow, multi-block DMA up to 2.5 MB, 0 dropouts. Card detect on PC28 (GPIO active-LOW). Mount at `/fs/microsd` ✓
- Pin-mux arbitration (SQI1 mux=7 ↔ SDMMC1 mux=8): SQI loads params at boot, then remuxes to SDMMC1 for logging ✓
- SPI master (SERCOM3): `sam_spi.c` polled-mode driver, `hardware/sam_sercom_spi.h` DFP-verified, GCLK2=100 MHz, pins PC12/PC13/PC14/PC15 func C. PX4 bus table in `spi.cpp` ✓
- I2C master (SERCOM5): `sam_i2c_master.c` interrupt-driven driver, `hardware/sam_sercom_i2c.h` DFP-verified, GCLK2=100 MHz, 400 kHz bus, pins PC25=SDA/PC26=SCL func D. PX4 bus table in `i2c.cpp` ✓
- DSU DID read: enables APB clock (MCLK_ID=1), reads 0x44000120, extracts REVISION[31:28] → `ver all` shows real silicon rev ✓
- Chip UID (FUSES_CALOTP UNIQID): reads factory-programmed 96-bit serial from 0x0A0071E0 → `ver all` shows unique PX4GUID per board ✓
- EIC driver (`sam_eic.c`): 16-channel async-mode external interrupt controller; GCLK3 for debounce; `px4_arch_gpiosetevent()` wired via `pic32czca90_gpiosetevent.c`; builds and links but NOT hardware-tested (no active DRDY consumer) ✓

**Pending — by priority for a flying PX4 system:**

> **Storage (done):** SD card → flight logs (`/fs/microsd`). SQI1 → params/caldata/dataman.
> **Sensors (verified on hardware):** ICM45686 (SPI3 DMA), BMI088 accel+gyro (I2C5 DMA), BMM150 mag (I2C5 DMA) — all 0 bad transfers.
>
> **DFP peripheral names (CA90 — do not use SAMD5x names):**
> - SD/MMC controller → `SDMMC` (`component/sdmmc.h`) — not "SDHC" or "HSMCI"
> - System DMA → `DMA` (`component/dma.h`) — not "DMAC"
> - SQI flash → `SQI` (`component/sqi.h`) — has integrated BD-DMA; does **not** use system DMA
> - USB → `USBHS` (`component/usbhs.h`) — high speed; not "USBFS"
>
> **DFP location:** `C:/Users/I74182/.mchp_packs/Microchip/PIC32CZ-CA90_DFP/1.7.168/CA90/include/`

#### P1 — SD Card Logging — ADMA2 mode (flight logs → /fs/microsd) — DONE ✓
> **Completed.** SDMMC1 ADMA2 mode working. Multi-block DMA up to 2.5 MB verified, 0 dropouts.
>
> **Hardware:** SDMMC1 (not SDMMC0). Micro-SD socket wired to SDMMC1 pins
> (PC30/CLK, PG03/CMD, PC31/DAT0, PG00/DAT1, PG01/DAT2, PG02/DAT3, PC28/CD).
>
> **Pin sharing:** SDMMC1 and SQI1 share physical pins (mux=8 vs mux=7).
> Pin-mux arbitration code exists; SQI loads params first, then remuxes to SDMMC1.
>
> **GCLK assignment:**
> - SDMMC1 main clock: GCLK4 → 100 MHz (PLL0/3); `GCLK_PCHCTRL[60] = GEN(4)|CHEN`
> - SDMMC1 slow clock: GCLK5 → 12 MHz (PLL0/25); `GCLK_PCHCTRL[61] = GEN(5)|CHEN`
>
- `hardware/sam_sdmmc.h` — DFP-verified; SDMMC1 base `0x460A0000`, `GCLK_ID=60`, `GCLK_ID_SLOW=61`, `MCLK_ID_AHB=71`, `MCLK_ID_APB=72` ✓
- `sam_sdmmc.c` — ADMA2 mode; NuttX SDIO lower-half ✓
- Board init: card detect on PC28, `mmcsd_slotinitialize()`, mount at `/fs/microsd` ✓
- **Win:** `ls /fs/microsd` succeeds; logger writes with 0 dropouts ✓

#### P2 — SQI Flash Parameter Storage (params/caldata/dataman) — DONE ✓
> **Completed 2026-05-13.** Params persist across reboot (hardware-verified).
> See `docs/sqi_hardware_behavior.md` for full silicon quirk documentation.
>
> **CRITICAL: SQI1 and SDMMC1 share pins — mux=7 (SQI) vs mux=8 (SDMMC).**
> Currently SQI1 owns the pins full-time. When SDMMC1 is added, pin mux
> switching is required between SQI and SDMMC operations.
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
> and SPI3 TX+RX dual-channel (ICM45686 FIFO bursts). NOINC flag for NULL-buffer handling.
> MPU nocache region at 0x200F0000 for BD structs.
>
- `hardware/sam_dma.h` — DFP-verified register defs, trigger IDs, BD descriptor struct ✓
- `sam_dmac.c` — BD-based transfer engine, NuttX `sam_dmachannel`/`sam_dmastart`/`sam_dmastop` API ✓
- I2C5: RX DMA for bulk sensor FIFO reads; `i2c_stats` shows `dma_s=1.6M, dma_d=1.6M` ✓
- SPI3: TX+RX dual-channel DMA with NOINC for NULL buffers; `icm45686: bad transfer: 0` ✓

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

#### P5 — SPI Master → IMU (SERCOM3) — DONE ✓
> **Completed.** SPI DMA driver on SERCOM3. GCLK2=100 MHz.
> Pins: PC12=MOSI, PC13=SCK, PC14=CS (GPIO), PC15=MISO, function C.
> ICM45686 running at full rate, 0 bad transfers, 0 FIFO overflows.
>
- `hardware/sam_sercom_spi.h` — DFP-verified register definitions ✓
- `sam_spi.c` — NuttX `spi_dev_s` with TX+RX DMA (polled fallback for ≤4 bytes) ✓
- `spi.cpp` — PX4 board SPI bus table and chip-select functions ✓
- **Win:** `icm45686: bad transfer: 0` over 307k+ rate control cycles ✓

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

#### P9 — PROGMEM Crash Log (crash survives reboot)
- `sam_progmem.c` — `up_progmem_*` API using `hardware/sam_fcw.h` (already exists)
- `NSECTORS=2` → last 8 KB of PFM reserved at runtime; no linker change needed
- defconfig: `CONFIG_PIC32CZCA90_PROGMEM=y`, `CONFIG_MTD_PROGMEM=y`, `CONFIG_BOARD_CRASHDUMP=y`
- Board config: `HAS_PROGMEM`; board init: `board_hardfault_init(2, true)`
- **Win:** deliberate PANIC(); reboot; crash dump visible on console

#### P10 — ADC / Battery Monitoring
- `hardware/sam_adc.h` from DFP `component/adc.h`; ADC0 `GCLK_ID=41`, `MCLK_ID_APB=51`
- ADC driver (ADC0–ADC3, 12-bit SAR); wire battery voltage divider to PX4 battery_status

#### P11 — CAN-FD (MCAN)
- `hardware/sam_mcan.h` from DFP `component/can.h`; `sam_mcan.c`
- CAN3 (J701) `GCLK_CH=48`, CAN4 (J702) `GCLK_CH=49`; ATA6561 transceivers on-board

#### P12 — Safety / Misc
- `hardware/sam_wdt.h` from DFP `component/wdt.h`; `sam_wdt.c` — 4 s timeout, EWINT at 2 s
- ~~DSU: un-stub `board_mcu_version()` / `board_get_uuid32()`~~ — **DONE ✓** (DID at 0x44000120, UID at 0x0A0071E0)

**Missing register headers** (drivers blocked until these exist):

| Header needed              | DFP source                  | Blocks              |
|----------------------------|-----------------------------|---------------------|
| `hardware/sam_usbhs.h`     | `component/usbhs.h`        | USB HS driver (P8)  |
| `hardware/sam_adc.h`       | `component/adc.h`           | ADC driver (P10)    |
| `hardware/sam_mcan.h`      | `component/can.h`           | CAN-FD driver (P11) |
| `hardware/sam_wdt.h`       | `component/wdt.h`           | Watchdog (P12)      |
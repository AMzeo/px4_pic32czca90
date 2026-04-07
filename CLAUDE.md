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
        ├── GCLK0 (SRC=6=PLL0_1, DIV=1) → 300 MHz
        │     └── MCLK.CLKDIV[1]=2  →  150 MHz  →  CPU
        └── GCLK1 (SRC=6=PLL0_1, DIV=2) → 150 MHz → SERCOM1 core clock
              └── SERCOM1 BAUD=64730 → 115200 baud

OSCULP32K → GCLK3 (SRC=3, DIV=1) → 32.768 kHz → SERCOM slow clock
```

**Key clock facts:**
- DFLL48M is not configured in software — it is already running from hardware reset
- XOSC0 (MEMS oscillator) is not used — PLL0 references DFLL directly
- `sam_dfll_configure()` must NOT be called — CA90 has no DFLLSYNC register
- MCLK.CLKDIV[1]=2 is written before switching GCLK0 to PLL0 and kept permanently;
  effective CPU speed = 150 MHz (`BOARD_CPU_FREQUENCY = DPLL0_FREQUENCY / 2`)
- **300 MHz operation:** restoring CLKDIV[1]=1 after the GCLK switch causes a system
  hang (board completely dead). Root cause unknown — likely requires FCR explicit
  configuration or executing the divider-restore from SRAM to avoid instruction-fetch
  issues during the CPU clock step. Left at 150 MHz until resolved.

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
| BOARD_CPU_FREQUENCY set to wrong value | `board.h` (both) | SysTick wrong frequency | `BOARD_CPU_FREQUENCY` = `BOARD_DPLL0_FREQUENCY / 2` = 150 MHz (MCLK.CLKDIV[1]=2 permanent); restoring CLKDIV[1]=1 at runtime causes system hang — 300 MHz deferred |
| VTOR not set explicitly | `sam_start.c` | Early HardFault routed to BootROM vectors | Added explicit NVIC_VECTAB write first in `__start()` |
| chip.h ITCM_SIZE wrong | `arch/arm/include/pic32czca90/chip.h` | 64 KB reported but CA90 has 128 KB ITCM | Changed to `128*1024` |
| `.vectors` placed at PFM (0x0C000000) | `script.ld`, `linker.ld` | No console output after programming — BootROM reads SP/PC from BFM (0x08000000) which was unprogrammed (0xFF) | Added `boot_rom` region at 0x08000000; `.vectors` → `boot_rom`, `.text` → `flash` (VECTOR_REGION=boot_rom convention) |
| arm_vectors.c SysTick/PendSV to `my_hardfault` | `arch/arm/src/armv7-m/arm_vectors.c` | ABDE appears but NuttX hangs immediately — first SysTick tick hits infinite loop before scheduler starts | Routes [11] SVC, [14] PendSV, [15] SysTick → `exception_common`; only true faults [2..10,12..13] use `my_hardfault` |
| IRQ table SAMD5x-derived (137 IRQs) | `arch/arm/include/pic32czca90/pic32czca90_irq.h` | SERCOM1 DRE at wrong offset (50 vs 64); interrupt-driven drivers would bind wrong vectors | Complete rewrite from Harmony `device_vectors.h` (PIC32CZ8110CA80208 DFP): 222 IRQs, SERCOM0-9 ×7 each, TCC0-9, CAN0-5 |
| chip.h peripheral counts SAMD5x-derived | `arch/arm/include/pic32czca90/chip.h` | NSERCOM=8, NTC=8, NTCC=5, NCAN=2 — all wrong for CA90 | Updated to NSERCOM=10, NTC=0 (CA90 has no TC, only TCC), NTCC=10, NCAN=6 |
| DSU DID address wrong (SAMD5x value) | `board_mcu_version.c`, `board_identity.c` | `ver mcu` / `ver px4guid` / `ver all` hang → Hard Fault (unmapped read at 0x41002018) | CA90 DSU at 0x44000000 (APB A), DID offset = 0x0120 (DFP-verified) → `CA90_DSU_DID = 0x44000120` |
| DSU reads cause CPU bus stall | `board_mcu_version.c`, `board_identity.c` | `ver mcu`, `ver px4guid`, `ver all` hang system completely — CPU stalls waiting for AHB response | DSU APB clock not enabled (MCLK_ID_APB_DSU missing from sam_mclk.h); bus stall on clock-gated peripheral. Stubbed with fixed return values until DSU MCLK ID is known and clock is enabled |
| rcS `ver all` in startup script blocked NSH | `ROMFS/px4fmu_common/init.d/rcS` | System printed 3 lines of ver output then hung — remaining lines in TX buffer couldn't drain because CPU was stalled at DSU access | Removed `ver all` from rcS; DSU stall also fixed by stubbing board_identity/mcu_version |
| `param set-default` doesn't set active in-memory value | `rc.board_defaults` | `cdcacm_autostart` runs despite `SYS_USB_AUTO -1` set via `set-default`; ~5 s after boot idle: "Device /dev/ttyACM0 does not exist" spam, system appears stuck | `param set-default` only updates stored default — when param import fails (no storage), in-memory value stays at compiled-in default. Changed to `param set SYS_USB_AUTO -1` |

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
- BOARD_CPU_FREQUENCY = 150 MHz (MCLK.CLKDIV[1]=2 kept permanently; 300 MHz hangs — see Clock Tree) ✓
- I-cache and D-cache enabled (CONFIG_ARMV7M_ICACHE/DCACHE=y) ✓
- MPU enabled (CONFIG_ARM_MPU=y, 16 regions) ✓
- chip.h ITCM_SIZE corrected to 128 KB ✓
- chip.h peripheral counts corrected: NSERCOM=10, NTC=0, NTCC=10, NCAN=6 ✓
- Register headers (sam_oscctrl.h, sam_mclk.h, sam_gclk.h) verified vs DFP ✓
- arm_vectors.c: SysTick [15] and PendSV [14] route through `exception_common` ✓
- pic32czca90_irq.h: 222 IRQs from Harmony device_vectors.h (was SAMD5x 137) ✓
- NSH console alive, nx_start() reached ✓
- NSH interactive prompt (`nsh>`) verified on hardware; full keyboard input working ✓
- `cdcacm_autostart` suppressed via `param set SYS_USB_AUTO -1` (USB driver not yet implemented) ✓

**Pending — by priority for a flying PX4 system:**

#### P1 — DMA (prerequisite for high-bandwidth I2C/SPI/USB)
1. Add missing register header `hardware/sam_dmac.h` (DMAC base, channel regs, descriptor layout)
2. Implement `sam_dmac.c` — 32-channel DMA driver (port from samd5e5/sam_dmac.c, update regs)
3. Configure MPU nocache region for DMA descriptors (linker already has 64 KB at 0x200F0000)

#### P2 — I2C master (prerequisite for IMU, mag, baro)
4. Add missing register header `hardware/sam_sercom_i2c.h` (I2C mode CTRLA/B, BAUD, STATUS)
5. Implement `sam_i2c_master.c` (port from samd5e5/sam_i2c_master.c)
6. Fix Kconfig wiring: `PIC32CZCA90_SERCOM*_ISI2C` must `select PIC32CZCA90_HAVE_I2C_MASTER`
   (currently Make.defs compiles sam_i2c_master.c on `HAVE_I2C_MASTER` but Kconfig never sets it)
7. Wire up I2C bus in `boards/microchip/czca90curiosity/src/init.c` (currently stub returns -1)

#### P3 — SPI master (prerequisite for SPI sensors / external flash)
8. Add missing register header `hardware/sam_sercom_spi.h`
9. Implement `sam_spi.c` (port from samd5e5/sam_spi.c)
10. Fix Kconfig wiring: `PIC32CZCA90_SERCOM*_ISSPI` must `select PIC32CZCA90_HAVE_SPI`

#### P4 — Timer/PWM (prerequisite for motor output)
11. Add missing register header `hardware/sam_tcc.h`
12. Implement `sam_oneshot.c`, `sam_oneshot_lowerhalf.c`, `sam_freerun.c` (timer abstractions over TCC)
13. Implement TCC PWM driver — TCC0 has multiple match/capture channels → PWM outputs for motors
14. Implement PX4 `io_timer` abstraction over TCC (in `boards/microchip/czca90curiosity/src/`)
15. Add TCC GCLK channel assignments to `sam_sercom.c` / board clock init

#### P5 — USB CDC-ACM (MAVLink over J200)
16. Add missing register header `hardware/sam_usb.h` (USB FS device controller regs)
17. Implement `sam_usb.c` — USB full-speed device driver (CA90 has FS USB, not HS)
18. Remove `-ENODEV` stubs in `boards/microchip/czca90curiosity/src/init.c`
19. Enable `CONFIG_USBDEV`, `CONFIG_CDCACM` in defconfig
20. Restore `param set SYS_USB_AUTO 0` in rc.board_defaults once USB driver works

#### P6 — ADC / battery monitoring
21. Add missing register header `hardware/sam_adc.h`
22. Implement ADC driver (ADC0/ADC1, 16 channels each, 12-bit SAR)
23. Wire battery voltage divider channel to PX4 battery_status module

#### P7 — CAN-FD
24. Add missing register header `hardware/sam_mcan.h` (MCAN — ISO 11898-1 CAN-FD)
25. Implement `sam_mcan.c` (port from SAMV7 mcan driver — same IP block)
26. Enable CAN0-5 in defconfig and board init as needed

#### P8 — Safety / misc
27. Implement `sam_wdt.c` — watchdog timer (flight safety requirement)
28. Implement `sam_eic.c` — external interrupt controller (needed for sensor data-ready IRQs)
29. Add `hardware/sam_eic.h`, `hardware/sam_wdt.h` register headers
30. Implement SDHC driver (`hardware/sam_sdhc.h` + driver) for SD card data logging
31. Implement SQI driver (`hardware/sam_sqi.h` + driver) if external flash needed (CA90 uses SQI, not QSPI)

**Missing register headers summary** (drivers blocked until these exist):

| Header needed | Blocks |
|---------------|--------|
| `hardware/sam_sercom_i2c.h` | I2C driver |
| `hardware/sam_sercom_spi.h` | SPI driver |
| `hardware/sam_dmac.h` | DMA driver |
| `hardware/sam_tcc.h` | TCC PWM/timer driver |
| `hardware/sam_usb.h` | USB driver |
| `hardware/sam_adc.h` | ADC driver |
| `hardware/sam_mcan.h` | CAN-FD driver |
| `hardware/sam_eic.h` | EIC driver |
| `hardware/sam_wdt.h` | Watchdog driver |
| `hardware/sam_sdhc.h` | SDHC driver |
| `hardware/sam_sqi.h` | SQI (external flash) driver |
# PIC32CZ CA90 Port — Developer Reference Guide

## What This Is

This repo ports PX4 autopilot firmware to the **Microchip PIC32CZ CA90**
(Cortex-M7, 300 MHz, 8 MB Flash, 1 MB SRAM) running on the
**Curiosity Ultra board (EV16W43A)**, using NuttX as the RTOS.

If you're confused about which file to edit or how things connect — this
guide is for you.

---

## The Three-Layer Stack

```
┌─────────────────────────────────────────────────────┐
│  PX4 APPLICATION LAYER                              │
│  boards/microchip/czca90curiosity/                  │
│  Flight stack, uORB, MAVLink, drivers               │
├─────────────────────────────────────────────────────┤
│  NuttX RTOS BOARD LAYER                             │
│  platforms/nuttx/NuttX/nuttx/boards/arm/            │
│       pic32czca90/pic32czca90-curiosity/            │
│  Board-specific init, LEDs, buttons, mounts         │
├─────────────────────────────────────────────────────┤
│  NuttX RTOS CHIP LAYER                              │
│  platforms/nuttx/NuttX/nuttx/arch/arm/src/          │
│       pic32czca90/                                  │
│  Clock, GPIO, UART, IRQ, startup — talks directly   │
│  to hardware registers                              │
└─────────────────────────────────────────────────────┘
              ↓ hardware
         PIC32CZ CA90 silicon
```

**Rule of thumb:**
- Hardware register wrong? → Edit the **chip layer** `hardware/` headers
- Boot sequence wrong? → Edit `sam_start.c` or `sam_clockconfig.c`
- Console UART not working? → Edit `sam_lowputc.c` or `board.h`
- LED or button wrong? → Edit **board layer** `src/pic32czca90_*.c`
- PX4 module/driver missing? → Edit **PX4 layer** `boards/microchip/czca90curiosity/`

---

## Complete File Map

### Chip Layer — `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/`

| File | What it does | Edit when... |
|---|---|---|
| `hardware/pic32czca90_memorymap.h` | ALL peripheral base addresses | Peripheral address is wrong / missing |
| `hardware/sam_oscctrl.h` | OSCCTRL register offsets and bit fields | Clock source won't start |
| `hardware/sam_gclk.h` | GCLK register layout + peripheral channel IDs | Peripheral gets no clock |
| `hardware/sam_mclk.h` | MCLK APB bus gate registers | APB peripheral won't enable |
| `hardware/sam_usart.h` | SERCOM USART register layout | UART frame/baud problems |
| `hardware/sam_port.h` | PORT (GPIO) register layout | Pin configuration wrong |
| `hardware/pic32czca90_pinmap.h` | Pin constants (which port/pin/function) | Wrong pins assigned to peripheral |
| `hardware/sam_nvmctrl.h` | Flash controller — **DO NOT USE**, CZCA90 uses FCR/FCW instead | — |
| `sam_clockconfig.c/.h` | Full clock bring-up (XOSC→DPLL→300 MHz) | Clock frequency wrong |
| `sam_lowputc.c/.h` | Early polled UART (console before OS) | No output at all on VCP |
| `sam_start.c` | Reset entry point, .data/.bss copy | Board hangs before any output |
| `sam_irq.c` | NVIC setup, IRQ enable/disable | Interrupts not working |
| `sam_timerisr.c` | SysTick ISR | OS tick not running |
| `sam_serial.c` | Full interrupt-driven UART driver | /dev/ttyS not working |
| `sam_port.c` | GPIO driver | GPIO ioctl broken |
| `sam_gclk.c` | GCLK helper functions | Clock enable sequences |
| `sam_sercom.c` | SERCOM base init (shared by UART/SPI/I2C) | All SERCOM peripherals broken |
| `Make.defs` | Source file list for Make builds | Adding a new .c file |
| `Kconfig` | Feature configuration symbols | Adding a new Kconfig option |

### Board Layer — `platforms/nuttx/NuttX/nuttx/boards/arm/pic32czca90/pic32czca90-curiosity/`

| File | What it does | Edit when... |
|---|---|---|
| `include/board.h` | Clock frequencies, console config, LED/button indices | Any board-level constant wrong |
| `scripts/linker.ld` | Memory regions for linker | Linker errors, wrong flash/RAM layout |
| `configs/nsh/defconfig` | NuttX build configuration (what features are on) | Changing enabled features |
| `src/pic32czca90_boot.c` | `sam_board_initialize()` — first board init | Early board-level hardware setup |
| `src/pic32czca90_bringup.c` | OS-level init (filesystems, device registration) | Adding a new device/filesystem |
| `src/pic32czca90_appinit.c` | `board_app_initialize()` — PX4 entry point | Changing what starts at boot |
| `src/pic32czca90_autoleds.c` | LED state machine for boot status | LED behavior wrong |
| `src/pic32czca90_userleds.c` | User-controllable LED API | LED ioctl broken |

### PX4 Layer — `boards/microchip/czca90curiosity/`

| File | What it does | Edit when... |
|---|---|---|
| `default.px4board` | Which PX4 modules/drivers are compiled | Enabling/disabling PX4 features |
| `nuttx-config/include/board.h` | Same as NuttX board.h (PX4 build uses this copy) | Must keep in sync with NuttX board.h |
| `nuttx-config/scripts/script.ld` | Linker script used by PX4 build | Flash/RAM layout for PX4 |
| `nuttx-config/nsh/defconfig` | NuttX config for PX4 build | Changing NuttX features in PX4 build |
| `nuttx-config/scripts/Make.defs` | Make variables for PX4 NuttX build | Build system variables |
| `src/board_config.h` | PX4 board constants (GPIO, timer, bus IDs) | PX4 driver pin assignments |
| `src/init.c` | PX4 board init — `board_app_initialize` | Board-level PX4 init |
| `src/led.c` | LED control for PX4 armed/disarmed state | PX4 LED behavior |
| `init/rc.board_defaults` | Default parameter values | PX4 parameter defaults |
| `init/rc.board_sensors` | Sensor startup script | Sensor driver startup order |
| `init/rc.board_mavlink` | MAVLink port configuration | MAVLink UART assignment |
| `firmware.prototype` | Board identity for PX4 firmware ID | Firmware version info |

### PX4 Platform — `platforms/nuttx/src/px4/microchip/pic32czca90/`

| File | What it does |
|---|---|
| `hrt/hrt.c` | High-resolution timer using DWT cycle counter |
| `adc/adc.cpp` | ADC driver stub |
| `board_reset/board_reset.cpp` | Reboot via SCB AIRCR |
| `board_critmon/board_critmon.c` | Critical section monitor |
| `version/board_identity.c` | Board identity string |
| `version/board_mcu_version.c` | MCU version info |
| `io_pins/io_timer_stub.c` | IO timer stub (no PWM yet) |
| `include/px4_arch/micro_hal.h` | HAL macros for this MCU |

---

## The 5 Bugs That Were Fixed

These were the exact reasons the board had no response:

### Bug 1 — Wrong peripheral base addresses (pic32czca90_memorymap.h)
**Was:** SAMD5x addresses (OSCCTRL=0x40001000, GCLK=0x40001C00, PORT=0x41008000)  
**Is:** CZCA90 addresses (OSCCTRL=0x44040000, GCLK=0x44050000, PORT=0x44840000)  
**Effect:** Every register write went to the wrong silicon address. Clock config never worked. Board hung immediately.

### Bug 2 — OSCCTRL register offset order swapped (sam_oscctrl.h)
**Was:** EVCTRL at 0x0000, XOSCCTRL0 at 0x0014 (SAMD5x order)  
**Is:** XOSCCTRL0 at 0x0000, EVCTRL at 0x0008 (CZCA90 order per DS60001749K §18.7)  
**Effect:** XOSC0 enable writes hit the EVCTRL register. XOSC0 never started. DPLL had no reference. CPU stayed on internal RC oscillator.

### Bug 3 — Wrong GCLK channel ID for SERCOM4 (sam_gclk.h)
**Was:** `GCLK_CHAN_SERCOM4_CORE = 34` (SAMD5x value)  
**Is:** `GCLK_CHAN_SERCOM4_CORE = 25` (CZCA90 value per DS60001749K)  
**Effect:** SERCOM4 peripheral channel 34 doesn't exist on CZCA90. SERCOM4 received no clock. Console UART produced no output even if everything else was correct.

### Bug 4 — Console UART on wrong pins (pic32czca90_pinmap.h)
**Was:** `PORT_SERCOM4_PAD0 = PB08, PORT_SERCOM4_PAD1 = PB09`  
**Is:** `PORT_SERCOM4_PAD0 = PC21 (TX), PORT_SERCOM4_PAD1 = PC22 (RX)`  
**Source:** DS70005522C schematic (PKoB4_VCP signal trace → PC21/PC22)  
**Effect:** UART data was being driven on physically unconnected pins. Even if SERCOM4 was clocked correctly, no data reached the PKoB4 chip.

### Bug 5 — XOSC0 frequency wrong (board.h both copies)
**Was:** `BOARD_XOSC0_FREQUENCY = 24000000` (24 MHz)  
**Is:** `BOARD_XOSC0_FREQUENCY = 12000000` (12 MHz)  
**Source:** DS70005522C Table 2-2: Y300 = DSC6011JI2B-012.0000 = **12 MHz**  
**Also fixed:** `BOARD_GCLK5_DIV = 4 → 2` (to maintain 6 MHz DPLL reference)  
**Effect:** DPLL was locked to 150 MHz instead of 300 MHz. All timing was wrong.

---

## How to Add Support for a New Peripheral

Here is the exact sequence using CAN3 as an example:

**Step 1 — Verify the base address** from DS60001749K Table 8-x and confirm it's in `pic32czca90_memorymap.h`. CAN3 = 0x45860000 (APB D).

**Step 2 — Create a hardware header** `hardware/sam_can.h` with register offsets and bit definitions from the datasheet CAN section.

**Step 3 — Enable the APB clock** — add to `sam_clockconfig.c`:
```c
modifyreg32(SAM_MCLK_APBDMASK, 0, MCLK_APBDMASK_CAN3);
```

**Step 4 — Enable the GCLK channel** — add to `sam_clockconfig.c`:
```c
putreg32(GCLK_PCHCTRL_GEN(BOARD_GCLK_GEN_48M) | GCLK_PCHCTRL_CHEN,
         SAM_GCLK_PCHCTRL(GCLK_CHAN_CAN3));
```

**Step 5 — Configure pins** — add to `pic32czca90_pinmap.h`:
```c
#define PORT_CAN3_TX (PORT_PORTD | PORT_FUNC(6) | PORT_PIN(13) | PORT_FLAG_PMUXEN)
#define PORT_CAN3_RX (PORT_PORTC | PORT_FUNC(6) | PORT_PIN(29) | PORT_FLAG_PMUXEN | PORT_FLAG_INEN)
```

**Step 6 — Write the driver** `sam_can.c` using the register addresses from Step 2.

**Step 7 — Add to build** — add `sam_can.c` to `Make.defs` and `CMakeLists.txt` (conditionally on `CONFIG_PIC32CZCA90_CAN3`).

**Step 8 — Add Kconfig option** — add to `Kconfig`:
```
config PIC32CZCA90_CAN3
    bool "Enable CAN3 (Curiosity J701)"
    default n
```

**Step 9 — Enable in defconfig** — add to `configs/nsh/defconfig`:
```
CONFIG_PIC32CZCA90_CAN3=y
```

---

## How to Flash and Debug

**Flash via PKoB4 (on-board CMSIS-DAP debugger):**
```bash
openocd -f interface/cmsis-dap.cfg \
        -c "set CPUTAPID 0x6ba02477" \
        -c "adapter speed 4000" \
        -c "transport select swd" \
        -f target/cortexm7.cfg \
        -c "program build/pic32czca90curiosity_default.bin \
            0x0C000000 verify reset exit"
```

**Open the VCP console** (PKoB4 USB serial port appears as `/dev/ttyACM0` on Linux):
```bash
screen /dev/ttyACM0 115200
# or
minicom -b 115200 -D /dev/ttyACM0
```

**Expected output after successful boot:**
```
NuttShell (NSH) NuttX-12.x
nsh>
```

**If you see nothing at all:** The most common causes are:
1. Wrong flash address (should be 0x0C000000 not 0x00000000)
2. SERCOM4 not getting its GCLK (channel must be 25, not 34)
3. Console pins wrong (must be PC21/PC22 not PB08/PB09)

---

## Clock Tree Reference

```
12 MHz MEMS oscillator (Y300 on board)
    ↓ XOSC0
    ↓ GCLK5 (÷2 = 6 MHz)   ← DPLL reference
    ↓ DPLL0 (×50 = 300 MHz)
    ↓ GCLK0 (÷1)
    ↓ CPU = 300 MHz

DFLL48M (internal, open-loop)
    ↓ GCLK1 (48 MHz)
    ↓ SERCOM4 core (GCLK channel 25) → 115200 baud UART
    ↓ (future: USB)

OSCULP32K (internal 32 kHz)
    ↓ GCLK3 → WDT, RTC, SERCOM slow clock
```

**DPLL0 calculation:**
- Fin = GCLK5 = 12/2 = 6 MHz
- DPLL0 CTRLB: REFCLK = 0 (GCLK), GCLK = 5
- DPLL0 RATIO: LDR = 49, LDRFRAC = 0
- Fout = Fin × (LDR + 1) = 6 × 50 = **300 MHz** ✓

**SERCOM4 baud:**
- Fref = GCLK1 = 48 MHz
- BAUD = 65536 × (1 − 16 × 115200/48000000) = 63019 = 0xF61B
- Actual baud = 115200 × (1 − error) where error < 0.01%

---

## IRQ Numbers (CZCA90 — DS60001749K Table 10-2)

NuttX IRQ number = peripheral IRQ index + 16.

| Peripheral | IRQ indices | NuttX IRQ numbers |
|---|---|---|
| SERCOM0 | 55–61 | 71–77 |
| SERCOM1 | 62–68 | 78–84 |
| SERCOM2 | 69–75 | 85–91 |
| SERCOM3 | 76–82 | 92–98 |
| SERCOM4 | 83–89 | 99–105 |
| SERCOM4 RXC | 87 | **103** |
| SERCOM4 DRE | 85 | **101** |
| SERCOM4 TXC | 86 | **102** |
| CAN3 | 197 | 213 |
| CAN4 | 198 | 214 |
| GMAC ETH0 | 202 | 218 |

---

## Quick Troubleshooting

| Symptom | Most likely cause | File to check |
|---|---|---|
| No VCP output at all | Wrong flash address | `script.ld` — must be 0x0C000000 |
| VCP output stops at 'A' | Clock config hang | `sam_clockconfig.c`, `board.h` XOSC0_FREQUENCY |
| VCP output garbled | Wrong baud or GCLK | `board.h` SERCOM4_FREQUENCY, GCLK channel |
| Board resets in loop | Hard fault on startup | `pic32czca90_memorymap.h` addresses wrong |
| SysTick not running | Timer ISR not registered | `sam_timerisr.c`, `sam_irq.c` |
| SERCOM driver not found | APB clock not enabled | `sam_mclk.h` APBEMASK_SERCOM4 bit position |
| NSH prompt but no sensors | GCLK channel wrong for SPI | `sam_gclk.h` SERCOM2/3 core channel IDs |

---

## Datasheet References

| Topic | Document | Section |
|---|---|---|
| Memory map | DS60001749K | Section 8 (Tables 8-1 to 8-11) |
| NVIC IRQ numbers | DS60001749K | Section 10.2 (Table 10-2) |
| OSCCTRL registers | DS60001749K | Section 18.7 |
| GCLK registers | DS60001749K | Section 20.7 |
| MCLK registers | DS60001749K | Section 21.6 |
| SERCOM USART | DS60001749K | Section 35 |
| Board pinout | DS70005522C | Tables 2-3 to 2-11 |
| Board schematic | DS70005522C | Section 3.1 |

# PIC32CZ CA90 NuttX/PX4 Port Status

**Branch:** `pic32czca90-port`
**Board:** PIC32CZ CA90 Curiosity Ultra (EV16W43A)
**MCU:** PIC32CZ CA90 (Cortex-M7, 300 MHz), 8 MB PFM, 832 KB SRAM

---

## Hardware

| Item | Value |
|------|-------|
| MCU | PIC32CZ CA90 (Cortex-M7, 300 MHz) |
| Board | PIC32CZ CA90 Curiosity Ultra (EV16W43A) |
| BFM (Boot Flash) | 128 KB at 0x08000000 — vector table |
| PFM (Program Flash) | 8 MB at 0x0C000000 — code |
| SRAM | 832 KB at 0x20020000 |
| nocache region | 64 KB at 0x200F0000 — DMA buffers |
| Clock | DFLL48M → PLL0 → 300 MHz CPU, 150 MHz GCLK1 |
| Console | SERCOM1, PC04/PC07 (func D), 115200 baud → J700 PKOB4 USB |
| CA80 vs CA90 | Identical; CA90 adds HSM (Hardware Security Module) |

---

## Port Status

### Done — hardware verified

| Item | Notes |
|------|-------|
| Vector table at BFM (0x08000000) | `boot_rom` linker region; BootROM reads SP/PC from here |
| VTOR re-affirmed at `__start` | First instruction writes VTOR = 0x08000000 |
| BSS/data init | Pointer arithmetic fixed; all globals correct on boot |
| Clock tree | PLL0→300 MHz, GCLK1→150 MHz, GCLK3→32 kHz — matches Harmony |
| SERCOM1 console | PC04 TX / PC07 RX, func D, BAUD=64730→115200 — matches Harmony |
| showprogress A→B→D→E | Verified on hardware: clock+UART, earlyserial, GPIO, nx_start() |
| I-cache + D-cache | `CONFIG_ARMV7M_ICACHE/DCACHE=y`, write-through |
| MPU | `CONFIG_ARM_MPU=y`, 16 regions |

### Done — code fixed, awaiting hardware re-flash

| Item | Notes |
|------|-------|
| SysTick/PendSV/SVC vectors | `arm_vectors.c`: [14]/[15]/[11] now → `exception_common` (was `my_hardfault` → infinite loop) |
| IRQ table | `pic32czca90_irq.h`: 222 IRQs from Harmony `device_vectors.h` (was SAMD5x 137, wrong offsets) |
| chip.h peripheral counts | NSERCOM=10, NTC=0, NTCC=10, NCAN=6 (was SAMD5x-derived wrong values) |

### Pending — by priority

| Priority | Item |
|----------|------|
| P0 | Re-flash and verify NSH `nsh>` prompt (SysTick fix) |
| P1 | DMA driver (`sam_dmac.h` + `sam_dmac.c`, 32 channels) |
| P2 | I2C master (`sam_sercom_i2c.h` + `sam_i2c_master.c`) |
| P3 | SPI master (`sam_sercom_spi.h` + `sam_spi.c`) |
| P4 | TCC PWM driver (`sam_tcc.h` + driver, TCC0-9) |
| P5 | USB FS CDC-ACM (`sam_usb.h` + `sam_usb.c`) |
| P6 | ADC driver (`sam_adc.h`, ADC0/ADC1) |
| P7 | CAN-FD (`sam_mcan.h` + `sam_mcan.c`, CAN0-5) |
| P8 | WDT, EIC, SDHC, SQI |

---

## Key Architecture Notes

- **No TC peripheral**: CA90 uses TCC (TCC0-TCC9) for all timers — no TC0-7 as in SAMD5x.
- **10 SERCOMs** (SERCOM0-9), 7 IRQ vectors each (not 4 as in SAMD5x).
- **SERCOM1 IRQ base**: EXTINT+62 (DRE=EXTINT+64). Correct from Harmony DFP.
- **USB**: Full-speed USB FS (not USBHS). Different IP than SAMV7.
- **SQI not QSPI**: External flash uses SQI0/SQI1 (CA90-specific), not the SAMD5x QSPI IP.
- **CAN**: 6× MCAN (CAN0-5), same IP as SAMV7 — driver portable with address changes.

---

## Console Boot Sequence

Expected on `/dev/ttyACM0` at 115200 baud:

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
| E | Caches enabled, entering `nx_start()` |

Only `ABDE` (no NSH) indicates NuttX crashes before scheduler starts — typically a vector table or SysTick issue.

---

## Missing Register Headers

| Header | Blocks |
|--------|--------|
| `hardware/sam_sercom_i2c.h` | I2C driver |
| `hardware/sam_sercom_spi.h` | SPI driver |
| `hardware/sam_dmac.h` | DMA driver |
| `hardware/sam_tcc.h` | TCC PWM/timer driver |
| `hardware/sam_usb.h` | USB FS driver |
| `hardware/sam_adc.h` | ADC driver |
| `hardware/sam_mcan.h` | CAN-FD driver |
| `hardware/sam_eic.h` | EIC driver |
| `hardware/sam_wdt.h` | Watchdog driver |
| `hardware/sam_sdhc.h` | SDHC driver |
| `hardware/sam_sqi.h` | SQI (external flash) driver |

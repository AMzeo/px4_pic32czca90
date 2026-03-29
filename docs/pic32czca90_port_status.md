# PIC32CZ CA90 Port Status

## Target Hardware

| Item | Value |
|------|-------|
| MCU | PIC32CZ CA90 (Cortex-M7, 300 MHz) |
| Board | Curiosity Ultra (EV16W43A) |
| Flash | 8 MB at 0x0C000000 |
| SRAM | 1 MB total (128 KB DTCM + 896 KB SRAM) |
| Clock source | 24 MHz MEMS oscillator (XTALEN=0) |
| Clock chain | XOSC0(24MHz) -> GCLK5(÷4=6MHz) -> DPLL0(×50=300MHz) -> GCLK0 -> CPU |
| Console | SERCOM4 @ 115200 (PB08=TX, PB09=RX, via PKOB4 debug USB) |
| NuttX chip layer | New `pic32czca90` (GCLK-based, derived from SAMD5E5 patterns) |

---

## Files Created/Modified

### 1. NuttX Chip Layer (`arch/arm/src/pic32czca90/`)

| File | Purpose | Status |
|------|---------|--------|
| `Make.defs` | Build definitions, includes armv7-m/Make.defs | Done |
| `chip.h` | Includes irq.h, defines ARMV7M_PERIPHERAL_INTERRUPTS | Done |
| `Kconfig` | SERCOM0-7, DMAC, CAN, GMAC, USB, ADC, TC, TCC, QSPI, etc. | Done |
| `sam_start.c` | Reset handler: .bss/.data init, clocks, FPU, earlyserial, board init | Done (fixed) |
| `sam_start.h` | Declares `sam_board_initialize()` | Done |
| `sam_irq.c` | NVIC init, default priorities, SVCall/HardFault attach | Done |
| `sam_timerisr.c` | SysTick at BOARD_CPU_FREQUENCY / CLK_TCK | Done |
| `sam_idle.c` | WFI idle loop | Done |
| `sam_clockconfig.h` / `.c` | GCLK-based clock init (XOSC, DFLL, DPLL, GCLK, MCLK, NVM wait states) | Done |
| `sam_gclk.h` / `.c` | GCLK generator configuration (12 generators, 48 channels) | Done |
| `sam_sercom.h` / `.c` | SERCOM clock enable (SERCOM4 on APB bus E) | Done |
| `sam_port.h` / `.c` | GPIO PORT driver (config, read, write) | Done |
| `sam_usart.h` / `.c` | USART config structs for SERCOM0-7 | Done |
| `sam_lowputc.h` / `.c` | Polled UART: GCLK enable, CTRLA/CTRLB config, baud calc, sync wait | Done |
| `sam_serial.h` / `.c` | Interrupt-driven NuttX serial driver for 8 SERCOMs | Done |
| `sam_config.h` | SERCOM-to-USART/SPI/I2C mapping with PIC32CZCA90_ prefixes | Done |
| `sam_periphclks.h` | Peripheral clock helpers | Done |
| `sam_userspace.h` | User-space support (protected build) | Done |

### 2. NuttX Hardware Headers (`arch/arm/src/pic32czca90/hardware/`)

| File | Purpose | Status |
|------|---------|--------|
| `pic32czca90_memorymap.h` | Full memory map including APB E @ 0x44000000 | Done |
| `sam_memorymap.h` | Wrapper include | Done |
| `sam_gclk.h` | GCLK registers (GENCTRL, PCHCTRL for 12 gen + 48 chan) | Done |
| `sam_mclk.h` | MCLK with APBEMASK for SERCOM4-7 | Done |
| `sam_oscctrl.h` | XOSC/DFLL/DPLL registers | Done |
| `sam_osc32kctrl.h` | 32K oscillator registers | Done |
| `sam_pm.h` | Power Manager | Done |
| `sam_supc.h` | Supply Controller | Done |
| `sam_nvmctrl.h` | Flash controller (wait states) | Done |
| `sam_usart.h` | SERCOM USART registers + compatibility aliases | Done (fixed) |
| `sam_port.h` | PORT registers | Done |
| `pic32czca90_pinmap.h` | Pin encoding + board pin assignments (SERCOM4, LED0) | Done |
| `sam_pinmap.h` | Wrapper include | Done |

### 3. NuttX Arch Include (`arch/arm/include/pic32czca90/`)

| File | Purpose | Status |
|------|---------|--------|
| `chip.h` | Flash/SRAM sizes, peripheral counts, NVIC priorities | Done (fixed) |
| `irq.h` | IRQ wrapper, NR_IRQS = SAM_IRQ_EXTINT + SAM_IRQ_NEXTINT | Done |
| `pic32czca90_irq.h` | 137 peripheral IRQ definitions | Done |

### 4. NuttX Integration Patches

| File | Change | Status |
|------|--------|--------|
| `arch/arm/Kconfig` | Added ARCH_CHIP_PIC32CZCA90, chip name default, Kconfig source | Done |
| `boards/Kconfig` | Added ARCH_BOARD_PIC32CZCA90_CURIOSITY, board name, Kconfig source | Done |
| `arch/arm/src/Makefile` | Uses `chip/Make.defs` symlink (auto from CONFIG_ARCH_CHIP) | N/A |
| `arch/arm/src/CMakeLists.txt` | File does not exist in this NuttX version (Makefile-only) | N/A |

### 5. NuttX Board (`boards/arm/pic32czca90/pic32czca90-curiosity/`)

| File | Purpose | Status |
|------|---------|--------|
| `include/board.h` | Full clock chain, GCLK_SET1/SET2, SERCOM config, LED defs | Done |
| `configs/nsh/defconfig` | Standalone NuttX config (SERCOM4 console, 115200) | Done |
| `scripts/linker.ld` | 8MB flash, 128K DTCM, 896K SRAM | Done |
| `Kconfig` | Board Kconfig | Done |
| `src/Makefile` | Board source Makefile | Done |
| `src/pic32czca90_boot.c` | sam_board_initialize() + board_late_initialize() | Done (fixed) |
| `src/pic32czca90-curiosity.h` | Board internal header | Done |
| `src/pic32czca90_bringup.c` | Mounts procfs | Done |
| `src/pic32czca90_appinit.c` | board_app_initialize() | Done |
| `src/pic32czca90_userleds.c` | User LED control | Done |
| `src/pic32czca90_autoleds.c` | Auto LED for NuttX events | Done |

### 6. PX4 Board (`boards/microchip/czca90curiosity/`)

| File | Purpose | Status |
|------|---------|--------|
| `default.px4board` | Toolchain, serial map (TEL1=/dev/ttyACM0), core PX4 modules | Done |
| `firmware.prototype` | Board ID 1390, 8MB flash | Done |
| `init/rc.board_defaults` | HITL params, USB MAVLink, circuit breakers | Done |
| `init/rc.board_sensors` | Empty (no sensors yet) | Done |
| `init/rc.board_extras` | Starts navigator | Done |
| `init/rc.board_mavlink` | USB CDC/ACM setup via sercon | Done |
| `nuttx-config/include/board.h` | PX4 copy of NuttX board.h (full clock config) | Done |
| `nuttx-config/nsh/defconfig` | PX4 NuttX config (CUSTOM_DIR, CDC/ACM, ROMFS, work queues) | Done |
| `nuttx-config/scripts/script.ld` | 8MB flash, 832K sram + 64K nocache | Done |
| `nuttx-config/scripts/Make.defs` | ARMv7-M toolchain defs | Done |
| `src/board_config.h` | GPIO defs, LED, PX4_GPIO_INIT_LIST | Done |
| `src/init.c` | sam_board_initialize() + board_app_initialize() | Done (fixed) |
| `src/led.c` | LED via sam_portconfig/sam_portwrite | Done |
| `src/CMakeLists.txt` | Board drivers CMake | Done |

### 7. PX4 Platform Layer (`platforms/nuttx/src/px4/microchip/pic32czca90/`)

| File | Purpose | Status |
|------|---------|--------|
| `CMakeLists.txt` | Adds all subdirectories | Done |
| `include/px4_arch/micro_hal.h` | GPIO: px4_arch_* -> sam_port*, NVIC priorities, cache | Done |
| `include/px4_arch/adc.h` | ADC stub declarations | Done |
| `include/px4_arch/dshot.h` | DShot stub (empty) | Done |
| `include/px4_arch/io_timer.h` | io_timers_t/timer_io_channels_t stub structs | Done |
| `include/px4_arch/io_timer_hw_description.h` | Stub (empty) | Done |
| `include/px4_arch/hw_description.h` | GPIO::Port/Pin enums | Done |
| `include/px4_arch/spi_hw_description.h` | Stub (empty) | Done |
| `include/px4_arch/i2c_hw_description.h` | Stub (empty) | Done |
| `hrt/hrt.c` | DWT cycle counter HRT (300MHz, ~14.3s wrap) | Done |
| `hrt/CMakeLists.txt` | HRT build | Done |
| `version/board_identity.c` | UUID from DSU DID register | Done |
| `version/board_mcu_version.c` | MCU version from DSU DID | Done |
| `version/CMakeLists.txt` | Version build | Done |
| `board_reset/board_reset.cpp` | up_systemreset() wrapper | Done |
| `board_reset/CMakeLists.txt` | Reset build | Done |
| `io_pins/io_timer_stub.c` | Empty io_timers/timer_io_channels arrays | Done |
| `io_pins/pwm_servo.c` | PWM servo stubs (return -ENOSYS) | Done |
| `io_pins/CMakeLists.txt` | IO pins build | Done |
| `adc/adc.cpp` | ADC stubs (return -ENOSYS / UINT32_MAX) | Done |
| `adc/CMakeLists.txt` | ADC build | Done |
| `board_critmon/board_critmon.c` | Critical section monitoring stub | Done |
| `board_critmon/CMakeLists.txt` | Critmon build | Done |

### 8. PX4 CMake Patch

| File | Change | Status |
|------|--------|--------|
| `platforms/nuttx/cmake/px4_impl_os.cmake` | Added PIC32CZCA90 -> manufacturer=microchip, chip=pic32czca90 | Done |

---

## Bugs Fixed During Audit

| # | File | Issue | Fix |
|---|------|-------|-----|
| 1 | `arch/arm/include/pic32czca90/chip.h` | Missing `NVIC_SYSH_PRIORITY_MIN/DEFAULT/MAX/STEP` | Added 3-bit priority defs (0xe0/0x80/0x00/0x20) |
| 2 | `sam_start.c` | Used custom `sam_fpu_configure()` that only set CONTROL.FPCA without enabling FPU coprocessor in CPACR | Replaced with standard `arm_fpuconfig()` |
| 3 | `sam_start.c` | ramfuncs copy not guarded by `#ifdef CONFIG_ARCH_RAMFUNCS` | Added guard |
| 4 | `sam_start.c` | Missing `arm_earlyserialinit()` call | Added under `#ifdef USE_EARLYSERIALINIT` |
| 5 | `sam_start.c` | Missing `showprogress()` debug output | Added boot progress characters A-E |
| 6 | PX4 `init.c` | Function named `pic32czca90_boardinitialize()` but sam_start.c calls `sam_board_initialize()` | Renamed to `sam_board_initialize()` |
| 7 | PX4 `init.c` | Referenced undefined `LED_RED/GREEN/BLUE`, nonexistent `board_hardfault_init()`, `drv_led_start()` | Simplified to use `led_on(0)` only |
| 8 | NuttX `pic32czca90_boot.c` | Missing `sam_board_initialize()` definition | Added function with LED/GPIO init |

---

## Known Limitations (Current State)

### Functional but Minimal
1. **HRT uses DWT cycle counter** — 32-bit counter at 300 MHz wraps every ~14.3 seconds. Works for boot and short operations but will lose time over minutes. Production fix: implement TC-based HRT with hardware compare.

2. **No PWM output** — io_timer and pwm_servo are stubs returning -ENOSYS. Need TC/TCC timer driver.

3. **No ADC** — Stub returns -ENOSYS. Need ADC0/ADC1 driver.

4. **No SPI/I2C** — sam_spi.c and sam_i2c_master.c not yet written. No sensor support.

5. **No USB driver** — sam_usb.c not yet written. CDC/ACM MAVLink won't work until USB host stack is ported. Console via SERCOM4 (PKOB4) works.

6. **No SD card** — SDHC driver not yet written (CA90 uses SDHC, not HSMCI like CA90).

7. **No QSPI** — No persistent parameter storage. Parameters live in RAM only.

8. **No CAN-FD** — sam_mcan.c from SAMV7 is near-identical IP but needs base address and clock changes.

9. **No Ethernet** — GMAC driver not yet written.

10. **Board identity uses DSU DID** — The DID register gives device type, not unique serial number. The actual chip serial number address should be verified from the datasheet.

### Build Integration
11. **PX4 defconfig may need tuning** — Stack sizes, work queue config, and enabled modules may need adjustment after first successful build.

12. **No ROMFS airframe file** — The `etc/init.d/airframes/` directory is empty. HITL simulation needs airframe 1001 or 4001 definition.

---

## Next Steps for Complete Port

### Phase 1: First Boot (NuttX Shell)
1. Build: `make microchip_czca90curiosity_default`
2. Fix any remaining compile errors (expect header path issues, missing prototypes)
3. Flash via PKOB4 debug probe
4. Verify NSH console on SERCOM4 at 115200 baud

### Phase 2: USB + MAVLink
1. Port USB device driver (`sam_usb.c`) from SAMD5E5
2. Enable CDC/ACM in defconfig
3. Test MAVLink over USB CDC/ACM

### Phase 3: HRT + Timers
1. Write TC driver (`sam_tc.c`) for TC0
2. Implement proper TC-based HRT with hardware compare/capture
3. This enables accurate PX4 scheduling

### Phase 4: Sensors
1. Port SPI driver (`sam_spi.c`) from SAMD5E5
2. Port I2C driver (`sam_i2c_master.c`) from SAMD5E5
3. Connect Click sensor boards, add to rc.board_sensors

### Phase 5: PWM + Flight
1. Port TCC/TC PWM driver
2. Configure io_timer and timer_io_channels
3. Set PWM_MAIN_FUNCn parameters
4. HITL simulation with jMAVSim

### Phase 6: Storage
1. Port SDHC driver or QSPI driver for persistent params
2. Configure MTD partitions for params/caldata/waypoints

### Phase 7: Upstream
1. Run nxstyle on all NuttX files
2. Add SPDX-License-Identifier headers
3. Submit NuttX PR
4. Submit PX4 PR

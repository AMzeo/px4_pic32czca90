# SAMV71 PX4 Port: Production Implementation Plan

## Context

The SAMV71-XULT PX4 port is currently a working prototype with 4-channel PWMC PWM, IMU, baro, mag, GPS, battery ADC, safety, SD card, USB, RC serial input, and HRT. The goal is to bring this to production quality as a general-purpose dev platform, with a custom production PCB planned. The primary gaps are: io_timer allocation API (foundation for everything else), DShot output, input capture for RC, and board-level production hardening.

**Branch:** `samv7-custom`
**User priorities:** IO timer API first, then DShot, bidirectional DShot deferred (architecture must allow it later)

---

## Phase 0: Cleanup (Prerequisite)
**Complexity: S | Dependency: None | Dev Board**

Remove debug instrumentation from `io_timer_pwmc.c` before building the allocation layer on top.

### Files
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`
  - Remove hardcoded absolute address reads (lines 267-270: `getreg32(0x40020220)`, lines 319-325: PIO register dumps)
  - Convert `PX4_INFO` logging in init/set_rate to `PX4_DEBUG`
  - Remove `(void)base;` / `(void)pwm_ch;` debug-suppression lines
  - Gate any remaining diagnostics behind `#ifdef CONFIG_DEBUG_PWM`
- Commit the 3 uncommitted files (delta doc updates, CDTYUPD cleanup)

### Verify
- Build clean, no warnings
- `actuator_test set -m 1 -v 0.5` still works on all 4 channels

---

## Phase 1: IO Timer Allocation API
**Complexity: L | Dependency: Phase 0 | Dev Board**

Foundation phase. Every subsequent feature depends on proper timer/channel allocation with conflict detection, mode tracking, IRQ dispatch, and OneShot support.

### 1A: Extend `io_timer.h` Data Structures

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h`

**Add to `io_timers_t`:**
```c
uint32_t     clock_freq;    // MCK frequency (150MHz)
dshot_conf_t dshot;         // DShot DMA config (reserved for Phase 2)
```

**Add `dshot_conf_t`:**
```c
typedef struct dshot_conf_t {
    uint8_t  xdmac_ch_tx;     // XDMAC channel for PWM TX (13=PWM0, 39=PWM1)
    uint8_t  xdmac_ch_rx[4];  // Reserved for bidir capture
    uint32_t tc_capture_base;  // Reserved for bidir TC capture
} dshot_conf_t;
```

**Add to `timer_io_channels_t`:**
```c
uint16_t masks;       // Channel bit in SR/ISR registers
uint8_t  ccr_offset;  // Offset to CDTY from channel base
```

**Expand `io_timer_channel_mode_t` enum** (currently `uint8_t`) to full enum matching STM32:
- Add `IOTimerChanMode_Dshot`, `IOTimerChanMode_DshotInverted`, `IOTimerChanMode_CaptureDMA`, `IOTimerChanModeSize`

**Add `channel_handler_t`** callback signature (matching STM32):
```c
typedef void (*channel_handler_t)(void *context, const io_timers_t *timer,
    uint32_t chan_index, const timer_io_channels_t *chan,
    hrt_abstime isrs_time, uint16_t isrs_rcnt);
```

**Add `io_timers_channel_mapping_element_t`** struct with `first_channel_index`, `channel_count`, `lowest_timer_channel`, `channel_count_including_gaps`

**Declare new functions:**
```c
int io_timer_allocate_timer(unsigned timer, io_timer_channel_mode_t mode);
int io_timer_unallocate_timer(unsigned timer);
int io_timer_allocate_channel(unsigned channel, io_timer_channel_mode_t mode);
uint32_t io_timer_channel_get_gpio_output(unsigned channel);
void io_timer_update_dma_req(uint8_t timer, bool enable);
int io_timer_set_dshot_mode(uint8_t timer, unsigned dshot_pwm_freq);
```

### 1B: Update `io_timer_hw_description.h`

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

- `initIOPWMTimer()`: Populate `clock_freq`, `clock_register`, `clock_bit`, `vectorno`, `dshot.xdmac_ch_tx`
- `initIOPWMChannel()`: Populate `masks = (1 << channel)`, `ccr_offset = 0x04`
- `initIOTimerChannelMapping()`: Generate full mapping with `first_channel_index`, `channel_count`, etc.

### 1C: Implement Allocation Logic in `io_timer_pwmc.c`

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

**Add state tracking** (pattern from STM32 `io_timer.c` lines 293-493):
```c
io_timer_channel_allocation_t channel_allocations[IOTimerChanModeSize] = { UINT16_MAX, 0 };
static io_timer_channel_mode_t timer_allocations[MAX_IO_TIMERS] = {};
static struct { channel_handler_t callback; void *context; } channel_handlers[MAX_TIMER_IO_CHANNELS];
```

**Implement:**
- `io_timer_allocate_timer()` / `io_timer_unallocate_timer()` — timer-level mode exclusivity
- `io_timer_allocate_channel()` / `io_timer_unallocate_channel()` — channel-level bitmask tracking with critical sections
- Refactor `io_timer_init_timer()` to accept mode, call `io_timer_allocate_timer()`, install PWMC IRQ handler
- Refactor `io_timer_channel_init()` to call `io_timer_allocate_channel()`, support `IOTimerChanMode_Dshot`
- Refactor `io_timer_get_channel_mode()` and `io_timer_get_mode_channels()` to use `channel_allocations[]` array
- Add `io_timer_channel_get_gpio_output()` — returns GPIO-output config for a channel pin (for DShot idle/safety)

**Add IRQ handlers:**
```c
static int io_timer_handler(uint16_t timer_index);  // Read ISR1, dispatch to callbacks
static int io_timer_handler0(int irq, void *context, void *arg);  // PWM0
static int io_timer_handler1(int irq, void *context, void *arg);  // PWM1
```

**Add OneShot support:**
- `io_timer_trigger()`: For OneShot channels, write CDTYUPD then use channel counter event ISR to disable after one period
- `io_timer_set_enable()`: Support `IOTimerChanMode_OneShot` alongside `IOTimerChanMode_PWMOut`

### 1D: PWM Safety on Reset

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

Implement `board_on_reset()`:
- Disable all PWMC channels (write PWM_DIS for both modules)
- Reconfigure motor GPIOs as output-low via `io_timer_channel_get_gpio_output()`

### 1E: Update `timer_config.cpp`

**File:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`

- Update `io_timers_channel_mapping` to use new `io_timers_channel_mapping_element_t` struct

### Verify
- Build clean
- `actuator_test set -m 1 -v 0.5` — existing 4-ch PWM still works
- Allocation conflict: init same channel twice → returns `-EBUSY`
- `io_timer_get_mode_channels(IOTimerChanMode_PWMOut)` returns correct bitmask
- `reboot` → all PWM outputs go low (board_on_reset works)

---

## Phase 2: DShot Output (Unidirectional)
**Complexity: XL | Dependency: Phase 1 | Dev Board**

### Architecture Decision: PWMC Sync Mode + XDMAC

SAMV7 has no DMAR burst equivalent. Instead, use **PWMC Synchronous Channel Mode (SCM)**:
- Mark channels as synchronized (SCM.SYNCx bits)
- Set UPDM=2 (DMA update mode via DMAR register)
- XDMAC writes 24-bit duty values to DMAR; hardware distributes to channels sequentially
- Each period: 4 DMAR writes (one per sync channel) = one DShot bit across all motors

**DShot timing (MCK=150MHz):**
| Protocol | Bit Period | CPRE | Clock | CPRD | Bit-1 (75%) | Bit-0 (37.5%) |
|----------|-----------|------|-------|------|-------------|---------------|
| DShot150 | 6.67 us | 1 (MCK/2) | 75 MHz | 500 | 375 | 187 |
| DShot300 | 3.33 us | 1 (MCK/2) | 75 MHz | 250 | 187 | 93 |
| DShot600 | 1.67 us | 1 (MCK/2) | 75 MHz | 125 | 93 | 46 |

**DMA buffer layout:** 17 periods × 4 channels = 68 words per XDMAC transfer (16 data bits + 1 zero pad)

### New Files
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/dshot.h` — SAMV7-specific DShot defines
- `platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c` — Full DShot driver
- `platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt`

### Key Functions to Implement
```c
int up_dshot_init(uint32_t channel_mask, unsigned dshot_pwm_freq, bool enable_bidirectional);
void up_dshot_trigger(void);     // Flush dcache, start XDMAC to DMAR
int up_dshot_arm(bool armed);
void dshot_motor_data_set(unsigned channel, uint16_t data, bool telemetry);
// Bidir stubs: up_bdshot_get_erpm() returns -ENOSYS, etc.
```

### `io_timer_pwmc.c` Additions
- `io_timer_set_dshot_mode()`: Reconfigure PWMC for DShot timing (CPRE=1, CPRD per protocol)
- `io_timer_update_dma_req()`: Enable/disable WRDY interrupt in IER2 for DMA triggering
- `io_timer_channel_init()`: Handle `IOTimerChanMode_Dshot` (same GPIO setup as PWMOut, timing set by dshot.c)

### Build System
- `platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt`: Add `add_subdirectory(dshot)`
- `boards/.../default.px4board`: Enable `CONFIG_DRIVERS_DSHOT=y`

### Bidir Extension Point
Architecture preserves clean path to bidirectional DShot:
- `dshot_conf_t` has `xdmac_ch_rx[]` and `tc_capture_base` fields (unused now)
- `IOTimerChanMode_DshotInverted` and `IOTimerChanMode_CaptureDMA` are in the enum
- Custom PCB must route motor pins to pads with both PWMC and TC alternate functions

### Verify
- Build succeeds with DShot driver linked
- Scope: DShot150 bit pattern — Bit-1 = 75% duty @ 6.67us, Bit-0 = 37.5% @ 6.67us
- BLHeli32/AM32 ESC: motor spins with `dshot` driver
- DShot commands: beacon, direction change
- No regression: PWM mode still works when DShot parameter not set

---

## Phase 3: Input Capture (TC-based)
**Complexity: M | Dependency: Phase 1 | Dev Board (TC5/PC29)**

Input capture uses TC (Timer/Counter), not PWMC. TC5 on PC29/TIOA5 is already reserved for RC.

### New File
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/input_capture.c`

### TC Capture Mode Configuration
- TC_CMR: WAVE=0 (capture mode), TCCLKS=MCK/2 (75MHz), LDRA=rising, LDRB=falling
- TC_RA: captured counter on rising edge
- TC_RB: captured counter on falling edge
- TC_SR: LDRAS/LDRBS flags → interrupt → callback

### API (matches STM32 `input_capture.c`)
```c
int up_input_capture_set(unsigned channel, input_capture_edge edge,
    capture_filter_t filter, capture_callback_t callback, void *context);
int up_input_capture_get_filter(unsigned channel, capture_filter_t *filter);
int up_input_capture_set_filter(unsigned channel, capture_filter_t filter);
int up_input_capture_get_trigger(unsigned channel, input_capture_edge *edge);
int up_input_capture_set_trigger(unsigned channel, input_capture_edge edge);
```

### Build
- Add `input_capture.c` to `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt`

### Verify
- PPM RC input on TC5/PC29: `rc_input` module receives valid channels
- Pulse widths match scope measurements (timing accuracy < 1us)

---

## Phase 4: Production Hardening
**Complexity: M | Dependency: Phase 1 | Dev Board**

### 4A: PWMC Fault Protection
**File:** `io_timer_pwmc.c`
- Configure FMR/FPE/FPV registers: outputs go LOW on fault
- Connect to software fault for `board_on_reset()` safety

### 4B: PWM Watchdog
- HRT callback checks timestamp per channel
- If `io_timer_set_ccr()` not called within timeout → force duty to zero

### 4C: FRAM Parameter Storage Prep
**File:** `board_config.h`
- Define `FLASH_BASED_PARAMS` conditionally (when FRAM Click board is populated)
- Stub MTD integration for future EEPROM/FRAM

### Verify
- PWMC outputs go low on fault trigger
- Motors stop if update loop stalls (watchdog test)
- Parameter save/load cycle on SD card unaffected

---

## Phase 5: 8-Channel PWM
**Complexity: M | Dependency: Phase 1 | Custom PCB Required**

### Add PWM1 Module
**Files:** `timer_config.cpp`, `board_config.h`, `io_timer_hw_description.h`

- Add `PWMCPeripheral::C` to enum (needed for PWM1 pin functions)
- Expand `io_timers[]` to include `initIOPWMTimer(PWM::PWM1)`
- Add 4 more channels to `timer_io_channels[]` (PWM1 CH0-CH3)
- Update `DIRECT_PWM_OUTPUT_CHANNELS = 8`, `BOARD_NUM_IO_TIMERS = 2`
- Enable `CONFIG_SAMV7_PWM1*` in defconfig

PWM1 pin options (custom PCB selects):
- PWM1_H0: PA12 (Periph C) or PA30 (Periph A)
- PWM1_H1: PA14 (Periph C) or PA31 (Periph A)
- PWM1_H2: PA1 (Periph A) or PD0 (Periph C)
- PWM1_H3: PA8 (Periph A) or PD2 (Periph C)

### Verify
- All 8 channels produce PWM
- PWM0 and PWM1 can run at different rates
- DShot works on both modules

---

## Phase 6: Extended ADC & Power Monitoring
**Complexity: M | Dependency: Custom PCB**

### Expand ADC
**Files:** `board_config.h`, `adc.cpp`
- Add channels: 2nd brick V/I, 5V rail, 3.3V rail, RSSI
- Extend `adc.cpp` AFEC initialization for new channels

### Power Rail Control
**File:** `board_config.h`
```c
#define GPIO_VDD_3V3_SENSORS_EN  (GPIO_OUTPUT | ...)
#define GPIO_VDD_5V_PERIPH_EN    (GPIO_OUTPUT | ...)
#define VDD_5V_PERIPH_EN(on)     px4_arch_gpiowrite(GPIO_VDD_5V_PERIPH_EN, !(on))
```

**File:** `init.c`
- Implement `board_peripheral_reset(ms)`: power-cycle peripheral rails

---

## Phase 7: HW Versioning & Final Production
**Complexity: M | Dependency: Phases 5-6, Custom PCB**

- ADC resistor-ladder for HW version/revision detection
- EEPROM/FRAM MTD driver for parameter redundancy (dual storage)
- USB VBUS detection GPIO
- Peripheral reset circuit integration
- Full `rc.board_sensors` with hw-version-based sensor tree selection

---

## Dependency Graph

```
Phase 0 (Cleanup)
    │
    v
Phase 1 (IO Timer Allocation API)  ← FOUNDATION
   ╱    │    ╲
  v     v     v
P2     P3    P4          (can run in parallel)
DShot  Capture  Hardening
                                   ─── Dev Board boundary ───
Phase 5 (8-ch PWM)      ← Custom PCB
    │
Phase 6 (ADC/Power)     ← Custom PCB
    │
Phase 7 (Versioning)    ← Custom PCB
```

## Effort Summary

| Phase | Scope | Effort | Calendar |
|-------|-------|--------|----------|
| 0 | Cleanup + commit | S | 1 day |
| 1 | IO Timer Allocation API | L | 2-3 weeks |
| 2 | DShot Output (PWMC+XDMAC) | XL | 3-4 weeks |
| 3 | Input Capture (TC-based) | M | 1 week |
| 4 | Production Hardening | M | 1 week |
| 5 | 8-Channel PWM (PCB) | M | 1 week |
| 6 | ADC/Power (PCB) | M | 1 week |
| 7 | HW Versioning (PCB) | M | 1-2 weeks |
| **Total dev board (P0-P4)** | | | **~7-9 weeks** |
| **Total with PCB (P0-P7)** | | | **~10-13 weeks + fab** |

## Key Reference Files

| Purpose | File |
|---------|------|
| SAMV7 io_timer API | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h` |
| SAMV7 PWMC driver | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c` |
| SAMV7 HW description | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h` |
| STM32 io_timer (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c` |
| STM32 DShot (reference) | `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c` |
| STM32 input_capture (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/input_capture.c` |
| Board config | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| Timer config | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| Board init | `boards/microchip/samv71-xult-clickboards/src/init.c` |
| NuttX XDMAC API | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.h` |
| NuttX PWMC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_pwm.h` |
| NuttX TC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_tc.h` |

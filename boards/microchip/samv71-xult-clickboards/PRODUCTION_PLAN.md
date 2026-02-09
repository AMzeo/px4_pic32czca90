# SAMV71 PX4 Port: Production Implementation Plan

**Rev 2 — 2026-02-08** (addresses code review findings from Rev 1)

## Context

The SAMV71-XULT PX4 port is currently a working prototype with 4-channel PWMC PWM, IMU, baro, mag, GPS, battery ADC, safety, SD card, USB, RC serial input, and HRT. The goal is to bring this to production quality as a general-purpose dev platform, with a custom production PCB planned. The primary gaps are: io_timer allocation API (foundation for everything else), DShot output, input capture for RC, and board-level production hardening.

**Branch:** `samv7-custom`
**User priorities:** IO timer API first, then DShot, bidirectional DShot deferred (architecture must allow it later)
**Supersedes:** `PRODUCTION_READINESS.md` (stale — incorrectly states ADC missing, 3 TC channels)

---

## Known Risks & Corrections (from Rev 1 review)

| # | Severity | Finding | Correction in this rev |
|---|----------|---------|----------------------|
| 1 | Critical | `board_on_reset()` is a no-op (init.c:189) — safety hole exists NOW | Promoted to Phase 0 as production blocker |
| 2 | Critical | API convergence with STM32 io_timer underscoped — consumers (pwm_servo, dshot, RPMCapture, PPSCapture) expect full typed enum, allocation tables, channel mapping, callback signatures | Phase 1 reframed as "API convergence + safety closure + migration" with explicit consumer compatibility matrix |
| 3 | High | `PRODUCTION_READINESS.md` stale (says no ADC, 3 TC channels) | Marked superseded; Phase 0 adds deprecation header |
| 4 | High | Phase 3 input capture not integrated with current serial RC strategy (`rc.board_defaults` uses UART4) | Phase 3 now includes param/startup integration and coexistence plan |
| 5 | High | Phase 5 `PWMCPeripheral::C` missing from enum (only A/B); `MAX_IO_TIMERS` hardcoded to 4 not board-derived | Phase 1 fixes MAX_IO_TIMERS derivation; Phase 5 adds Periph C/D |
| 6 | Medium | Debug instrumentation (absolute `getreg32`, `PX4_INFO` spam) is production blocker, not cleanup nice-to-have | Phase 0 reframed as production blocker |
| 7 | Medium | Dead legacy files `io_timer_tc.c`, `io_timer_stub.c` still on disk (not in CMakeLists but confusing) | Phase 0 marks deprecated with header comments |
| 8 | Medium | `initIOTimerChannel()` Timer5 → index 4, but `MAX_IO_TIMERS=4` (0-3 valid) — latent bounds issue | Phase 1 fixes MAX_IO_TIMERS to be board-derived via `BOARD_NUM_IO_TIMERS` |

---

## Phase 0: Safety Closure & Cleanup (Production Blocker)
**Complexity: S | Dependency: None | Dev Board**

This phase closes an existing safety hole and removes debug instrumentation that obscures production code.

### 0A: Close `board_on_reset()` Safety Hole (CRITICAL)

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

`board_on_reset()` at line 189 is already wired and called at boot (line 251) but does nothing.
This means on any reset, PWM outputs remain in their last state — **motors can run uncontrolled**.

Implement immediately (no dependency on Phase 1 allocation API — use direct register writes with
local constants defined in init.c). `PWM_DIS_OFFSET` is not in any shared header — it is defined
locally in `io_timer_pwmc.c`. For safety-critical reset code, define concrete constants in init.c
rather than depending on driver internals.

**Required includes** (already present in init.c): `board_config.h` → `sam_gpio.h` (provides
`sam_configgpio()`, `GPIO_OUTPUT`, `GPIO_OUTPUT_CLEAR`, `GPIO_PORT_PIOx`, `GPIO_PIN*`);
`px4_arch/io_timer.h` (provides `timer_io_channels[]`).

```c
/*
 * Local PWMC register constants for reset safety.
 * Defined here (not imported from driver) so reset works even if
 * the driver has not been linked or initialized.
 */
#define SAMV7_PWM0_BASE     0x40020000
#define SAMV7_PWM_DIS       0x008       /* PWM Disable Register offset */
#define SAMV7_PWM_DIS_ALL   0x0F        /* Channels 0-3 disable mask */

__EXPORT void board_on_reset(int status)
{
    /* 1. Disable all PWM0 channels via hardware register */
    putreg32(SAMV7_PWM_DIS_ALL, SAMV7_PWM0_BASE + SAMV7_PWM_DIS);

    /* 2. Reconfigure each motor GPIO from peripheral-mux to output-low.
     *    GPIO encoding (sam_gpio.h):
     *      bits 21-23 = mode (GPIO_OUTPUT = 2 << 21)
     *      bits 16-20 = config (GPIO_CFG_DEFAULT = 0)
     *      bit  12    = initial value (GPIO_OUTPUT_CLEAR = 0 → low)
     *      bits 5-7   = port, bits 0-4 = pin
     *    We preserve port+pin from the channel's gpio_out and replace mode.
     */
    for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; i++) {
        uint32_t pin_id = timer_io_channels[i].gpio_out & (GPIO_PORT_MASK | GPIO_PIN_MASK);
        sam_configgpio(GPIO_OUTPUT | GPIO_CFG_DEFAULT | GPIO_OUTPUT_CLEAR | pin_id);
    }
}
```

**After Phase 1 lands:** Replace the loop body with `io_timer_channel_get_gpio_output()` for
consistency with other PX4 boards. The direct-register approach above is intentionally self-contained
so it works even before Phase 1 allocation API exists.

### 0B: Remove Debug Instrumentation (Production Blocker)

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`
- Remove hardcoded absolute address reads (lines 267-270: `getreg32(0x40020220)`, lines 319-325: PIO register dumps)
- Convert `PX4_INFO` logging in `io_timer_init_timer`/`io_timer_set_rate` to `PX4_DEBUG`
- Remove `(void)channel_handler;` / `(void)context;` debug-suppression lines (these will be used by Phase 1C)
- Remove `(void)base;` / `(void)pwm_ch;` if present
- Gate any remaining diagnostics behind `#ifdef CONFIG_DEBUG_PWM`

### 0C: Mark Legacy Files Deprecated

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c` — Add header: `/* DEPRECATED: TC-based PWM replaced by PWMC (io_timer_pwmc.c). Retained for reference only. Do not add to CMakeLists. */`
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_stub.c` — Same deprecation header

### 0D: Mark Stale Docs Superseded

**File:** `boards/microchip/samv71-xult-clickboards/PRODUCTION_READINESS.md`
- Add header: `> **SUPERSEDED** by [PRODUCTION_PLAN.md](PRODUCTION_PLAN.md) as of 2026-02-08. This document contains stale information (e.g., incorrectly states ADC is missing, references 3 TC-based PWM channels). Refer to the production plan for current status.`

### 0E: Commit Uncommitted Files

- Stage and commit: delta doc updates, CDTYUPD cleanup, PWM frequency fix doc

### Verify
- Build clean, no warnings
- `actuator_test set -m 1 -v 0.5` still works on all 4 channels
- `reboot` → oscilloscope confirms all PWM outputs go low immediately

---

## Phase 1: IO Timer API Convergence & Allocation
**Complexity: L | Dependency: Phase 0 | Dev Board**

Foundation phase. Reframed from "allocation API" to **full API convergence with STM32 io_timer** — because every consumer (pwm_servo, dshot, input_capture, RPMCapture, PPSCapture) expects the STM32 signature contract.

### Consumer Compatibility Matrix

These consumers call io_timer functions and must compile + work against the SAMV7 implementation:

| Consumer | Functions Called | Current SAMV7 Status |
|----------|----------------|---------------------|
| `pwm_servo.c` (SAMV7) | `io_timer_channel_init(ch, PWMOut, NULL, NULL)`, `io_timer_set_ccr()`, `io_timer_set_enable()` | Works (minimal) |
| `src/drivers/dshot/dshot.c` | `io_timer_allocate_timer()`, `io_timer_set_dshot_burst_mode()`, `io_timer_update_dma_req()`, `io_timer_unallocate_timer/channel()` | Not implemented |
| `src/drivers/pwm_out/PWMOut.cpp` | `io_timer_channel_init()`, `io_timer_set_pwm_rate()`, `io_timer_set_ccr()`, `io_timer_get_group()` | Partially works |
| `src/drivers/rpm/RPMCapture.cpp` | `io_timer_allocate_channel(ch, RPM)`, `io_timer_channel_get_gpio_output()`, `io_timer_unallocate_channel()` | Will fail |
| `src/drivers/pps_capture/PPSCapture.cpp` | `io_timer_allocate_channel(ch, PPS)`, `io_timer_channel_get_as_pwm_input()`, `io_timer_unallocate_channel()` | Will fail |
| `input_capture.c` (Phase 3) | `io_timer_channel_init(ch, Capture, handler, ctx)` with full callback signature | Will fail (wrong handler typedef) |
| `board_on_reset()` (50+ boards) | `io_timer_channel_get_gpio_output(i)` in loop | Not implemented |

### 1A: Converge `io_timer.h` with STM32 API

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h`

**Current state:** `io_timer_channel_mode_t` is `typedef uint8_t` with `#define` constants, `channel_handler_t` takes only `void *context`, `io_timers_t` has no `clock_freq`/`dshot`, structs missing `masks`/`ccr_offset`.

**Target state:** Match STM32 `io_timer.h` signatures exactly.

**Changes:**

1. **Fix `MAX_IO_TIMERS` derivation** (closes finding #8 bounds issue):
```c
#ifdef BOARD_NUM_IO_TIMERS
#define MAX_IO_TIMERS  BOARD_NUM_IO_TIMERS
#else
#define MAX_IO_TIMERS  2
#endif
```

2. **Replace `uint8_t` typedef + `#define` with proper enum** (matches STM32 line 72):
```c
typedef enum io_timer_channel_mode_t {
    IOTimerChanMode_NotUsed = 0,
    IOTimerChanMode_PWMOut  = 1,
    IOTimerChanMode_PWMIn   = 2,
    IOTimerChanMode_Capture = 3,
    IOTimerChanMode_OneShot = 4,
    IOTimerChanMode_Trigger = 5,
    IOTimerChanMode_Dshot   = 6,
    IOTimerChanMode_LED     = 7,
    IOTimerChanMode_PPS     = 8,
    IOTimerChanMode_RPM     = 9,
    IOTimerChanMode_Other   = 10,
    IOTimerChanMode_DshotInverted = 11,
    IOTimerChanMode_CaptureDMA = 12,
    IOTimerChanModeSize
} io_timer_channel_mode_t;
```

3. **Add `dshot_conf_t`** (before `io_timers_t`):
```c
typedef struct dshot_conf_t {
    uint8_t  xdmac_ch_tx;     // XDMAC channel for PWM TX (13=PWM0, 39=PWM1)
    uint8_t  xdmac_ch_rx[4];  // Reserved for bidir capture
    uint32_t tc_capture_base;  // Reserved for bidir TC capture
} dshot_conf_t;
```

4. **Extend `io_timers_t`** to match STM32 (add `clock_freq`, `dshot`):
```c
typedef struct io_timers_t {
    uint32_t     base;
    uint32_t     clock_register;
    uint32_t     clock_bit;
    uint32_t     clock_freq;     // NEW: MCK frequency (150MHz)
    uint32_t     vectorno;
    dshot_conf_t dshot;          // NEW: DShot DMA config
} io_timers_t;
```

5. **Extend `timer_io_channels_t`** (add `masks`, `ccr_offset`):
```c
typedef struct timer_io_channels_t {
    uint32_t  gpio_out;
    uint32_t  gpio_in;
    uint8_t   timer_index;
    uint8_t   timer_channel;
    uint16_t  masks;        // NEW: Channel bit in SR/ISR (1 << channel)
    uint8_t   ccr_offset;   // NEW: Offset to CDTY from channel base
} timer_io_channels_t;
```

6. **Fix `channel_handler_t` signature** to match STM32 (line 130):
```c
typedef void (*channel_handler_t)(void *context, const io_timers_t *timer,
    uint32_t chan_index, const timer_io_channels_t *chan,
    hrt_abstime isrs_time, uint16_t isrs_rcnt);
```

7. **Add `io_timers_channel_mapping_element_t`** struct (matches STM32 line 108):
```c
typedef struct io_timers_channel_mapping_element_t {
    uint32_t first_channel_index;
    uint32_t channel_count;
    uint32_t lowest_timer_channel;
    uint32_t channel_count_including_gaps;
} io_timers_channel_mapping_element_t;

typedef struct io_timers_channel_mapping_t {
    io_timers_channel_mapping_element_t element[MAX_IO_TIMERS];
} io_timers_channel_mapping_t;
```

8. **Update function declarations** to match STM32 signatures:
```c
int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode);  // ADD mode param
int io_timer_allocate_timer(unsigned timer, io_timer_channel_mode_t mode);
int io_timer_unallocate_timer(unsigned timer);
int io_timer_allocate_channel(unsigned channel, io_timer_channel_mode_t mode);
int io_timer_unallocate_channel(unsigned channel);
uint32_t io_timer_channel_get_gpio_output(unsigned channel);
uint32_t io_timer_channel_get_as_pwm_input(unsigned channel);
void io_timer_update_dma_req(uint8_t timer, bool enable);
int io_timer_set_dshot_mode(uint8_t timer, unsigned dshot_pwm_freq);
```

9. **Add externs** for channel mapping:
```c
__EXPORT extern const io_timers_channel_mapping_t io_timers_channel_mapping;
```

### 1B: Update `io_timer_hw_description.h`

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

1. **`initIOPWMTimer()`**: Populate `clock_freq = BOARD_MCK_FREQUENCY` (150MHz), `vectorno` (SAM_IRQ_PWM0=31 / SAM_IRQ_PWM1=60), `dshot.xdmac_ch_tx` (13 for PWM0, 39 for PWM1)
2. **`initIOPWMChannel()`**: Populate `masks = (1 << channel)`, `ccr_offset = 0x04` (CDTY offset within channel register block)
3. **Replace `io_timers_channel_mapping_t`** — remove the local struct definition (line 140-142, currently `uint32_t element[]` bitmask) and use the proper `io_timers_channel_mapping_element_t` from `io_timer.h`
4. **Rewrite `initIOTimerChannelMapping()`** to populate `first_channel_index`, `channel_count`, `lowest_timer_channel`, `channel_count_including_gaps` per timer

### 1C: Implement Allocation Logic in `io_timer_pwmc.c`

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

**Add state tracking** (pattern from STM32 `io_timer.c`):
```c
static io_timer_channel_allocation_t channel_allocations[IOTimerChanModeSize];
static io_timer_channel_mode_t timer_allocations[MAX_IO_TIMERS];
static struct { channel_handler_t callback; void *context; } channel_handlers[MAX_TIMER_IO_CHANNELS];
```

Initialize `channel_allocations[IOTimerChanMode_NotUsed] = UINT16_MAX` (all channels start as not-used).

**Implement:**
- `io_timer_allocate_timer()` / `io_timer_unallocate_timer()` — timer-level mode exclusivity with critical sections
- `io_timer_allocate_channel()` / `io_timer_unallocate_channel()` — channel-level bitmask tracking with `irqsave`/`irqrestore`
- Refactor `io_timer_init_timer()` to accept mode param, call `io_timer_allocate_timer()`, install PWMC IRQ handler
- Refactor `io_timer_channel_init()` to call `io_timer_allocate_channel()`, store callback+context, support `IOTimerChanMode_Dshot` and `IOTimerChanMode_OneShot`
- Refactor `io_timer_get_channel_mode()` and `io_timer_get_mode_channels()` to use `channel_allocations[]` array
- `io_timer_channel_get_gpio_output()` — derive GPIO output-low config from `timer_io_channels[channel].gpio_out`
- `io_timer_channel_get_as_pwm_input()` — return `timer_io_channels[channel].gpio_in`

**Add IRQ handlers:**
```c
static int io_timer_handler(uint16_t timer_index);  // Read ISR1, dispatch to channel_handlers[]
static int io_timer_handler0(int irq, void *context, void *arg);  // PWM0 → io_timer_handler(0)
static int io_timer_handler1(int irq, void *context, void *arg);  // PWM1 → io_timer_handler(1)
```

**Add OneShot support:**
- `io_timer_trigger()`: For OneShot channels, write CDTYUPD then use channel counter event ISR to disable after one period
- `io_timer_set_enable()`: Support `IOTimerChanMode_OneShot` alongside `IOTimerChanMode_PWMOut`

**Migration risk mitigation:**
- `pwm_servo.c` currently calls `io_timer_init_timer(timer)` with no mode — update to `io_timer_init_timer(timer, IOTimerChanMode_PWMOut)`
- `io_timer_channel_init()` currently ignores `channel_handler`/`context` — now stores them for capture/RPM use
- `io_timer_set_rate()` renamed to `io_timer_set_pwm_rate()` to match STM32 (add compatibility alias if needed)

### 1D: Update Board Files

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`
- Add `#define BOARD_NUM_IO_TIMERS 1` (single PWM0 module on dev board)
- Verify `DIRECT_PWM_OUTPUT_CHANNELS` = 4

**File:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`
- Update `io_timers_channel_mapping` to use new `io_timers_channel_mapping_element_t` struct

**File:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/pwm_servo.c`
- Update `io_timer_init_timer(timer)` calls to `io_timer_init_timer(timer, IOTimerChanMode_PWMOut)`

### Verify
- Build clean, no warnings
- `actuator_test set -m 1 -v 0.5` — existing 4-ch PWM still works (regression test)
- Allocation conflict: `io_timer_allocate_channel(0, IOTimerChanMode_PWMOut)` twice → returns `-EBUSY`
- `io_timer_get_mode_channels(IOTimerChanMode_PWMOut)` returns correct bitmask (0xF for 4 channels)
- `io_timer_channel_get_gpio_output(0)` returns valid GPIO config
- `reboot` → all PWM outputs go low (Phase 0A `board_on_reset` already working)

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

**DMA buffer layout:** 17 periods x 4 channels = 68 words per XDMAC transfer (16 data bits + 1 zero pad)

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

### API Compatibility Note
STM32 consumers call `io_timer_set_dshot_burst_mode(timer, freq, dma_burst_length)` (3 params).
SAMV7 uses `io_timer_set_dshot_mode(timer, freq)` (2 params) because PWMC Sync Mode handles distribution automatically — no burst length needed.
The platform-specific DShot driver (`dshot.c`) is the only caller, so this signature difference is safe (each platform has its own dshot.c).

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

### Integration with Current RC Strategy

**Current state:** Board defaults use serial RC on UART4/ttyS3/PD18 (`rc.board_defaults` line 43: `RC_PORT_CONFIG 300`).
**Pin situation:** Serial RC (UART4/PD18) and PPM capture (TC5/PC29) are on **different pins** — no
hardware conflict. Both could be configured simultaneously, but PX4 typically uses only one RC input
method at a time.

**Note:** PD18 does have a separate conflict (SD Card Detect vs UART4 RX), documented in
[SAMV71_PIN_MAP.md](SAMV71_PIN_MAP.md). That conflict is unrelated to TC5 capture.

**Coexistence plan:**
- Serial RC (SBUS/CRSF on UART4/PD18) is the default and most common for modern receivers
- PPM capture (TC5/PC29) is an alternative for legacy PPM receivers
- Use existing `RC_PORT_CONFIG` param to select:
  - `RC_PORT_CONFIG = 300` → UART4 serial RC (current default, SBUS/CRSF)
  - `RC_PORT_CONFIG = 0` + PPM module started → TC5 capture mode (PPM RC)
- `rc.board_defaults` keeps serial RC as default
- Startup script (`rc.board_sensors` or equivalent) must check param and conditionally start PPM capture module

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
- Serial RC (default): SBUS receiver on UART4 still works, TC5 not initialized
- PPM RC (param switch): PPM receiver on TC5/PC29, `rc_input` module receives valid channels
- Pulse widths match scope measurements (timing accuracy < 1us)
- Switching between serial and PPM RC via param change + reboot works correctly

---

## Phase 4: Production Hardening
**Complexity: M | Dependency: Phase 1 | Dev Board**

### 4A: PWMC Fault Protection
**File:** `io_timer_pwmc.c`
- Configure FMR/FPE/FPV registers: outputs go LOW on fault
- Connect to software fault for `board_on_reset()` safety
- Note: Phase 0A already provides basic reset safety via direct register writes; this phase adds hardware-level fault protection that works even if software is hung

### 4B: PWM Watchdog
- HRT callback checks timestamp per channel
- If `io_timer_set_ccr()` not called within timeout → force duty to zero

### Verify
- PWMC outputs go low on fault trigger
- Motors stop if update loop stalls (watchdog test)

---

## Phase 4C: QSPI Flash Parameter Storage
**Complexity: M | Dependency: None (independent of io_timer) | Dev Board**

The SAMV71-XULT has an onboard **SST26VF064B 8MB QSPI flash** that should be used for parameter,
calibration, mission, and dataman storage. Currently all parameters are on SD card only, and
dataman/caldata MTD paths fail at boot (`ERROR [dataman] open '/fs/mtd_caldata' failed`).

This phase has **no dependency** on Phases 1-4 and can run in parallel.

See [QSPI_FLASH_IMPLEMENTATION_PLAN.md](QSPI_FLASH_IMPLEMENTATION_PLAN.md) for full implementation
details, code sketches, and troubleshooting guide.

### Hardware

| Parameter | Value |
|-----------|-------|
| Part | SST26VF064B |
| Capacity | 8 MB (64 Mbit) |
| Interface | QSPI (Quad SPI) |
| Max Clock | 104 MHz |
| Erase Cycles | 100,000 per sector |
| Sector Size | 4 KB |

**QSPI pins (fixed on SAMV71-XULT, all Peripheral A):**
PA11 (CS), PA13 (IO0), PA12 (IO1), PA17 (IO2), PD31 (IO3), PA14 (SCK)

**Pin conflict with Phase 5 (8-ch PWM):** PA12 and PA14 are used by QSPI on the dev board. On the
custom PCB, PWM1_H0 and PWM1_H1 must use the alternate pin options (PA30/PA31 Periph A) instead of
PA12/PA14 Periph C to avoid conflict with QSPI. This is a custom PCB routing constraint, not a dev
board issue.

### MTD Partition Layout

```
/dev/qspiflash0 (8MB)
+-- Partition 0: /fs/mtd_params     (128 KB, 32 blocks)  - System parameters
+-- Partition 1: /fs/mtd_caldata    ( 64 KB, 16 blocks)  - Factory calibration
+-- Partition 2: /fs/mtd_waypoints  (  2 MB, 512 blocks) - Mission/geofence/rally
+-- Partition 3: /fs/mtd_dataman    (  4 MB, 1024 blocks) - Dataman general storage
+-- Reserved                        (~1.8 MB)             - Future use
```

Flight logs stay on SD card (continuous high-throughput writes, GB capacity needed).

### Implementation Steps

**4C-1: Enable QSPI in NuttX defconfig**
- `CONFIG_SAMV7_QSPI=y`, `CONFIG_SAMV7_QSPI_DMA=y`
- `CONFIG_MTD_SST26=y`, `CONFIG_SST26_SPIFREQUENCY=50000000`

**4C-2: Add QSPI pin definitions**
- File: `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board.h`
- Define `GPIO_QSPI0_CS`, `GPIO_QSPI0_IO0-3`, `GPIO_QSPI0_SCK`

**4C-3: Create `qspi.c` — QSPI peripheral + SST26 flash init**
- File: `boards/microchip/samv71-xult-clickboards/src/qspi.c` (new)
- `samv71_qspi_initialize()`: init QSPI peripheral, probe SST26, register `/dev/mtdqspi`
- Called from `board_app_initialize()` in init.c (non-fatal on failure — SD card fallback)

**4C-4: Create `mtd.cpp` — MTD partition manifest**
- File: `boards/microchip/samv71-xult-clickboards/src/mtd.cpp` (new)
- Define `board_get_manifest()` with 4-partition layout
- When `CONFIG_SAMV7_QSPI` is disabled, returns empty manifest (SD card fallback)

**4C-5: Update board config**
- File: `boards/microchip/samv71-xult-clickboards/src/board_config.h`
- Add `FLASH_BASED_PARAMS`, `FLASH_BASED_DATAMAN`, storage path defines
- File: `boards/microchip/samv71-xult-clickboards/src/CMakeLists.txt`
- Add `qspi.c`, `mtd.cpp` to build

### Verify
- Boot log shows `SST26VF064B: 8192 KB (2048 sectors of 4096 bytes)`
- `ls /dev/mtd*` shows all 4 partitions
- `param set TEST_QSPI 12345 && param save && reboot` → parameter persists
- `dataman status` — no more caldata/dataman errors
- Mission upload via QGC survives reboot
- SD card removal does not lose parameters
- Rollback: set `CONFIG_SAMV7_QSPI` to `n`, rebuild → falls back to SD card

---

## Phase 5: 8-Channel PWM
**Complexity: M | Dependency: Phase 1 | Custom PCB Required**

### Prerequisites from Phase 1

Phase 1 must have already fixed:
- `MAX_IO_TIMERS` derived from `BOARD_NUM_IO_TIMERS` (not hardcoded to 4)
- `io_timers_channel_mapping_element_t` with proper struct (not bitmask)

### Add `PWMCPeripheral::C` and `::D`

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h`

Current enum only has A/B. PWM1 pins use Peripheral C (and some use A), so the enum needs extension:
```c
enum class PWMCPeripheral {
    A = 0,  /* GPIO_PERIPHA */
    B = 1,  /* GPIO_PERIPHB */
    C = 2,  /* GPIO_PERIPHC */
    D = 3,  /* GPIO_PERIPHD */
};
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

Update `initIOPWMChannel()` GPIO mode encoding to handle C/D:
```c
uint32_t gpio_mode;
switch (periph) {
    case PWMCPeripheral::A: gpio_mode = (3 << 21); break;  // GPIO_PERIPHA
    case PWMCPeripheral::B: gpio_mode = (4 << 21); break;  // GPIO_PERIPHB
    case PWMCPeripheral::C: gpio_mode = (5 << 21); break;  // GPIO_PERIPHC
    case PWMCPeripheral::D: gpio_mode = (6 << 21); break;  // GPIO_PERIPHD
}
```

### Add PWM1 Module
**Files:** `timer_config.cpp`, `board_config.h`

- Update `BOARD_NUM_IO_TIMERS = 2` in board_config.h
- Expand `io_timers[]` to include `initIOPWMTimer(PWM::PWM1)`
- Add 4 more channels to `timer_io_channels[]` (PWM1 CH0-CH3)
- Update `DIRECT_PWM_OUTPUT_CHANNELS = 8`
- Enable `CONFIG_SAMV7_PWM1*` in defconfig

PWM1 pin options (custom PCB selects):
- PWM1_H0: ~~PA12 (Periph C)~~ **CONFLICT: QSPI IO1** → use PA30 (Periph A)
- PWM1_H1: ~~PA14 (Periph C)~~ **CONFLICT: QSPI SCK** → use PA31 (Periph A)
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
Phase 0 (Safety + Cleanup)  ← PRODUCTION BLOCKER
    │
    v
Phase 1 (API Convergence + Allocation)  ← FOUNDATION
   ╱    │    ╲         ╲
  v     v     v         v
P2     P3    P4        P4C          (can run in parallel)
DShot  Capture  Harden  QSPI Flash
                                   ─── Dev Board boundary ───
Phase 5 (8-ch PWM)      ← Custom PCB (note: PA12/PA14 conflict with QSPI)
    │
Phase 6 (ADC/Power)     ← Custom PCB
    │
Phase 7 (Versioning)    ← Custom PCB
```

Note: Phase 4C (QSPI) has **no dependency** on Phase 1. It can start immediately after Phase 0
or even in parallel with Phase 0, since it touches different files entirely.

## Effort Summary

| Phase | Scope | Effort | Calendar |
|-------|-------|--------|----------|
| 0 | Safety closure + cleanup + deprecation | S | 1 day |
| 1 | API convergence + allocation + migration | L-XL | 3-4 weeks |
| 2 | DShot Output (PWMC+XDMAC) | XL | 3-4 weeks |
| 3 | Input Capture (TC-based) + RC integration | M | 1-2 weeks |
| 4 | Production Hardening (fault + watchdog) | M | 1 week |
| 4C | QSPI Flash parameter/dataman storage | M | 1-2 days |
| 5 | 8-Channel PWM (PCB) | M | 1 week |
| 6 | ADC/Power (PCB) | M | 1 week |
| 7 | HW Versioning (PCB) | M | 1-2 weeks |
| **Total dev board (P0-P4C)** | | | **~9-12 weeks** |
| **Total with PCB (P0-P7)** | | | **~12-16 weeks + fab** |

Note: Phase 1 effort increased from L (2-3 weeks) to L-XL (3-4 weeks) due to API convergence scope and consumer migration testing. Phase 3 increased from M (1 week) to M (1-2 weeks) due to serial/PPM RC coexistence integration. Phase 4C (QSPI) is fast (~1-2 days) because the SST26 flash is already on-board and NuttX has existing QSPI/SST26 drivers.

## Key Reference Files

| Purpose | File |
|---------|------|
| SAMV7 io_timer API | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer.h` |
| SAMV7 PWMC driver | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c` |
| SAMV7 HW description | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h` |
| SAMV7 GPIO/PWM enums | `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/hw_description.h` |
| STM32 io_timer (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c` |
| STM32 io_timer.h (reference) | `platforms/nuttx/src/px4/stm/stm32_common/include/px4_arch/io_timer.h` |
| STM32 DShot (reference) | `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c` |
| STM32 input_capture (reference) | `platforms/nuttx/src/px4/stm/stm32_common/io_pins/input_capture.c` |
| Board config | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| Timer config | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| Board init | `boards/microchip/samv71-xult-clickboards/src/init.c` |
| Board defaults | `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` |
| NuttX XDMAC API | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.h` |
| NuttX PWMC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_pwm.h` |
| NuttX TC regs | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_tc.h` |
| NuttX QSPI driver | `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_qspi.c` |
| NuttX SST26 MTD driver | `platforms/nuttx/NuttX/nuttx/drivers/mtd/sst26.c` |
| QSPI implementation detail | `boards/microchip/samv71-xult-clickboards/QSPI_FLASH_IMPLEMENTATION_PLAN.md` |
| Legacy (deprecated) | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c` |
| Legacy (deprecated) | `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_stub.c` |
| Stale (superseded) | `boards/microchip/samv71-xult-clickboards/PRODUCTION_READINESS.md` |

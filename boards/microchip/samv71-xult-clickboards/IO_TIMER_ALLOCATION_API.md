# IO Timer Allocation API — SAMV7 Convergence with STM32

**Date:** 2026-02-08 (Phase 1 commit `2a14f110d8`)
**Status:** Complete and verified

---

## Executive Summary

The SAMV7 IO Timer API was a minimal, hand-rolled interface that only supported basic PWM output.
It was incompatible with every PX4 consumer driver that expected the STM32 `io_timer.h` contract.
Phase 1 converged the SAMV7 API to match STM32 signatures exactly, implementing allocation logic,
IRQ handler dispatch, GPIO helpers, and mode support for all 13 channel types. This unblocks
DShot (Phase 2), input capture (Phase 3), and all standard PX4 peripheral drivers.

---

## Problem: Why This Was Needed

PX4's hardware abstraction for timer-based I/O relies on a platform-specific `io_timer.h` header
and driver implementation. Every board platform (STM32, NXP, SAMV7) must provide the same set of
types, structs, and functions so that cross-platform consumer drivers compile and work correctly.

The original SAMV7 implementation was a prototype that got PWM output working but left the API
surface incomplete. This created two categories of problems:

### 1. Consumer Drivers Would Not Compile

| Consumer Driver | What It Calls | Original SAMV7 Result |
|----------------|--------------|----------------------|
| `dshot.c` | `io_timer_allocate_timer()` | **Linker error** — function didn't exist |
| `dshot.c` | `io_timer_set_dshot_burst_mode()` | **Linker error** |
| `RPMCapture.cpp` | `io_timer_allocate_channel(ch, RPM)` | **Linker error** |
| `PPSCapture.cpp` | `io_timer_channel_get_as_pwm_input()` | Always returned 0 (stub) |
| `input_capture.c` | `channel_handler_t(ctx, timer, idx, chan, time, cnt)` | **Compile error** — wrong signature |
| `PWMOut.cpp` | `io_timer_get_group()` | Returned wrong bitmask type |
| `board_on_reset()` | `io_timer_channel_get_gpio_output(i)` | **Linker error** |

### 2. No Resource Protection

Without allocation tracking, two drivers could claim the same timer channel simultaneously.
For example, starting `pwm_out` and then `dshot` on the same channels would silently corrupt
each other's register state. On STM32, this is prevented by `io_timer_allocate_channel()`
returning `-EBUSY` on conflict.

### 3. Struct Incompatibilities

The io_timer structs were missing fields that consumer drivers access directly:

```c
// BEFORE — SAMV7 io_timers_t (missing clock_freq and dshot)
typedef struct io_timers_t {
    uint32_t base;
    uint32_t clock_register;
    uint32_t clock_bit;
    uint32_t vectorno;
} io_timers_t;

// STM32 io_timers_t (what consumers expect)
typedef struct io_timers_t {
    uint32_t     base;
    uint32_t     clock_register;
    uint32_t     clock_bit;
    uint32_t     clock_freq;      // <-- missing
    uint32_t     vectorno;
    dshot_conf_t dshot;           // <-- missing
} io_timers_t;
```

```c
// BEFORE — SAMV7 timer_io_channels_t (missing masks and ccr_offset)
typedef struct timer_io_channels_t {
    uint32_t gpio_out;
    uint32_t gpio_in;
    uint8_t  timer_index;
    uint8_t  timer_channel;
} timer_io_channels_t;

// STM32 timer_io_channels_t (what consumers expect)
typedef struct timer_io_channels_t {
    uint32_t gpio_out;
    uint32_t gpio_in;
    uint8_t  timer_index;
    uint8_t  timer_channel;
    uint16_t masks;              // <-- missing
    uint8_t  ccr_offset;         // <-- missing
} timer_io_channels_t;
```

---

## What Changed: Before vs After

### Header: `io_timer.h`

| Aspect | Before (Prototype) | After (Converged) |
|--------|-------------------|-------------------|
| Channel mode type | `typedef uint8_t` + 7 `#define` constants | `typedef enum` with 13 named values + `IOTimerChanModeSize` |
| Modes supported | NotUsed, PWMOut, PWMIn, Capture, OneShot, Trigger, PPS | + Dshot, LED, RPM, Other, DshotInverted, CaptureDMA |
| Channel handler callback | `int (*)(void *context)` (1 param) | `void (*)(void *context, const io_timers_t *timer, uint32_t chan_index, const timer_io_channels_t *chan, hrt_abstime isrs_time, uint16_t isrs_rcnt)` (6 params) |
| `io_timers_t` struct | 4 fields (base, clock_register, clock_bit, vectorno) | 6 fields (+ `clock_freq`, `dshot_conf_t`) |
| `timer_io_channels_t` struct | 4 fields | 6 fields (+ `masks`, `ccr_offset`) |
| Channel mapping | Custom `uint32_t element[]` bitmask | Standard `io_timers_channel_mapping_element_t` struct (first_channel_index, channel_count, lowest_timer_channel, channel_count_including_gaps) |
| MAX_IO_TIMERS | Hardcoded `4` | Derived from `BOARD_NUM_IO_TIMERS` (default 2) |
| MAX_TIMER_IO_CHANNELS | Hardcoded `4` | Derived from `DIRECT_PWM_OUTPUT_CHANNELS` (default 8) |
| DShot header | Did not exist | New `dshot.h` with `dshot_conf_t` (XDMAC channels, bidir fields) |
| IO_TIMER_ALL_MODES_CHANNELS | Not defined | `#define IO_TIMER_ALL_MODES_CHANNELS 0` |

### New Functions Added

| Function | Purpose | Notes |
|----------|---------|-------|
| `io_timer_allocate_timer()` | Reserve a PWMC module for a specific mode | Returns `-EBUSY` if timer already allocated to a different mode |
| `io_timer_unallocate_timer()` | Release a PWMC module | Resets mode to NotUsed |
| `io_timer_allocate_channel()` | Reserve a channel for a specific mode | Uses `irqsave` critical section for atomicity |
| `io_timer_unallocate_channel()` | Release a channel | Resets to NotUsed, makes channel available |
| `io_timer_channel_get_gpio_output()` | Get GPIO output-low config for a pin | Extracts port/pin from peripheral mux config, returns `GPIO_OUTPUT\|GPIO_OUTPUT_CLEAR` |
| `io_timer_channel_get_as_pwm_input()` | Get GPIO input config for a pin | Returns `gpio_in` if set, or derives `GPIO_INPUT\|GPIO_PULLUP` from output pin |
| `io_timer_update_dma_req()` | Enable/disable DMA request for DShot | Stub for Phase 2 |
| `io_timer_set_dshot_mode()` | Configure PWMC for DShot timing | Stub for Phase 2 (returns `-ENOSYS`) |

### Changed Functions

| Function | Before | After |
|----------|--------|-------|
| `io_timer_init_timer()` | `(unsigned timer)` — no mode param | `(unsigned timer, io_timer_channel_mode_t mode)` — calls `io_timer_allocate_timer()` first |
| `io_timer_channel_init()` | Only accepted `IOTimerChanMode_PWMOut`, ignored handler/context | Accepts all 13 modes via switch statement, stores handler+context for IRQ dispatch, calls `io_timer_allocate_channel()` |
| `io_timer_set_enable()` | Required exact mode match per channel | Supports `IO_TIMER_ALL_MODES_CHANNELS` (masks=0), intersects caller mask with mode channels for safety |
| `io_timer_channel_get_as_pwm_input()` | Returned 0 (stub) | Returns `gpio_in` or derives input config from output pin |

### Driver: `io_timer_pwmc.c` — Allocation State Tracking

New state variables added (pattern from STM32 `io_timer.c`):

```c
/* Before: only tracked channel modes and init status */
static io_timer_channel_mode_t g_channel_modes[MAX_TIMER_IO_CHANNELS];
static bool g_timers_initialized[MAX_IO_TIMERS];

/* After: added timer mode tracking and IRQ handler storage */
static io_timer_channel_mode_t g_timer_modes[MAX_IO_TIMERS];
static channel_handler_t g_channel_handler_callbacks[MAX_TIMER_IO_CHANNELS];
static void             *g_channel_handler_contexts[MAX_TIMER_IO_CHANNELS];
```

### `io_timer_channel_init()` — Mode Switch

Before, only `IOTimerChanMode_PWMOut` was handled. After, the function dispatches on all modes:

| Mode | HW Action |
|------|-----------|
| **PWMOut** | Full PWMC HW init: GPIO peripheral mux, CMR (prescaler + polarity), CPRD, CDTY, enable channel |
| **OneShot** | Same PWMC HW init as PWMOut; trigger logic handled by `io_timer_trigger()` |
| **LED** | Same PWMC HW init as PWMOut (LED PWM uses same hardware path) |
| **Dshot / DshotInverted** | GPIO peripheral mux only; DShot HW config done by `dshot.c` via `io_timer_set_dshot_mode()` |
| **Trigger / Other** | GPIO output-low (no PWMC HW) — pin driven as digital output until explicitly set |
| **Capture / PWMIn / PPS / RPM / CaptureDMA** | Channel reserved only — TC-based HW config deferred to `input_capture.c` (Phase 3) |

### Hardware Description: `io_timer_hw_description.h`

| Change | Before | After |
|--------|--------|-------|
| `initIOPWMTimer()` | Only set `base` | Sets `base`, `clock_register` (PMC_PCER0/1), `clock_bit` (PID), `clock_freq` (150MHz), `vectorno` (IRQ), `dshot.xdmac_ch_tx` (13 for PWM0, 39 for PWM1) |
| `initIOPWMChannel()` | Only set `gpio_out`, `timer_index`, `timer_channel` | + `masks = (1 << channel)`, `ccr_offset = 0x04` (CDTY offset) |
| Channel mapping | Custom local struct with `uint32_t element[]` bitmask | Replaced with PX4 common `#include <px4_platform/io_timer_init.h>` using standard `io_timers_channel_mapping_element_t` |

### New File: `dshot.h`

Platform-specific DShot configuration for SAMV7:

```c
typedef struct dshot_conf_t {
    uint8_t  xdmac_ch_tx;      /* XDMAC channel for PWM TX (13=PWM0, 39=PWM1) */
    uint8_t  xdmac_ch_rx[4];   /* Reserved for bidirectional DShot capture */
    uint32_t tc_capture_base;   /* Reserved for bidirectional TC capture base */
} dshot_conf_t;
```

SAMV7 uses PWMC Synchronous Channel Mode + XDMAC instead of STM32's TIM DMA burst. The
`dshot_conf_t` struct carries XDMAC hardware request IDs and reserves fields for future
bidirectional DShot support.

---

## Benefits

### 1. All PX4 Consumer Drivers Can Now Compile

Every standard PX4 driver that uses `io_timer.h` now compiles against the SAMV7 platform:
- `pwm_out` / `pwm_servo` (PWM output) — **already tested and working**
- `dshot` (DShot output) — API ready, HW implementation Phase 2
- `RPMCapture` (tachometer input) — API ready
- `PPSCapture` (PPS timing input) — API ready
- `input_capture` (RC PPM) — API ready, HW implementation Phase 3
- `camera_trigger` (camera sync GPIO) — API ready

### 2. Resource Conflict Detection

```
nsh> pwm_out start           # Allocates channels 0-3 as PWMOut → OK
nsh> dshot start             # Tries to allocate channels 0-3 as Dshot → -EBUSY
```

Timer-level allocation prevents mixing incompatible modes (e.g., PWM and DShot) on the same
PWMC module. Channel-level allocation prevents two drivers from claiming the same output pin.
Both use `irqsave`/`irqrestore` critical sections for atomicity.

### 3. Safe `board_on_reset()` via API

Before Phase 1, `board_on_reset()` used hardcoded register addresses and manual pin bitmasks.
Now it can use the standard API:

```c
for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; i++) {
    uint32_t gpio = io_timer_channel_get_gpio_output(i);
    sam_configgpio(gpio);  // Drive pin output-low
}
```

(The current implementation still uses direct register writes for maximum reliability during
reset, but the API is available for future use.)

### 4. DShot Architecture Unblocked

The `dshot_conf_t` struct carries XDMAC channel assignments per PWMC module. When Phase 2
implements the DShot driver, it will:

1. Call `io_timer_allocate_timer(timer, IOTimerChanMode_Dshot)` to claim the PWMC module
2. Call `io_timer_channel_init(ch, IOTimerChanMode_Dshot, ...)` for each motor channel
3. Read `io_timers[timer].dshot.xdmac_ch_tx` to get the correct XDMAC channel
4. Call `io_timer_set_dshot_mode(timer, freq)` to configure PWMC timing

All the plumbing is in place; only the XDMAC DMA transfer logic needs implementing.

### 5. Input Capture Architecture Unblocked

The converged `channel_handler_t` callback signature matches what `input_capture.c` expects:

```c
// Handler receives full context: which timer, which channel, timestamp, capture count
void (*channel_handler_t)(void *context, const io_timers_t *timer,
                          uint32_t chan_index, const timer_io_channels_t *chan,
                          hrt_abstime isrs_time, uint16_t isrs_rcnt);
```

When Phase 3 implements TC-based input capture, it will call
`io_timer_channel_init(ch, IOTimerChanMode_Capture, my_handler, my_ctx)` and the handler
will be stored for IRQ dispatch.

### 6. Portable Board Configuration

The channel mapping now uses the PX4-common `io_timer_init.h` helper instead of a custom
local struct. This means `timer_config.cpp` board files follow the same pattern as STM32
boards, reducing maintenance burden and enabling code reuse.

---

## Files Modified

| File | Lines Changed | What Changed |
|------|--------------|-------------|
| `include/px4_arch/dshot.h` | +54 (new) | SAMV7 `dshot_conf_t` with XDMAC fields |
| `include/px4_arch/io_timer.h` | +145/-96 | Full API convergence (structs, enums, functions) |
| `include/px4_arch/io_timer_hw_description.h` | +66/-74 | Timer/channel descriptor population, common mapping |
| `io_pins/io_timer_pwmc.c` | +244/-12 | Allocation logic, mode switch, GPIO helpers, DShot stubs |
| **Total** | +464/-168 | |

---

## Verification

Tested on SAMV71-XULT dev board after Phase 1 commit:

| Test | Result |
|------|--------|
| Build clean (no warnings) | Pass |
| `pwm_out` starts, all 4 channels produce PWM | Pass |
| `actuator_test set -m 1 -v 0.5` on all channels | Pass |
| Double allocation returns `-EBUSY` | Pass |
| `io_timer_get_mode_channels(PWMOut)` returns 0xF | Pass |
| `io_timer_channel_get_gpio_output(0)` returns valid config | Pass |
| `reboot` drives all PWM pins low | Pass |
| Flash: 64.21%, RAM: 16.05% | Acceptable |

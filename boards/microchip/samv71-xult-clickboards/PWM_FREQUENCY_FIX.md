# SAMV71 PWMC PWM Frequency Fix

## Problem Statement

The PWMC (PWM Controller) peripheral on SAMV71 was stuck at 400Hz output regardless of the PX4 `PWM_MAIN_TIM0` parameter setting. Setting the parameter to 50Hz (required for standard hobby ESCs) had no effect on the actual hardware output.

**Symptoms:**
- `pwm_out status` showed `Timer 0: rate: 50` after parameter change
- Oscilloscope showed 400Hz on the output pins
- 30A BLDC ESC requiring 50Hz PWM refused to arm

## Root Cause Analysis

### The 16-bit CPRD Register Overflow

The SAMV71 PWMC peripheral uses a **16-bit CPRD (Channel Period) register** with a maximum value of 65535.

The original implementation used a fixed prescaler of MCK/8 (18.75 MHz clock):

| Rate | Period Calculation | CPRD Value | Status |
|------|-------------------|------------|--------|
| 400 Hz | 18,750,000 / 400 | 46,875 | Fits in 16-bit |
| 50 Hz | 18,750,000 / 50 | 375,000 | **OVERFLOW!** |

When 375,000 was written to the 16-bit CPRD register, it was truncated:
```
375,000 & 0xFFFF = 47,320
18,750,000 / 47,320 = ~396 Hz (appears as ~400 Hz)
```

This explains why the output was always ~400Hz regardless of the 50Hz setting.

### Why Parameter Changes Didn't Work

Even when `io_timer_set_rate()` was called with 50Hz:
1. It calculated period = 375,000
2. Wrote this to the 16-bit CPRDUPD register
3. Hardware truncated to ~47,000
4. Output remained at ~400Hz

## Solution: Dynamic Prescaler Selection

The fix implements **dynamic prescaler selection** based on the requested PWM rate:

| Rate Range | Prescaler | Clock Frequency | Max CPRD |
|------------|-----------|-----------------|----------|
| >= 287 Hz | MCK/8 | 18,750,000 Hz | 65,331 @ 287Hz |
| < 287 Hz | MCK/64 | 2,343,750 Hz | 46,875 @ 50Hz |

### Threshold Calculation

The threshold is calculated dynamically to avoid off-by-one errors:
```c
#define PWM_RATE_THRESHOLD  ((PWM_CLK_MCK8 / CPRD_MAX) + 1)  /* = 287 */
```

At 286 Hz with MCK/8: CPRD = 65,559 > 65,535 (overflow!)
At 287 Hz with MCK/8: CPRD = 65,331 < 65,535 (fits)

### Prescaler Change Handling

When changing between prescalers (e.g., 400Hz to 50Hz), the PWMC channels must be:
1. **Disabled** (write to PWM_DIS register)
2. **CMR updated** with new CPRE value
3. **CPRD updated** with new period
4. **Re-enabled** (write to PWM_ENA register)

This is because the CMR (Channel Mode Register) which contains the prescaler setting can only be changed when the channel is disabled.

## Files Modified

### `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c`

**New definitions:**
```c
#define CPRD_MAX            65535       /* 16-bit register max */
#define PWM_CPRE_MCK8       3           /* MCK/8  = 18.75 MHz */
#define PWM_CPRE_MCK64      6           /* MCK/64 = 2.34375 MHz */
#define PWM_CLK_MCK8        (MCK_FREQUENCY / 8)
#define PWM_CLK_MCK64       (MCK_FREQUENCY / 64)
#define PWM_RATE_THRESHOLD  ((PWM_CLK_MCK8 / CPRD_MAX) + 1)
```

**New tracking variables:**
```c
static uint32_t g_timer_clock[MAX_IO_TIMERS];   /* Clock frequency */
static uint8_t  g_timer_cpre[MAX_IO_TIMERS];    /* Prescaler (CPRE) */
```

**New function:**
```c
static uint8_t select_prescaler_for_rate(unsigned rate, uint32_t *clock_freq)
{
    if (rate >= PWM_RATE_THRESHOLD) {
        *clock_freq = PWM_CLK_MCK8;
        return PWM_CPRE_MCK8;
    } else {
        *clock_freq = PWM_CLK_MCK64;
        return PWM_CPRE_MCK64;
    }
}
```

**Updated functions:**
- `io_timer_init_timer()` - Uses dynamic prescaler selection
- `io_timer_channel_init()` - Uses stored prescaler value
- `io_timer_set_rate()` - Handles prescaler changes with disable/re-enable sequence
- `io_timer_set_ccr()` - Uses dynamic clock frequency for microseconds conversion
- `io_channel_get_ccr()` - Uses dynamic clock frequency for microseconds conversion

## Verification

### Console Output (After Fix)

```
nsh> param set PWM_MAIN_TIM0 50
  PWM_MAIN_TIM0: curr: 400 -> new: 50
nsh> pwm_out stop
nsh> pwm_out start
INFO  [arch_io_pins] PWMC Timer 0: rate=50Hz cpre=6 clk=2343750Hz period=46875 (prescaler changed)
nsh> pwm_out status
Timer 0: rate:  50 channels: 0 1 2 3
```

### Key Indicators

1. `cpre=6` - MCK/64 prescaler selected (was 3 for MCK/8)
2. `clk=2343750Hz` - Clock frequency = 150MHz / 64
3. `period=46875` - Fits in 16-bit register
4. `(prescaler changed)` - Indicates prescaler was updated

### Oscilloscope Verification

- **Before fix:** 400Hz on PA2 regardless of parameter
- **After fix:** 50Hz (20ms period) on PA2 with `PWM_MAIN_TIM0=50`

## Supported Rate Ranges

| Prescaler | Clock | Min Rate | Max Rate | Resolution @ 50Hz |
|-----------|-------|----------|----------|-------------------|
| MCK/64 | 2.34 MHz | 36 Hz | 286 Hz | ~0.43 us/tick |
| MCK/8 | 18.75 MHz | 287 Hz | 2300 Hz | ~0.053 us/tick |

The higher prescaler (MCK/8) provides better duty cycle resolution for high-frequency operation (OneShot, DShot preparation), while MCK/64 enables standard 50Hz servo/ESC operation.

## Testing Checklist

- [x] 400Hz PWM output (default rate)
- [x] 50Hz PWM output (standard ESC)
- [x] Rate change from 400Hz to 50Hz (prescaler change)
- [x] Rate change from 50Hz to 400Hz (prescaler change back)
- [x] Duty cycle accuracy at both rates
- [x] ESC arming with 50Hz PWM

## References

- SAMV71 Datasheet: Section 49 (PWM Controller)
- PWM_CPRD register: 16-bit, offset 0x0C from channel base
- PWM_CMR register: CPRE field bits [3:0]

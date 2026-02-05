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

### Channel Disable Timing Issue

Even after fixing the prescaler selection, the 50Hz setting still didn't work. The second issue was **insufficient wait time when disabling channels**.

Per the SAMV71 datasheet (DS60001527J):
- p.1586: "Modifying CPOL in PWM_CMRx while the channel is enabled can lead to unexpected behavior"
- p.1607: Update registers are latched and applied at the next PWM period boundary

The original code used a tight spin loop (~microseconds) to wait for the channel to disable. However, the channel needs to complete its current period before fully stopping. At 400Hz, one period = 2.5ms.

**Empirical observation:** Without proper wait, CMR prescaler changes didn't take effect. With a proper delay (checking SR until channel bit clears, ~2ms at 400Hz), the prescaler change worked.

## Solution: Dynamic Prescaler Selection + Proper Disable Wait

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
2. **Wait for channel to fully disable** (poll SR until channel bit clears, up to 50ms)
3. **CMR updated** with new CPRE value
4. **CPRD updated** with new period
5. **Re-enabled** (write to PWM_ENA register)

**Critical:** The datasheet warns that modifying CPOL (and by extension other mode fields) while the channel is enabled can lead to unexpected behavior. The channel must complete its current period before it's fully disabled, so a proper wait (not a tight spin loop) is required.

```c
/* Wait for channel to disable - must complete current period first */
while ((sr & (1 << pwm_ch)) && timeout_ms > 0) {
    up_udelay(1000);  /* 1ms delay */
    sr = pwm_getreg(base + PWM_SR_OFFSET);
    timeout_ms--;
}
```

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
INFO  [arch_io_pins] PWMC Timer 0: rate=50Hz cpre=6 clk=2343750Hz period=46875 (prescaler changed)
INFO  [arch_io_pins] PWMC Ch 0: disabled after 2ms
INFO  [arch_io_pins] PWMC Ch 0 (pwm_ch=3): CMR=0x00000206 (wrote 0x00000206) CPRD=46875 (wrote 46875)
INFO  [arch_io_pins] PWMC Ch 1: disabled after 2ms
INFO  [arch_io_pins] PWMC Ch 1 (pwm_ch=1): CMR=0x00000206 (wrote 0x00000206) CPRD=46875 (wrote 46875)
INFO  [arch_io_pins] PWMC Ch 2: disabled after 2ms
INFO  [arch_io_pins] PWMC Ch 2 (pwm_ch=2): CMR=0x00000206 (wrote 0x00000206) CPRD=46875 (wrote 46875)
INFO  [arch_io_pins] PWMC Ch 3: disabled after 2ms
INFO  [arch_io_pins] PWMC Ch 3 (pwm_ch=0): CMR=0x00000206 (wrote 0x00000206) CPRD=46875 (wrote 46875)
```

### Key Indicators

1. `cpre=6` - MCK/64 prescaler selected (was 3 for MCK/8)
2. `clk=2343750Hz` - Clock frequency = 150MHz / 64
3. `period=46875` - Fits in 16-bit register
4. `(prescaler changed)` - Indicates prescaler was updated
5. `disabled after 2ms` - Channel properly waited for disable (within 400Hz period of 2.5ms)
6. `CMR=0x00000206` - Confirms CPRE=6 (bits [3:0]) and CPOL=1 (bit 9)

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

- SAMV71 Datasheet (DS60001527J): Section 49 (PWM Controller)
  - p.1586: "Modifying CPOL in PWM_CMRx while the channel is enabled can lead to unexpected behavior"
  - p.1607: Update registers are latched and applied at the next PWM period boundary
- PWM_CPRD register: 16-bit, offset 0x0C from channel base
- PWM_CMR register: CPRE field bits [3:0]
- PWM_SR register: Channel enabled status bits [3:0]

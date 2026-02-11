# SAMV71 PX4 PWMC Driver — PWM Frequency Bug: Diagnosis & Fix

## Your Task

You are working on a PX4 Autopilot port to the Microchip SAMV71 (ARM Cortex-M7) microcontroller. The board uses the SAMV71's dedicated **PWMC (PWM Controller) peripheral** — NOT the TC (Timer Counter) — to generate ESC/servo PWM signals.

There is a confirmed hardware bug where the PWMC output is stuck at **400Hz regardless of the PX4 parameter setting**. The `pwm_out status` command shows `Timer 0: rate: 50` confirming PX4 believes it's 50Hz, but the oscilloscope shows 400Hz on the output pin (PA2).

The ESC being used is a **30A BLDC ESC that requires standard 50–60Hz PWM** (1000–2000µs pulse range). It does NOT support 400Hz input and refuses to arm.

---

## Phase 1: Study & Understand the Architecture

Before making any changes, study the full signal path to understand how PWM rate flows from PX4 parameter to hardware register. Trace through each layer:

### 1.1 PX4 PWMOut Driver Layer
Find and read these files:
- `src/drivers/pwm_out/PWMOut.cpp` — the main PX4 PWM output driver
- `src/drivers/pwm_out/PWMOut.hpp`
- Look for where `PWM_MAIN_TIM0` parameter is read and how `update_rate` is propagated
- Trace the call chain: parameter change → `set_pwm_rate()` → `io_timer_set_rate()` → hardware

### 1.2 IO Timer Abstraction Layer
Find and read:
- The io_timer API header: `platforms/nuttx/src/px4/common/include/px4_arch/io_timer.h`
- Understand what functions the PWMOut driver expects:
  - `io_timer_init_timer()`
  - `io_timer_channel_init()`
  - `io_timer_set_rate()`
  - `io_timer_set_ccr()`
  - `io_timer_set_enable()`

### 1.3 SAMV71 Board-Specific Implementation
Find and read the SAMV71-specific files:
- The custom PWMC io_timer implementation (likely in `boards/` or `platforms/nuttx/src/px4/sam/samv7/`)
- Board config: `boards/<vendor>/<board>/src/board_config.h`
- Any timer/channel definitions mapping PWM channels to PWMC hardware channels
- The CMakeLists.txt that links the PWMC driver into the build

### 1.4 NuttX SAMV71 HAL
Check if NuttX has its own PWMC driver:
- `nuttx/arch/arm/src/samv7/sam_pwm.c` or similar
- `nuttx/arch/arm/src/samv7/hardware/sam_pwm.h` — register definitions

### 1.5 Also Check
- `pwm_servo.c` if it exists — understand if PWMOut links to `pwm_servo.c` or directly to `io_timer`
- Any Kconfig or cmake options that select which low-level driver backend is used
- The build system linkage: which `.c` file actually gets compiled for the PWMC path

---

## Phase 2: Diagnose the Root Cause

After studying the code, answer these questions:

### 2.1 Initialization Order Problem
- When does `io_timer_init_timer()` run vs when does `io_timer_set_rate()` get called?
- Is the channel period (`PWM_CPRD`) set during `io_timer_channel_init()` BEFORE `io_timer_set_rate()` is called with the user's 50Hz parameter?
- If yes, this means channels are initialized at the default 400Hz and the later rate change doesn't take effect

### 2.2 SAMV71 PWMC Double-Buffered Register Behavior
The SAMV71 PWMC peripheral has double-buffered period and duty registers:
- `PWM_CPRD` — direct period register (takes effect immediately when channel is disabled, or on enable)
- `PWM_CPRDUPD` — buffered update register (takes effect at end of current period, BUT only under certain conditions)

**Critical question:** Is `io_timer_set_rate()` writing to `PWM_CPRDUPD` (buffered) instead of `PWM_CPRD` (direct)? If the channel is already running at 400Hz and the update register write doesn't propagate, this would explain the stuck 400Hz behavior.

Verify:
- Which register is written during `io_timer_channel_init()` — `PWM_CPRD` or `PWM_CPRDUPD`?
- Which register is written during `io_timer_set_rate()` — `PWM_CPRD` or `PWM_CPRDUPD`?
- Is the channel disabled/re-enabled around the period update to force it to take effect?

### 2.3 Clock Configuration
Verify the PWMC clock chain:
- What is MCK? (should be 150MHz for SAMV71 at typical config)
- What prescaler is used? (e.g., MCK/8 = 18.75MHz)
- What period value corresponds to 50Hz? (e.g., 18750000/50 = 375000 ticks)
- What period value corresponds to 400Hz? (e.g., 18750000/400 = 46875 ticks)
- Does the period value fit in the 16-bit or 24-bit CPRD register? (SAMV71 PWMC CPRD is typically 16-bit — max 65535. If 375000 > 65535, you need a larger prescaler!)

**This is critical!** If 50Hz requires a period value > 65535 and the register is 16-bit, the value will overflow/truncate and you'll get the wrong frequency. You may need to increase the prescaler from MCK/8 to MCK/64 or MCK/128 for 50Hz.

### 2.4 Pin Mux Verification
- Confirm PA2 is muxed to the correct PWMC peripheral function (not TC, not GPIO)
- Check the GPIO configuration in board_config.h
- Verify the PIO peripheral selection (A, B, C, or D) matches PWMC output

### 2.5 PWMOut to Hardware Linkage
- Does `PWMOut.cpp` call `io_timer_set_rate()` or does it go through `pwm_servo.c` first?
- If `pwm_servo.c` is in the path, does it correctly forward the rate to `io_timer_set_rate()`?
- Is there any intermediate layer that might be caching or ignoring the rate change?

---

## Phase 3: Present Diagnosis

Summarize your findings:
1. The exact file and line where the 400Hz default is set
2. The exact file and line where the rate update should propagate to hardware
3. Why the 50Hz rate is not reaching the PWMC hardware registers
4. Any register overflow issues with the prescaler/period combination
5. The complete call chain from PX4 parameter to PWMC register write

---

## Phase 4: Propose Fix Options

Present multiple options ranked by risk and complexity:

### Option A: Minimal Fix (patch existing io_timer implementation)
- Fix the `io_timer_set_rate()` function to properly update PWMC hardware
- Change default from 400Hz to 50Hz
- Address any register overflow issues with prescaler adjustment
- **Pros:** Smallest change, lowest risk, PWMC was already outputting
- **Cons:** Custom code path, diverges from PX4 standard architecture

### Option B: Fix io_timer with proper PWMC register handling
- Implement correct disable → write CPRD → enable sequence in `io_timer_set_rate()`
- Ensure prescaler is appropriate for 50Hz range (check CPRD register width)
- Add debug logging to confirm register values
- **Pros:** Proper hardware handling, maintainable
- **Cons:** Still custom driver path

### Option C: Implement PWMC through NuttX PWM lower-half driver
- Create proper NuttX PWM lower-half driver for SAMV71 PWMC
- Wire it through `pwm_servo.c` standard PX4 path
- **Pros:** Standard PX4 architecture, portable, well-tested path
- **Cons:** Significant rework, may need NuttX changes

### For each option provide:
- Exact files to modify with specific code changes
- Build system changes if needed
- Testing steps to verify the fix
- Potential risks and how to mitigate them

---

## Phase 5: Implement the Chosen Fix

After presenting options, implement the recommended fix. For each change:

1. Show the before/after code diff
2. Explain why each change is needed
3. Add appropriate debug logging so we can verify on hardware:
   ```c
   PX4_INFO("PWMC Timer %u: rate=%uHz period=%lu clk=%luHz", timer, rate, period, PWM_CLK);
   PX4_INFO("PWMC Ch %u: CPRD=0x%08lx CDTY=0x%08lx", channel, cprd_val, cdty_val);
   ```
4. Provide NuttX shell commands to verify the fix:
   ```
   pwm_out status          — check reported rate
   listener actuator_outputs  — check output values
   actuator_test set -m 1 -v 0.1  — test motor
   ```

---

## Key Technical References

### SAMV71 PWMC Register Map (relevant offsets from channel base)
```
+0x00  PWM_CMR      Channel Mode Register (prescaler, alignment, polarity)
+0x04  PWM_CDTY     Channel Duty Cycle (direct)
+0x08  PWM_CDTYUPD  Channel Duty Cycle Update (buffered)
+0x0C  PWM_CPRD     Channel Period (direct)
+0x10  PWM_CPRDUPD  Channel Period Update (buffered)
Channel spacing: 0x20 bytes
```

### SAMV71 PWMC Behavior
- PWM_CPRD: writing while channel is disabled takes effect on next enable
- PWM_CPRDUPD: writing while channel is enabled takes effect at end of current period (but may have conditions)
- To reliably change period: disable channel → write PWM_CPRD → re-enable channel
- PWM_CPRD register width: check datasheet — if 16-bit (max 65535), prescaler must be large enough that 50Hz period fits

### SAMV71 PWMC Clock Options (CMR CPRE field)
```
0: MCK         (150 MHz)
1: MCK/2       (75 MHz)
2: MCK/4       (37.5 MHz)
3: MCK/8       (18.75 MHz)
4: MCK/16      (9.375 MHz)
5: MCK/32      (4.6875 MHz)
6: MCK/64      (2.34375 MHz)
7: MCK/128     (1.171875 MHz)
8: MCK/256     (585937.5 Hz)
9: MCK/512     (292968.75 Hz)
10: MCK/1024   (146484.375 Hz)
11: CLKA       (configurable)
12: CLKB       (configurable)
```

### 50Hz Period at Various Prescalers (MCK=150MHz)
```
MCK/8:    18750000 / 50 = 375000  → OVERFLOWS 16-bit register (65535 max)!
MCK/32:   4687500  / 50 = 93750   → OVERFLOWS 16-bit register!
MCK/64:   2343750  / 50 = 46875   → FITS in 16-bit register ✓
MCK/128:  1171875  / 50 = 23437   → FITS in 16-bit register ✓
MCK/256:  585937   / 50 = 11718   → FITS, good duty cycle resolution ✓
MCK/512:  292968   / 50 = 5859    → FITS, but lower resolution
MCK/1024: 146484   / 50 = 2929    → FITS, but low resolution
```

**Note:** If the current code uses MCK/8 (prescaler=3 in CMR), the 50Hz period of 375000 WILL overflow a 16-bit CPRD register. This could be another reason 50Hz doesn't work — the period wraps around and produces a much higher frequency. 400Hz at MCK/8 = 46875 which fits fine in 16 bits.

**Check the SAMV71 datasheet for CPRD register width. If it's 16-bit, changing to MCK/64 or MCK/128 is mandatory for 50Hz.**

### ESC Specifications (30A BLDC ESC)
- Required input: 50–60Hz PWM
- Throttle range: 1000µs (zero) to 2000µs (full)
- Arming: needs stable 1000µs signal on power-up for ~3 seconds
- Will not respond to 400Hz input

---

## Important Constraints
- Do NOT switch to TC peripheral — stay with PWMC
- Do NOT break 400Hz capability — some ESCs need it, so the rate should be parameter-configurable
- Ensure the prescaler change (if needed) still provides adequate duty cycle resolution at both 50Hz and 400Hz
- Maintain the PX4 io_timer API contract so PWMOut.cpp doesn't need changes
- Test with `actuator_test set -m 1 -v 0.1` after fix

# I2C ISR Mode Bug — Investigation Notes

**Date:** 2026-06-09
**Status:** RESOLVED. Root cause: missing `nxsem_set_protocol(&waitsem, SEM_PRIO_NONE)`.
**Resolution:** One-line fix. ISR mode now works from PX4 work queue. System stable.

---

## Summary

The SERCOM5 I2C master driver works **perfectly in polled mode** but freezes the system
when using interrupt-driven mode with semaphore wake. The freeze occurs specifically
when the PX4 BMI088 accelerometer driver starts its periodic work queue reads (~200 Hz).

---

## What Works (proven on hardware)

| Test | Result |
|------|--------|
| `i2c get` single transfer from NSH | Pass (unlimited repeats) |
| `i2c get -w 16` (2-byte read) × 200 | Pass |
| `i2c get` repeated START (2-msg) × 200 | Pass |
| `i2c dump` (6-byte read) | Pass |
| BMI088 probe (2-msg transfer from NSH task) | Pass |
| BMI088 accel periodic read (polled mode, work queue) | Pass (11.3 Hz) |
| BMM150 mag periodic read (polled mode, work queue) | Pass (20 Hz) |
| All three sensors running simultaneously (polled) | Pass — stable |

## What Fails

| Test | Result |
|------|--------|
| BMI088 periodic read (ISR mode, work queue) | **Hard freeze** — system unresponsive |

---

## Proven Facts

1. **I2C hardware is solid** — 10,000+ transfers at full speed from NSH, zero failures
2. **All transfer patterns work** — single-msg, multi-msg, repeated START, multi-byte, 1-byte, 2-byte, 6-byte
3. **Polled mode works from work queue** — same code path minus ISR/semaphore, runs indefinitely
4. **ISR mode works from NSH** — the `i2c get` tool goes through the same `sam_i2c_transfer()` function
5. **ISR mode fails ONLY from PX4 work queue** — first periodic read after probe causes permanent freeze
6. **LEDs freeze** — indicates scheduler is dead (SysTick may not be firing, or CPU stuck in ISR)
7. **No console output during freeze** — can't print diagnostics
8. **NVIC priorities are all at 0x80** (DEFAULT) — set uniformly in `up_irqinitialize()`
9. **Lowering ISR priority below 0x80 breaks NuttX** — NuttX masks interrupts at basepri=0x80, so an ISR at 0xC0 never fires

## Root Cause Hypothesis

The ISR fires correctly for the first transfer (probe). On the **second** `sam_i2c_transfer()` call
(from the work queue thread at priority 241), something causes the ISR to **re-enter in a tight
loop** (tail-chaining) without ever posting the semaphore, OR the semaphore post doesn't wake
the work queue thread.

### Why it works from NSH but not work queue:

The `i2c get` tool runs from the NSH task (priority 100). The PX4 BMI088 driver runs from
`wq:hp_default` (priority 237). Both call the exact same `sam_i2c_transfer()` function with
the exact same message structure.

Possible explanations for the difference:
1. **Semaphore wake priority inversion** — `nxsem_post()` from ISR may not correctly wake a
   thread at priority 237 while the scheduler has other threads at priority 241-255 waiting
2. **Tail-chaining at equal priority** — if INTFLAG re-asserts during ISR return, the NVIC
   immediately re-enters the ISR (tail-chain) without giving SysTick a chance to fire.
   At equal priority (both 0x80), ARMv7-M tail-chains same-priority pending interrupts.
   If the ISR clears the flag but the hardware immediately sets it again (race), infinite loop.
3. **SERCOM INTFLAG edge case** — on CA90, writing ADDR might set MB immediately (before the
   byte physically goes on the wire), causing the ISR to see a "stale" MB that it handles
   incorrectly, leaving the flag perpetually asserted.
4. **Work queue thread stack/context** — something about the work queue thread's saved context
   or stack alignment causes the ISR return to restore incorrect state.

### NEW — Most likely (hypothesis #5 — PendSV / scheduler wake bug):

The critical difference between polled and ISR mode is NOT the I2C handling — it's the
**NuttX scheduler interaction**. In ISR mode:

1. Work queue thread (priority 237) calls `nxsem_tickwait()` → goes to sleep
2. SERCOM ISR fires → calls `nxsem_post()` → marks thread as ready
3. `nxsem_post` should trigger PendSV to context-switch back to the woken thread
4. PendSV fires → scheduler picks highest-priority ready thread → resumes work queue thread

If step 3 or 4 fails (PendSV doesn't fire, or scheduler picks wrong thread), the work queue
thread never wakes. The transfer times out (50 ms), SWRST fires, driver retries, same failure
repeats in a tight loop starving the system.

**Why it works from NSH (priority 100):** After ISR posts the semaphore, the NSH thread at
priority 100 may not need PendSV at all — the ISR return path may directly resume it if
nothing higher is pending. At priority 237, the scheduler has multiple threads at 241-255
that compete, making the wake path more complex.

**Key test for next session:** Run I2C ISR-mode transfer from a priority-100 background task
(not the work queue). If it works → PendSV/scheduler bug with high-priority thread wake.
If it fails → something else.

**Alternative test:** Disable all work queues except `wq:hp_default`, then start BMI088.
If it works with no competing threads → priority inversion / scheduler race confirmed.

### Older hypothesis (still possible) #2 or #3:

The SERCOM MB flag is level-triggered. After the ISR handles MB and writes the next data byte
(or ADDR for repeated START), the hardware immediately asserts MB again (because the write
completed instantly at the register level, before the physical I2C byte transfer). The ISR
re-enters, sees MB, tries to handle it, but the byte hasn't actually been sent yet — leading
to data corruption or state machine confusion.

**From NSH it works** because the single-threaded execution means there's a small delay between
ISR return and the next scheduler tick, giving the hardware time to actually clock the byte
out on the wire. From the work queue at high priority, the scheduler immediately re-dispatches,
the ISR fires again before the byte is on the wire.

---

## Investigation Plan for Next Session

### Step 1: Confirm ISR storm vs semaphore failure

Write ISR entry count to a **fixed SRAM address** (no syslog, no prints):
```c
// At top of i2c_interrupt():
*(volatile uint32_t *)0x200FFFE0 = g_isr_burst;
*(volatile uint32_t *)0x200FFFE4 = intflag;
*(volatile uint32_t *)0x200FFFE8 = status;
```

After the freeze, read these addresses via GDB/OpenOCD, or add an NSH command that
reads them after a reboot (SRAM contents may survive warm reset if not cleared by __start).

**Expected outcomes:**
- `g_isr_burst` is very large (>10000) → ISR storm confirmed
- `g_isr_burst` is small (1-5) → ISR never fires again → semaphore wake failure

### Step 2: If ISR storm confirmed — find the re-trigger cause

Add a **GPIO toggle** (e.g., LED1 pin PB22) at ISR entry. Connect scope/LA to PB22 + SDA + SCL.
Observe:
- Is the I2C bus idle (SCL/SDA both high) while the ISR storms? → hardware flag stuck
- Is the I2C bus active (clocking) while ISR storms? → ISR handling wrong, bus keeps going
- What's the ISR rate? MHz = tail-chaining. kHz = real bus activity.

### Step 3: Fix based on findings

**If flag is stuck (bus idle, ISR storming):**
- The ISR needs to check BUSSTATE before handling MB/SB
- If BUSSTATE=IDLE and MB fires, it's a stale flag → clear and return
- Add: `if ((status & BUSSTATE_MASK) == BUSSTATE_IDLE) { clear flags; return; }`

**If tail-chaining (MB re-asserts immediately after DATA write):**
- Disable the specific SERCOM interrupt (INTENCLR) after writing DATA
- Re-enable it after the byte physically completes (poll SYNCBUSY first)
- OR: switch to using only SERCOM-level interrupt enable (INTENSET/INTENCLR) instead
  of NVIC-level enable (up_enable_irq) — this avoids NVIC tail-chaining

**If semaphore wake failure:**
- Check if `nxsem_post()` from ISR context works correctly on CA90
- Verify `CONFIG_PRIORITY_INHERITANCE` is not interfering
- Try `nxsem_post()` → add a DMB + DSB before it (memory barrier for Cortex-M7 write buffer)

### Step 4: Production decision

Once ISR mode works, compare performance:
- Polled: 11 Hz accel (blocking), simple code, zero risk
- ISR: 200 Hz accel (non-blocking), complex, requires proven stability

If ISR fix is stable for 1+ hour continuous operation → use ISR mode.
If ISR fix is flaky → keep polled mode + dedicated work queue thread for I2C sensors.

---

## Current Driver State

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_i2c_master.c`

**Current mode:** Polled (ISR code still present but interrupts never enabled in transfer path)

**Key defines:**
- `I2C5_GCLK_FREQ = 100 MHz` (GCLK2)
- `I2C5_DEFAULT_FREQ = 400000` (400 kHz)
- `I2C_TIMEOUT_USEC = 50000` (50 ms)
- BAUD register = 120 → 400 kHz actual

**Sensor status:**
- BMI088 accel: 11.3 Hz on `wq:hp_default` (limited by polled blocking)
- BMM150 mag: 20 Hz on `wq:hp_default` (correct for magnetometer)
- ICM-20689: 399.8 Hz on `wq:SPI3` (SPI polled, unaffected)

---

## Files Modified During This Investigation

| File | Change |
|------|--------|
| `sam_i2c_master.c` | Rewrote transfer to polled mode; ISR code retained but unused |
| `rc.board_sensors` | I2C sensors commented out for manual testing |
| `default.px4board` | Stack sizes increased (can revert) |
| `board_config.h` | No changes needed |

## Related SAMV71 Reference

The SAMV71 TWIHS I2C driver (`arch/arm/src/samv7/sam_twihs.c`) uses ISR mode successfully:
- ISR handles ALL messages in sequence (no return to task between msgs)
- Uses TXCOMP interrupt for message completion
- Never disables/re-enables interrupts between messages
- Different IP (TWIHS vs SERCOM) — patterns may not directly apply

## Key Difference: SERCOM vs TWIHS

| Aspect | TWIHS (SAMV71) | SERCOM I2C (CA90) |
|--------|---------------|-------------------|
| Interrupt model | TXCOMP fires once per message | MB/SB fires per byte |
| Repeated START | Hardware automatic | Explicit ADDR write while OWNER |
| Bus state | Not exposed | STATUS.BUSSTATE register |
| SYNCBUSY | No | Yes — writes are async |
| Flag clearing | Write to clear register | Write 1 to INTFLAG bit |
| Multi-message | ISR advances msg pointer | ISR advances msg pointer (our impl) |

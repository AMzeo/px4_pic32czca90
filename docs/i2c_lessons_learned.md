# I2C Driver — Lessons Learned & Critical Fixes

**Date:** 2026-06-09 to 2026-06-10
**Outcome:** I2C sensors working in production config (ISR mode, dedicated wq:I2C5 thread)

---

## Summary of Issues & Fixes (in order of discovery)

### Fix 1: `nxsem_set_protocol(&waitsem, SEM_PRIO_NONE)` — CRITICAL

**Symptom:** System hard-freezes when PX4 work queue (priority 237) calls `i2c_transfer()`.
Same code works perfectly from NSH (priority 100). 10,000+ transfers from NSH = zero failures.

**Root cause:** Without `SEM_PRIO_NONE`, NuttX applies priority inheritance logic inside
`nxsem_post()` when called from ISR context. With a high-priority waiting thread (work queue
at 237) and multiple other threads at 241-255, the inheritance calculation corrupts scheduler
state or causes undefined behavior from ISR context.

**Fix:** One line in `sam_i2cbus_initialize()`:
```c
nxsem_init(&priv->waitsem, 0, 0);
nxsem_set_protocol(&priv->waitsem, SEM_PRIO_NONE);  // ← THIS LINE
```

**Rule:** ANY NuttX semaphore that is posted from ISR context MUST have `SEM_PRIO_NONE` set.
This is not optional. Both SAMV71's `sam_twihs.c` and our own `sam_sdmmc.c` do this.
SAMD5E5's NuttX i2c driver does NOT do this (bug in upstream NuttX — works by luck on
that platform due to different scheduler behavior).

**How to detect:** System freezes ONLY from high-priority work queue context. Works from NSH.
No error messages, no output, LEDs frozen.

---

### Fix 2: Work Queue Routing — `wq:I2C5` dedicated thread

**Symptom:** I2C sensors (BMM150, BMP388) land on `wq:hp_default` instead of a dedicated
I2C thread. This means they share CPU time with `manual_control`, `battery_status`, etc.
SAMV71 has `wq:I2C1` — dedicated thread for I2C sensors only.

**Root cause:** PX4's `WorkQueueManager.cpp` has a switch statement for I2C bus routing:
```cpp
case 0: return wq_configurations::I2C0;
case 1: return wq_configurations::I2C1;
case 2: return wq_configurations::I2C2;
case 3: return wq_configurations::I2C3;
case 4: return wq_configurations::I2C4;
// No case 5! Falls through to hp_default
```

Our I2C bus is SERCOM5 = PX4 bus 5 — not handled.

**Fix:** Added `case 5: return wq_configurations::I2C5;` plus:
- `WorkQueueManager.hpp`: Added `I2C5` wq_config_t definition
- `Kconfig`: Added `CONFIG_WQ_I2C5_PRIORITY` entry

**Rule:** If using I2C bus number > 4, add the corresponding case to WorkQueueManager.
Or use bus number ≤ 4 in `i2c.cpp` / `board_config.h` (requires `micro_hal.h` mapping).

---

### Fix 3: Bus State OWNER→IDLE recovery

**Symptom:** After first transfer, bus stays in OWNER state. Next transfer hangs in
bus-wait loop forever (EBUSY).

**Root cause:** On CA90 SERCOM, STOP completion does NOT generate an MB interrupt (unlike
SAMD5x documentation). The bus transitions OWNER→IDLE asynchronously after the STOP
physically completes on the wire. If the next transfer starts before that transition,
it sees OWNER and hangs.

**Fix:** In the bus-wait loop, if msg[0] sees BUSSTATE=OWNER, force IDLE:
```c
if (i == 0 && busstate == BUSSTATE_OWNER) {
    putreg8(I2C_INT_ALL, base + INTFLAG_OFFSET);
    putreg16(BUSSTATE_IDLE, base + STATUS_OFFSET);
    i2c_wait_syncbusy(base);
    break;
}
```

**Rule:** On CA90 SERCOM I2C, always force BUSSTATE=IDLE before starting a new transfer
if the bus is stuck in OWNER from a prior STOP.

---

### Fix 4: ISR-driven multi-message sequencing (SAMV71 pattern)

**Symptom:** Multi-message transfers (write reg + read data = 2 messages) work once (probe)
but fail on subsequent calls from work queue.

**Root cause:** Original code returned to task context between msg[0] and msg[1]. The task
did CTRLB writes, INTFLAG clearing, and INTENSET that corrupted the SERCOM state machine
between messages. On CA90, you cannot write CTRLB.CMD=0 while in OWNER state.

**Fix:** Handle all messages inside the ISR via `i2c_start_next_msg()`:
- ISR completes msg[0] → immediately writes ADDR for msg[1] (repeated START)
- ISR completes msg[1] → issues STOP, disables interrupts, wakes caller
- Task context only sees one `nxsem_tickwait` for the entire multi-message sequence

**Rule:** Never return to task context between I2C messages. The ISR must own the entire
transaction from START to STOP. This matches SAMV71's TWIHS driver pattern.

---

## What Did NOT Cause the Problem (red herrings we chased)

| Theory | Why it was wrong |
|--------|-----------------|
| NVIC priority (ISR too high) | All IRQs already at 0x80 from `up_irqinitialize` |
| NVIC priority (ISR too low) | Setting below 0x80 breaks NuttX (basepri masks it) |
| Stack overflow in work queue | SAMV71 uses same 3000-byte stacks and works |
| SPI interrupt conflict | Disabling SPI didn't help |
| Multi-byte reads broken | `i2c dump 6` works 100% from NSH |
| Repeated START broken | `i2c get` (2-msg) works 200x from NSH |
| FIFO enabled (CTRLC) | DFP confirms FIFOEN=0 at reset |
| SYNCBUSY infinite loop | Added timeout, didn't fix the freeze |
| ISR storm / tail-chaining | Runaway guard didn't prevent freeze |

---

## Production I2C Driver Configuration (final state)

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/pic32czca90/sam_i2c_master.c`

- Mode: ISR-driven with `nxsem_tickwait` (not polled)
- `SEM_PRIO_NONE` set on waitsem
- Multi-message handled entirely in ISR (`i2c_start_next_msg`)
- Bus OWNER→IDLE forced before each new transfer
- Timeout: 50ms per transfer (`I2C_TIMEOUT_USEC = 50000`)
- BAUD: 120 (400 kHz at 100 MHz GCLK)
- Interrupt lines: ALL 7 SERCOM5 vectors (Harmony pattern)
  - SERCOM5_6 (EXTINT+90) = Error
  - SERCOM5_0 (EXTINT+92) = MB (DRE)
  - SERCOM5_2 (EXTINT+94) = SB (I2C Data Ready) — NOT SERCOM5_1!
- SYNCBUSY wait after EVERY write to ADDR, DATA, CTRLB (Harmony pattern)

**Work Queue:** `wq:I2C5` (dedicated thread, added to PX4 WorkQueueManager)

**PX4 Bus:** 5 (matches SERCOM number; `PX4_NUMBER_I2C_BUSES=5`)

---

### Fix 5: Wrong SB NVIC vector — CRITICAL (Root Cause of work-queue timeout)

**Symptom:** I2C transfers work perfectly from NSH shell context but timeout (50ms) from
PX4 work queue context. MB interrupt fires but SB never reaches the CPU. Diagnostic shows
IF=0x00, ST=0x0020 (BUSSTATE=OWNER) — bus stuck waiting for SB delivery.

**Root cause:** DFP `instance/sercom5.h` defines I2C Data Ready (SB) interrupt source as
`SERCOM5_I2C_1_INT_SRC = 94` — NVIC vector EXTINT+94 = `SAM_IRQ_SERCOM5_2`.
We attached to `SAM_IRQ_SERCOM5_1` (EXTINT+93 = TXC/Address Match) — NOT the SB vector!

Why it worked from NSH: slower timing meant by the time MB ISR returned, SB INTFLAG was
already set. When MB re-entered (tail-chaining), handler saw SB=1 in INTFLAG and processed
it in the same ISR entry. From work queue (faster context switch), MB ISR returned BEFORE
SB set, and the pending interrupt on vector 94 was never delivered — CPU never received it.

**Fix:** Enable ALL 7 SERCOM5 NVIC vectors to the same handler (Harmony pattern from
`interrupts.c` lines 368-374). Handler reads INTFLAG to determine what happened.

**Rule:** On CA90, SERCOM I2C interrupt vector assignments DO NOT match USART/SPI names.
The `_N` suffix in `SAM_IRQ_SERCOMx_N` is position-in-vector-table, NOT function.
Always verify against DFP `instance/sercomN.h` `SERCOM_I2C_*_INT_SRC` defines.

---

### Fix 6: Missing SYNCBUSY waits in ISR — CRITICAL

**Symptom:** Intermittent data corruption or missed bytes at 300 MHz CPU / 100 MHz GCLK.
ADDR, DATA, and CTRLB writes in ISR take effect before the peripheral has synchronized
them into its clock domain.

**Root cause:** Harmony `plib_sercom0_i2c_master.c` waits for SYNCBUSY=0 after EVERY
write to ADDR (line 288), DATA (line 581), and CTRLB CMD (lines 560, 599, 637) — even
from ISR context. Our driver wrote these without waiting. At 300 MHz CPU with 100 MHz
peripheral clock, the CPU can issue the next ISR operation before the write propagates.

**Fix:** Added `i2c_wait_syncbusy(base)` after every write to ADDR, DATA, and CTRLB
in both the ISR and the `i2c_start_next_msg()` helper.

**Rule:** On CA90 SERCOM, ALWAYS wait for SYNCBUSY after writing ADDR, DATA, CTRLB,
or STATUS — regardless of whether you're in task or ISR context.

---

## Rules for Future I2C Work on CA90

1. **Always use `SEM_PRIO_NONE`** on any semaphore posted from ISR
2. **Never return to task context between I2C messages** — ISR handles the full sequence
3. **Force BUSSTATE=IDLE** before every new transfer (CA90 STOP doesn't auto-transition)
4. **Bus number must have a case in WorkQueueManager.cpp** or sensors land on wrong wq
5. **Verify with `work_queue status`** — sensors must be on `wq:I2Cn`, not `wq:hp_default`
6. **Test from work queue, not just NSH** — NSH testing is insufficient for ISR-mode validation
7. **CA90 SERCOM ≠ SAMD5x SERCOM** — never assume D5x documentation applies to CA90
8. **Enable ALL 7 SERCOM NVIC vectors** for I2C — SB fires on vector+4 (SERCOM5_2), not vector+3
9. **SYNCBUSY after EVERY register write** to ADDR, DATA, CTRLB, STATUS — even in ISR

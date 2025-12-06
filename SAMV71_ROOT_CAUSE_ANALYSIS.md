# SAMV71 HSMCI Sustained Logging Failure - Root Cause Analysis

## Executive Summary

Two potential root causes have been identified for the sustained SD card logging failure:

| Analysis | Root Cause | Expected Symptom | Analyst |
|----------|------------|------------------|---------|
| **Analysis A** | Completion handshake deadlock | ETIMEDOUT (errno 116) | Claude |
| **Analysis B** | Cache coherency failure | UNRE, data corruption | Gemini |

**Observed Symptom:** errno 116 (ETIMEDOUT) - strongly suggests Analysis A is primary cause.

**Recommendation:** Both issues may co-exist and should be addressed in sequence.

---

## 1. Problem Statement

From `attention_gemini.md`:
> "Manual SD write/read probes succeed (`echo`/`cat`), but any sustained logging (`logger start`) still times out with errno 116"

This indicates a timing-dependent bug that manifests only under sustained write load.

---

## 2. Architectural Comparison: FreeRTOS vs NuttX

### 2.1 FreeRTOS Approach (Reference Implementation)

| Aspect | FreeRTOS Implementation |
|--------|------------------------|
| **Completion Model** | Single callback, polling-based |
| **Cache Strategy** | Global flush (`SCB_CleanInvalidateDCache()`) before every transfer |
| **State Machine** | Simple: IDLE → CMD → LOCKED/ERROR |
| **DMA Descriptors** | Linked-list per block |

Key code from `mcid_dma.c`:
```c
// Cache: Nuclear option - flush everything
SCB_CleanInvalidateDCache();

// Completion: Single point
if (0 == dwMsk || pMcid->bState == MCID_ERROR) {
    _FinishCmd(pMcid, pCmd->bStatus);
}
```

### 2.2 NuttX Approach (Current Implementation)

| Aspect | NuttX Implementation |
|--------|---------------------|
| **Completion Model** | Dual-event (XFRDONE + DMA callback), interrupt-driven |
| **Cache Strategy** | Targeted buffer operations (`up_clean_dcache()`, `up_invalidate_dcache()`) |
| **State Machine** | Complex: Multiple flags (dmabusy, xfrbusy, txbusy, waitevents) |
| **DMA Descriptors** | Single setup call with total buffer size |

**Critical Requirement:** Both XFRDONE and DMA callback must fire AND correctly update state for semaphore wake-up.

---

# ANALYSIS A: Completion Handshake Deadlock (Claude)

## A.1 The Mechanism

**Root Cause:** Fix #12 in `sam_notransfer()` creates a deadlock when XFRDONE interrupt fires before DMA completion.

### A.1.1 Normal Expected Flow

**Scenario A: DMA completes first**
```
1. DMA callback → dmabusy=false, xfrbusy=true → no wake-up
2. XFRDONE → sam_endtransfer() → sam_notransfer() → xfrbusy=false
3. dmabusy=false check passes → sam_endwait() → SUCCESS
```

**Scenario B: XFRDONE completes first**
```
1. XFRDONE → sam_endtransfer() → sam_notransfer() → xfrbusy=false
2. dmabusy=true → no wake-up (expected, wait for DMA)
3. DMA callback → dmabusy=false, xfrbusy=false → sam_endwait() → SUCCESS
```

### A.1.2 Actual Flow with Fix #12 (THE BUG)

**When XFRDONE fires before DMA completes:**

```
1. XFRDONE fires
2. sam_endtransfer() called
3. sam_notransfer() called
   - Fix #12 checks: if (priv->dmabusy) { return; }  // <-- EARLY RETURN!
   - xfrbusy stays TRUE (never set to false!)
4. sam_endtransfer() checks: if (!priv->dmabusy && ...)
   - dmabusy=true → NO WAKE-UP
5. DMA callback fires
6. sam_dmacallback() checks: if (!priv->xfrbusy && ...)
   - xfrbusy=true (never cleared!) → NO WAKE-UP
7. Neither path wakes semaphore
8. Watchdog fires after 5 seconds → ETIMEDOUT (errno 116)
```

### A.1.3 The Code Evidence

**sam_notransfer() at line 1419-1451:**
```c
static void sam_notransfer(struct sam_dev_s *priv)
{
  /* FIX #12: Guard against disabling DMA while transfer is active. */
  if (priv->dmabusy)
    {
      mcerr("INFO: sam_notransfer called while DMA active - skipping (safe)\n");
      return;  // <-- EARLY RETURN: xfrbusy NEVER SET TO FALSE!
    }
  // ...
  priv->xfrbusy = false;  // <-- Never reached when dmabusy=true
  priv->txbusy  = false;
}
```

**sam_endtransfer() at line 1352-1401:**
```c
static void sam_endtransfer(struct sam_dev_s *priv, sdio_eventset_t wkupevent)
{
  sam_disablexfrints(priv);
  sam_notransfer(priv);  // <-- May return early without clearing xfrbusy!

  // For successful XFRDONE (not error/timeout):
  // wkupevent = SDIOWAIT_TRANSFERDONE
  // The error check below is FALSE, dmabusy is NOT cleared here
  if ((wkupevent & (SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR)) != 0)
    {
      priv->dmabusy = false;  // Only on error!
      sam_dmastop(priv->dma);
    }

  // dmabusy is still TRUE for success case → NO WAKE-UP
  if (!priv->dmabusy && (priv->waitevents & wkupevent) != 0)
    {
      sam_endwait(priv, wkupevent);  // Never reached!
    }
}
```

**sam_dmacallback() at line 1168-1230:**
```c
static void sam_dmacallback(DMA_HANDLE handle, void *arg, int result)
{
  if (priv->dmabusy)
    {
      priv->dmabusy = false;
      // ...
      // xfrbusy is TRUE because sam_notransfer returned early!
      else if (!priv->xfrbusy && (priv->waitevents & SDIOWAIT_TRANSFERDONE) != 0)
        {
          sam_endwait(priv, SDIOWAIT_TRANSFERDONE);  // Never reached!
        }
    }
}
```

## A.2 Why This Causes errno 116

- **errno 116 = ETIMEDOUT** in Linux/NuttX
- The semaphore wait in `sam_eventwait()` never gets signaled
- The 5-second watchdog timer fires and returns ETIMEDOUT
- This matches the observed symptom exactly

## A.3 The Fix for Analysis A

**Option A.1: Fix sam_notransfer() Logic (Minimal Change)**

```c
static void sam_notransfer(struct sam_dev_s *priv)
{
  /* FIX #12 REVISED: Don't touch hardware while DMA active,
   * BUT still update software state for completion tracking.
   */
  if (!priv->dmabusy)
    {
      /* Only touch hardware when DMA is done */
      /* (previously had WRPROOF/RDPROOF clearing here) */
    }

  /* ALWAYS update transfer flags regardless of DMA state */
  priv->xfrbusy = false;
  priv->txbusy  = false;
}
```

**Option A.2: Fix sam_endtransfer() Logic (More Robust)**

```c
static void sam_endtransfer(struct sam_dev_s *priv, sdio_eventset_t wkupevent)
{
  sam_disablexfrints(priv);

  /* CRITICAL: Clear xfrbusy BEFORE calling sam_notransfer()
   * This ensures DMA callback can wake us up even if notransfer returns early
   */
  priv->xfrbusy = false;

  sam_notransfer(priv);  // Now only does hardware cleanup

  // ... rest of function
}
```

---

# ANALYSIS B: Cache Coherency Failure (Gemini)

## B.1 The Mechanism

**Root Cause:** NuttX uses targeted cache operations that may miss buffer regions, causing DMA to read stale/garbage data from RAM.

### B.1.1 The Structural Problem

The Logger fails because it relies on Transmit (TX) DMA working reliably under high CPU load. The current driver works for RX DMA but fails for TX DMA.

**Cache Coherency Fragility:**
- NuttX uses a "surgical" approach (cleaning only the specific buffer range)
- This requires perfect MPU configuration, buffer alignment (32-byte cache line boundaries), and timing
- The Logger generates data rapidly in varying buffer locations
- If any part of the buffer remains in CPU cache when DMA starts, XDMAC reads stale data (zeros/garbage) from RAM
- This causes FIFO Underruns (UNRE) or Timeouts

**FreeRTOS Difference:**
- FreeRTOS uses the "Nuclear Option" (`SCB_CleanInvalidateDCache`)
- It flushes the entire data cache before every transfer
- It doesn't care about buffer alignment or MPU regions
- It guarantees RAM is 100% coherent with the CPU

### B.1.2 Why Targeted Cache Operations Fail

```
CPU writes to buffer:
  [Cache Line 0] [Cache Line 1] [Cache Line 2] [Cache Line 3]
       ↓              ↓              ↓              ↓
  [Dirty]        [Dirty]        [Dirty]        [Dirty]

up_clean_dcache(buffer, buffer+len):
  - If buffer is not 32-byte aligned, Cache Line 0 might be partially cleaned
  - If len is not 32-byte aligned, Cache Line 3 might be partially cleaned
  - DMA reads from RAM, sees stale data at boundaries

Result: UNRE (underrun) or corrupted data sent to SD card
```

### B.1.3 Expected Symptoms from Cache Issues

- **HSMCI_INT_UNRE**: Data transmit underrun (DMA starved)
- **HSMCI_INT_DCRCE**: Data CRC error (corrupted data)
- **Data corruption**: Files written with garbage bytes

**Note:** These are different from errno 116 (timeout), which is a completion issue.

## B.2 The Fix for Analysis B

### B.2.1 Transmit Path (Fixing the Logger)

In `sam_dmasendsetup()`:

```c
// REMOVE THIS:
// up_clean_dcache((uintptr_t)buffer, (uintptr_t)buffer + buflen);

// REPLACE WITH:
ARM_DSB();  // Data Synchronization Barrier
ARM_ISB();  // Instruction Synchronization Barrier
SCB_CleanDCache();  // Or up_clean_dcache_all() if available
```

**Why:** The logger writes data to a buffer and immediately requests a write. The "Clean" operation pushes data from Cache → RAM. If you miss even one byte (due to alignment), the DMA sends garbage. Flushing everything guarantees the DMA sees the exact data the logger generated.

### B.2.2 Receive Path (Stability)

In `sam_dmarecvsetup()`:

```c
// REMOVE THIS:
// up_invalidate_dcache((uintptr_t)buffer, (uintptr_t)buffer + buflen);

// REPLACE WITH:
SCB_CleanInvalidateDCache();  // Or up_flush_dcache_all()
```

**Why:** This ensures the CPU reads fresh data from RAM after the SD card writes to it, regardless of where that data landed.

### B.2.3 Performance Considerations

| Approach | Performance Impact | Reliability |
|----------|-------------------|-------------|
| Targeted cache ops | Minimal | Fragile |
| Global cache flush | ~100-500 cycles per transfer | Robust |

For SD card operations (milliseconds per transfer), the cache flush overhead is negligible.

---

# 3. Comparison of Analyses

## 3.1 Symptom Mapping

| Symptom | Analysis A (Deadlock) | Analysis B (Cache) |
|---------|----------------------|-------------------|
| **errno 116 (ETIMEDOUT)** | Primary cause | Secondary/contributing |
| **UNRE (underrun)** | Not caused | Primary symptom |
| **Data corruption** | Not caused | Primary symptom |
| **Manual ops work** | Explained (timing) | Explained (alignment luck) |
| **Sustained ops fail** | Explained (race condition) | Explained (rapid buffers) |

## 3.2 Why errno 116 Points to Analysis A

The observed symptom is specifically **ETIMEDOUT**, not data errors:

1. **Timeout** means the driver is waiting for something that never signals
2. This is a **completion handling** issue, not a data transfer issue
3. Cache corruption would cause UNRE/DCRCE errors, which would be caught and reported differently
4. The semaphore-based wait is what times out, and Analysis A explains exactly why it never gets signaled

## 3.3 Both Issues May Co-exist

These analyses are **complementary, not contradictory**:

1. **Cache coherency issues** → Some transfers may fail with data errors
2. **Completion deadlock** → Even successful transfers timeout because semaphore never wakes

The FreeRTOS driver avoids BOTH by:
- Using `SCB_CleanInvalidateDCache()` for cache (Gemini's point)
- Using polling instead of interrupt-driven completion (Claude's point)

---

# 4. Why Manual Operations Work

**Analysis A Explanation:**
- Manual operations have sufficient time between each command
- User types command, waits for output
- Driver state fully resets between operations
- Different timing: DMA often completes before XFRDONE
- No rapid back-to-back writes that trigger the race condition

**Analysis B Explanation:**
- Manual operations use well-aligned buffers
- Single commands are more likely to have proper cache line boundaries
- Time between operations allows cache to naturally synchronize

---

# 5. Recommended Fix Strategy

## 5.1 Phased Approach

**Phase 1: Apply Analysis A Fix (addresses errno 116 directly)**
```c
// In sam_notransfer(): Always clear flags regardless of dmabusy
priv->xfrbusy = false;
priv->txbusy  = false;
```

**Rationale:**
- 2-line change with zero performance impact
- Directly addresses the observed errno 116 symptom
- Low risk, easily reversible

**Phase 2: If still failing, apply Analysis B Fix**
```c
// In sam_dmasendsetup(): Replace targeted clean with global flush
ARM_DSB();
ARM_ISB();
SCB_CleanDCache();  // Or equivalent NuttX function
```

**Rationale:**
- Addresses potential cache coherency issues
- Matches proven FreeRTOS approach
- Minimal performance impact for SD card operations

## 5.2 Why This Order?

1. **Symptom match:** errno 116 is a timeout, not a data error
2. **Risk assessment:** Phase 1 is minimal code change; Phase 2 affects cache globally
3. **Diagnostic value:** If Phase 1 fixes it, we've confirmed the root cause

---

# 6. Test Plan

## 6.1 Phase 1 Testing

1. Apply Analysis A fix to `sam_notransfer()`
2. Rebuild PX4 firmware
3. Flash and boot
4. Run: `logger start`
5. Wait 60 seconds
6. Run: `logger stop`
7. Check for errno 116 or other timeouts
8. Verify log files written to SD card

**Expected Result:** No more errno 116 if Analysis A is correct.

## 6.2 Phase 2 Testing (if Phase 1 insufficient)

1. Apply Analysis B fix to `sam_dmasendsetup()` and `sam_dmarecvsetup()`
2. Rebuild and flash
3. Repeat logger test
4. Monitor for UNRE or data corruption

---

# 7. Risk Assessment

## Analysis A Fix
- **Risk Level:** Low
- **Impact:** Only changes software flag updates, not hardware interactions
- **Regression Risk:** None expected for manual operations
- **Reversibility:** Easy to revert

## Analysis B Fix
- **Risk Level:** Low-Medium
- **Impact:** Global cache flush affects entire system briefly
- **Regression Risk:** Slight performance impact on other subsystems during SD operations
- **Reversibility:** Easy to revert

---

# 8. Document History

| Date | Change | Author |
|------|--------|--------|
| Initial | Analysis A: Completion deadlock identified | Claude |
| Updated | Analysis B: Cache coherency added | Gemini (via user) |
| Updated | Combined document with comparison | Claude |

## Files Analyzed
- NuttX: `sam_hsmci.c` lines 1168-1451 (completion handling)
- FreeRTOS: `mcid_dma.c` (complete file, cache and completion)
- Documentation: All debug logs in project folder
- Previous fixes: SAMV71_HSMCI_FIXES_COMPLETE.md (Fixes #1-#25)

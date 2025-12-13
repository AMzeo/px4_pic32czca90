# SAMV71 FMUv6 Parity Roadmap

**Goal:** PX4 full flight stack on SAMV71-based FMU board with FMUv6-class feature parity

**Target:** Production-ready flight controller (not dev kit)

---

## Executive Summary

SAMV71 is Cortex-M7 but materially different from STM32H7 (peripheral set, DMA/cache model, timer capabilities). This plan achieves "PX4 full flight stack on SAMV71" - not bit-for-bit identical hardware.

### Must-Have for Flight
- Multiple IMUs with DRDY
- Barometer(s)
- Magnetometer
- High-rate PWM outputs (minimum)
- RC input
- SD card logging
- Watchdog
- USB (stable)

### Nice-to-Have
- DShot protocol
- CAN/UAVCAN
- Ethernet
- Telemetry RPM

---

## Workstreams & Milestones

### Phase 1: Foundation (Current → Week 4)

#### WS-1: FMUv6 Parity Matrix
**Status:** To Do

Create checklist mapping:
| FMUv6 Feature | PX4 Module | NuttX/Board Hook | SAMV71 Status |
|---------------|------------|------------------|---------------|
| PWM Outputs | PWM Out | io_timer | Partial |
| DShot | DShot | io_timer + DMA | Not Started |
| RC Input | RC Input | io_timer capture | Not Started |
| SD Logging | Logger | HSMCI | Working |
| SPI Sensors | Drivers | SPI + DRDY | Partial |
| I2C Sensors | Drivers | TWIHS | Working |
| CAN | UAVCAN | MCAN | Not Started |
| USB | MAVLink/NSH | USBHS | Stubbed |
| Watchdog | Safety | WDT | Not Started |
| Power Control | Board | GPIO | Not Started |

**Deliverable:** Complete requirements matrix document

---

#### WS-2: Hardware Design
**Status:** In Progress (using SAMV71-XULT dev kit)

**Production FMU Requirements:**
- [ ] Sensor SPI buses with separate CS lines
- [ ] DRDY lines to IRQ-capable pins
- [ ] Power rails with enable controls
- [ ] Multiple UARTs (GPS, Telem, Debug)
- [ ] CAN transceivers
- [ ] Proper USB VBUS sense
- [ ] SD slot wired for stable 4-bit mode
- [ ] Pinmux validation (no TC vs HSMCI conflicts)

**Current Conflicts Identified:**
- TC0 CH2 (PA26) conflicts with HSMCI0 DA2 → Resolved by avoiding PA26 for PWM

**Deliverable:** Validated pinmux and schematic review

---

#### WS-3: GPIO Interrupt Semantics (BLOCKING)
**Status:** To Do
**Priority:** CRITICAL

**Problem:**
`px4_arch_gpiosetevent()` doesn't attach handler like STM32 does.

**Required Behavior:**
```c
// Must work like STM32:
px4_arch_gpiosetevent(pin, rising, falling, event, handler, arg);
// - Attach handler
// - Pass arg to handler
// - Enable/disable reliably per pin
```

**Validation Criteria:**
- [ ] DRDY-driven SPI sensors run at high rate
- [ ] No polling required for sensor reads
- [ ] Multiple simultaneous DRDY interrupts work

**Files to Modify:**
- `boards/microchip/samv71-xult-clickboards/src/sam_gpiosetevent.c`
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/micro_hal.h`

**Deliverable:** Working DRDY-driven ICM20689 at 8kHz

---

### Phase 2: Core Flight (Week 4 → Week 8)

#### WS-4: Cache/DMA Coherency
**Status:** Partial (nocache region exists)
**Priority:** HIGH

**Standardize DMA Buffer Rules:**

| Buffer Type | Location | Cache Ops | Alignment |
|-------------|----------|-----------|-----------|
| SPI TX/RX | .nocache | None | 4-byte |
| HSMCI | .nocache | None | 32-byte |
| USB | .nocache | None | 4-byte |
| DShot DMA | .nocache | None | 4-byte |

**Implementation:**
- [ ] Verify `board_dma_alloc()` used consistently
- [ ] Document alignment requirements
- [ ] Add cache clean/invalidate helpers if needed
- [ ] Stress test SPI + SD + USB concurrent

**Deliverable:** DMA coherency guide + validated concurrent operation

---

#### WS-5: IO Timer Feature Complete
**Status:** PWM-out only
**Priority:** HIGH

**Required Capabilities:**

| Mode | Status | Description |
|------|--------|-------------|
| PWM Out | Working | 3 channels |
| OneShot | Not Started | ESC protocols |
| Input Capture | Not Started | RC input |
| DMA Waveform | Not Started | DShot |
| Timer Allocation | Not Started | Multi-subsystem coexist |

**Implementation Plan:**
```
TC0 CH0 - HRT (reserved)
TC0 CH1 - PWM1 (PA15)
TC0 CH2 - RESERVED (PA26 conflict)
TC1 CH0 - PWM2 (PC23)
TC1 CH1 - PWM3 (PC26)
TC1 CH2 - RC Input Capture (PC29)
```

**Deliverables:**
- [ ] PX4 mixer outputs working
- [ ] Arming/disarming safe states
- [ ] Failsafe behavior
- [ ] RC input via capture

---

#### WS-6: Motor Protocols
**Status:** PWM Only
**Priority:** MEDIUM

**Minimum Viable:**
- [ ] PWM 50Hz-400Hz working
- [ ] OneShot125 working
- [ ] Safe disarm behavior

**Enhanced (if feasible):**
- [ ] DShot150/300/600
- [ ] Bidirectional DShot (RPM telemetry)

**Note:** If TC can't do DMA burst cleanly, document constraints and provide robust PWM as minimum.

**Deliverable:** Motor spin test with arming sequence

---

### Phase 3: Production Quality (Week 8 → Week 12)

#### WS-7: Storage & Logging
**Status:** Working (with manual mount)
**Priority:** MEDIUM

**Required Changes:**
- [ ] Remove manual mount from board init
- [ ] Align with PX4/NuttX expected flow (card detect, automount)
- [ ] Verify sustained ULog write at flight rates
- [ ] Long-duration stress test (1+ hour)

**Validation:**
- 500 Hz IMU logging
- No buffer overruns
- Clean unmount on disarm

**Deliverable:** 1-hour logging test pass

---

#### WS-8: USB Production Ready
**Status:** Stubbed
**Priority:** MEDIUM

**Required:**
- [ ] Actual VBUS detection GPIO
- [ ] Correct CDCACM autostart behavior
- [ ] Hot-plug stability
- [ ] No DMA/cache/timing destabilization

**Test Cases:**
- Connect during flight logging
- Disconnect during MAVLink stream
- Rapid connect/disconnect cycles

**Deliverable:** USB stability test suite pass

---

#### WS-9: Sensor Stack Parity
**Status:** ICM20689 working
**Priority:** HIGH

**Target Configuration:**
| Sensor | Type | Interface | Status |
|--------|------|-----------|--------|
| ICM20689 | IMU | SPI + DRDY | Working |
| ICM42688P | IMU (backup) | SPI + DRDY | Not Started |
| BMP388 | Baro | SPI | In Progress |
| AK09915 | Mag | I2C | Configured |

**Required:**
- [ ] SPI locking correct
- [ ] Bus reset paths validated
- [ ] DRDY interrupt handling
- [ ] Multi-sensor concurrent operation

**Deliverable:** All sensors publishing at rate

---

#### WS-10: CAN/UAVCAN
**Status:** Not Started
**Priority:** MEDIUM

**Required:**
- [ ] MCAN driver bring-up
- [ ] PX4 CAN plumbing
- [ ] UAVCAN v1 node allocation
- [ ] Parameter set over CAN
- [ ] Runtime messaging under load

**Deliverable:** UAVCAN GPS/Mag working

---

### Phase 4: Robustness (Week 12 → Week 16)

#### WS-11: System Robustness
**Status:** Mostly Not Started
**Priority:** HIGH

| Feature | Status | Implementation |
|---------|--------|----------------|
| Watchdog | Not Started | SAMV71 WDT + PX4 integration |
| Unique ID | Synthesized | Read true silicon UID |
| Reset-to-bootloader | RAM only | Use SAMV71 GPBR registers |
| Hardfault dump | Not validated | Test with/without SD |

**Deliverables:**
- [ ] Watchdog triggers reset on hang
- [ ] Bootloader mode survives power cycle
- [ ] Crashdump saved to SD

---

#### WS-12: Production Boot/Update
**Status:** Not Started
**Priority:** MEDIUM

**Decisions Required:**
- [ ] Bootloader strategy (custom vs ROM boot)
- [ ] Firmware signing requirements
- [ ] PX4 tooling flash/update method
- [ ] Memory layout (bootloader, app, params)
- [ ] Upgrade resilience (fail-safe update)

**Deliverable:** Documented boot/update architecture

---

### Phase 5: Validation (Week 16 → Week 20)

#### WS-13: Test Strategy

**Test Progression:**
1. **HIL Testing**
   - Sensors + outputs simulation
   - Full flight simulation

2. **Bench Testing**
   - Motor/ESC spin tests
   - Full sensor suite
   - Power cycling

3. **Tethered Hover**
   - Indoor tethered flight
   - Basic stabilization

4. **Free Flight**
   - Outdoor testing
   - Full mission capability

**Automated Stress Tests:**
- [ ] DMA-heavy paths (SPI + SD + USB concurrent)
- [ ] Long-duration logging (4+ hours)
- [ ] Thermal stress
- [ ] Power brownout recovery

**Deliverable:** Test report with pass/fail matrix

---

## Timeline Summary

```
Week 1-4:   Foundation (GPIO, Requirements)
Week 4-8:   Core Flight (Timers, DMA, Motors)
Week 8-12:  Production Quality (USB, Storage, Sensors)
Week 12-16: Robustness (Watchdog, Boot, Safety)
Week 16-20: Validation (Testing, Flight)
```

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| TC timer limitations | Cannot do DShot | Use PWM/OneShot fallback |
| DMA coherency issues | Random faults | Strict nocache policy |
| Pin conflicts | Feature reduction | Careful pinmux planning |
| USB instability | Debug difficulty | UART fallback |

---

## Resource Requirements

- Hardware: Custom FMU PCB (or continued SAMV71-XULT testing)
- Software: NuttX expertise, PX4 driver development
- Testing: ESCs, motors, sensors, test bench

---

## Success Criteria

**Minimum Viable Flight Controller:**
- [ ] Stable attitude control
- [ ] GPS position hold
- [ ] SD logging working
- [ ] USB MAVLink working
- [ ] Watchdog protecting against hangs
- [ ] Safe failsafe behavior

**Full FMUv6 Parity:**
- [ ] All above plus...
- [ ] DShot motor protocol
- [ ] UAVCAN peripherals
- [ ] Production boot/update
- [ ] Full sensor redundancy

---

*Document Version: 1.0*
*Created: December 2024*

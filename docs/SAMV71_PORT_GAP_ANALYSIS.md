# SAMV71 Port Gap Analysis vs STM32 Reference

**Review Date:** December 2024
**Reviewer:** Codex AI
**Reference Target:** STM32 FMUv5x

---

## Executive Summary

The SAMV71 port has the skeleton needed to boot PX4 on NuttX but is missing or stubbed in several "PX4 flight-controller critical" areas compared to the mature STM32 implementation.

**Current Status:** Development/Prototyping
**Target Status:** Production-Ready Flight Controller

---

## Gap Analysis Table

| Feature | SAMV71 Status | STM32 Status | Priority | Effort |
|---------|---------------|--------------|----------|--------|
| GPIO Interrupts | Partial (no callback attach) | Complete | **HIGH** | Medium |
| IO Timer (PWM) | Basic (3 outputs) | Full multi-mode | **HIGH** | High |
| DShot Support | Not implemented | Complete | Medium | High |
| Capture/OneShot | Not implemented | Complete | Medium | High |
| USB VBUS | Stubbed (always present) | Real sensing | Medium | Low |
| DMA Strategy | Placeholder (nocache) | Explicit mapping | Medium | Medium |
| Unique ID (UUID) | Synthesized from CHIPID | True silicon UID | Low | Low |
| Reset Mode Storage | RAM only | RTC backup regs | Medium | Medium |
| Critical Monitor | Stub | DWT cycle counter | Low | Low |
| ADC | Not implemented | Complete | **HIGH** | Medium |
| Watchdog | Not implemented | Complete | **HIGH** | Low |

---

## Detailed Gap Analysis

### 1. GPIO Interrupts (HIGH PRIORITY)

**Problem:**
`sam_gpiosetevent()` configures pin interrupt mode but does NOT attach the callback/arg. Caller must do port-level `irq_attach()` separately.

**Impact:**
- PX4 drivers assume handler gets wired in `gpiosetevent()` call
- Sensor DRDY interrupts may not work correctly
- Card detect interrupts affected

**Files:**
- `boards/microchip/samv71-xult-clickboards/src/sam_gpiosetevent.c:66`
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/micro_hal.h:105`

**STM32 Reference:**
- `stm32_gpiosetevent()` properly wires handler

**Fix Required:**
```c
// sam_gpiosetevent.c needs to call irq_attach() internally
// like STM32 implementation does
```

---

### 2. IO Timer / PWM (HIGH PRIORITY)

**Problem:**
SAMV71 `io_timer_tc.c` is "PWM-out only" - no capture, oneshot, or DShot/DMA.

**Current Capability:**
- 3 PWM outputs only (TC1, TC3, TC4)
- No input capture
- No oneshot timing
- No DShot protocol

**Impact:**
- Cannot use DShot ESCs (modern standard)
- No RC input capture via timer
- Limited servo/ESC protocols

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_tc.c:196`
- `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp:61`

**STM32 Reference:**
- Full multi-mode allocator (PWMOut/OneShot/Capture/Trigger/DShot/LED)
- Per-timer/channel mapping tables
- IRQ handling and DMA-burst

**Fix Required:**
- Implement capture mode
- Add DShot DMA support
- Expand timer allocation

---

### 3. DMA Strategy (MEDIUM PRIORITY)

**Problem:**
SAMV71 DMA is placeholder - relies on MPU nocache region instead of explicit mapping.

**Current Approach:**
- Dynamic XDMAC, no explicit mapping
- 64KB nocache SRAM region
- Cache write-through mode

**Impact:**
- Less efficient than explicit DMA mapping
- May have performance implications
- Works but not optimal

**Files:**
- `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board_dma_map.h:36`
- `boards/microchip/samv71-xult-clickboards/nuttx-config/scripts/script.ld:113`

**STM32 Reference:**
- Explicit DMA stream/channel assignments
- Predictable DMA resources for SPI, SDMMC, DShot, PX4IO

---

### 4. USB VBUS Handling (MEDIUM PRIORITY)

**Problem:**
USB VBUS is stubbed - always reports "present".

**Files:**
- `boards/microchip/samv71-xult-clickboards/src/usb.c:79`

**Impact:**
- Cannot detect USB connection state
- Power management affected

**Fix Required:**
- Configure VBUS sense GPIO
- Implement real VBUS detection

---

### 5. Unique ID (LOW PRIORITY)

**Problem:**
UUID is synthesized from CHIPID CIDR/EXID, not true per-chip unique.

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/version/board_identity.c:79`

**Impact:**
- Multiple boards could have same "UUID"
- MAVLink identification affected

**Note:**
- SAMV71 does have a unique serial number in the chip
- Need to read from correct register

---

### 6. Reset Mode Storage (MEDIUM PRIORITY)

**Problem:**
Reset mode stored in RAM only - won't survive power loss.

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/board_reset/board_reset.cpp:51`

**Impact:**
- Bootloader mode requests lost on power cycle
- Some reset paths may not work

**Fix Required:**
- Use SAMV71 GPBR (General Purpose Backup Registers)
- These survive reset and are battery-backed

---

### 7. Critical Monitor (LOW PRIORITY)

**Problem:**
Critmon is a stub - no actual timing.

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/board_critmon/board_critmon.c:51`

**Impact:**
- No IRQ/critical section timing data
- Debugging harder

**Fix Required:**
- Implement using DWT cycle counter (Cortex-M7 has this)

---

## Architecture Comparison

### Memory Model

| Aspect | SAMV71 | STM32F7 |
|--------|--------|---------|
| Flash | 2MB | 2MB |
| Main SRAM | 320KB | Variable (DTCM/SRAM) |
| Nocache Region | 64KB dedicated | Explicit DMA regions |
| Cache Strategy | Write-through | Mixed |
| MPU | Nocache region | DMA-specific |

### Timer Resources

| Aspect | SAMV71 | STM32F7 |
|--------|--------|---------|
| HRT Timer | TC0 (16-bit) | GPT (32-bit options) |
| PWM Timers | TC1, TC3, TC4 | Multiple TIM |
| Capture | Not implemented | Full support |
| DShot | Not implemented | DMA burst |

---

## Recommended Priority Order

### Phase 1: Critical for Flight (HIGH)
1. **GPIO Interrupts** - Fix callback attachment
2. **ADC** - Battery voltage/current sensing
3. **Watchdog** - Safety critical

### Phase 2: Enhanced Functionality (MEDIUM)
4. **IO Timer Capture** - RC input
5. **USB VBUS** - Proper connection detection
6. **Reset Mode Storage** - Use GPBR registers
7. **DMA Optimization** - Explicit mapping

### Phase 3: Polish (LOW)
8. **DShot Support** - Modern ESC protocol
9. **True UUID** - Unique identification
10. **Critical Monitor** - Debugging support

---

## STM32 Feature Directories (Reference)

```
platforms/nuttx/src/px4/stm/stm32_common/
├── adc/           # ADC driver
├── board_critmon/ # Critical section monitoring
├── board_hw_info/ # Hardware info
├── board_reset/   # Reset handling
├── dshot/         # DShot protocol
├── hrt/           # High-resolution timer
├── io_pins/       # IO timer (full featured)
├── led_pwm/       # LED PWM
├── tone_alarm/    # Tone output
├── version/       # Board identity
└── watchdog/      # Watchdog driver
```

### SAMV7 Currently Has:
```
platforms/nuttx/src/px4/microchip/samv7/
├── board_critmon/ # Stub
├── board_reset/   # RAM-only
├── hrt/           # Working
├── io_pins/       # PWM-only
└── version/       # Synthesized UUID
```

### Missing in SAMV7:
- `adc/`
- `board_hw_info/`
- `dshot/`
- `led_pwm/`
- `tone_alarm/`
- `watchdog/`

---

## Action Items

- [ ] Fix GPIO interrupt callback attachment
- [ ] Implement ADC driver for battery monitoring
- [ ] Add watchdog support
- [ ] Implement timer capture mode
- [ ] Fix USB VBUS detection
- [ ] Use GPBR for reset mode storage
- [ ] Add explicit DMA channel mapping
- [ ] Implement DShot protocol
- [ ] Read true silicon UUID
- [ ] Implement critmon with DWT

---

*This analysis helps prioritize development effort to bring SAMV71 to production-ready status.*

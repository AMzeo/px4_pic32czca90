# DShot Implementation Plan — SAMV7 PWMC + XDMAC

> **Phase 2 of Production Plan**
> **Target:** Unidirectional DShot150/300/600 output on 4 channels
> **Scope:** New driver file + stubs filled in + board config + build integration
> **Bidirectional DShot:** Deferred to Phase 4+ (architecture reserved)

---

## 1. Architecture Overview

### 1.1 STM32 vs SAMV7 — Key Difference

STM32 uses **TIM DMA Burst mode**: DMA writes to DMAR register, hardware
distributes to CCR1-CCR4 based on DCR (DMA Control Register) configuration.
Each timer update event triggers one DMA transfer of N words (burst length).

SAMV7 has **no TIM DMA burst**. Instead, we use **PWMC Synchronous Channel
Mode (SCM)** + **XDMAC**:
- PWMC SCM synchronizes multiple channels to share the same period
- SCM.UPDM=2 enables automatic duty cycle updates from the DMAR register
- XDMAC writes duty values to DMAR; hardware distributes to sync channels
- One XDMAC transfer per period end (SCM.PTRM=0)

### 1.2 Data Flow

```
DShot Frame (16 bits per motor)
    ↓
dshot_motor_data_set() — pack throttle + telemetry + CRC into 16-bit packet
    ↓
up_dshot_trigger() — encode each bit as duty cycle value, fill DMA buffer
    ↓
XDMAC Memory→Peripheral transfer to PWMC DMAR register
    ↓
PWMC hardware distributes duty values to synchronized channels (CH0-CH3)
    ↓
Each bit period: output pin HIGH for CDTY ticks, LOW for remainder of CPRD
    ↓
After 16 bits + 1 reset: transfer complete, wait for next trigger
```

### 1.3 Timing Parameters

**Clock selection:** MCK/2 = 75 MHz (CPRE=1)

Using MCK/2 instead of MCK/8 gives finer timing resolution for DShot encoding.
The `clock_freq` field in `io_timers_t` is 150 MHz (MCK), so effective clock
after prescaler = 75 MHz.

| Protocol | Bit Period | CPRD | T1H (75%) | T0H (37.5%) | Frame Time |
|----------|-----------|------|-----------|-------------|------------|
| DShot150 | 6.67 µs | 500 | 375 | 188 | 106.7 µs |
| DShot300 | 3.33 µs | 250 | 188 | 94 | 53.3 µs |
| DShot600 | 1.67 µs | 125 | 94 | 47 | 26.7 µs |

**Bit encoding (within CPRD period, CPOL=1):**
- Bit 1: CDTY = 75% of CPRD (high for 75% of bit period)
- Bit 0: CDTY = 37.5% of CPRD (high for 37.5% of bit period)
- Reset: CDTY = 0 (output stays low for one full period)

---

## 2. File Structure

### 2.1 New Files

```
platforms/nuttx/src/px4/microchip/samv7/dshot/
├── CMakeLists.txt          # Build integration
└── dshot.c                 # Main DShot driver implementation
```

### 2.2 Modified Files

```
platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer_pwmc.c
    — Fill io_timer_set_dshot_mode() stub
    — Fill io_timer_update_dma_req() stub
    — Add IOTimerChanMode_Dshot case in io_timer_channel_init()

platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt
    — Add dshot/ subdirectory

boards/microchip/samv71-xult-clickboards/default.px4board
    — Enable CONFIG_DRIVERS_DSHOT=y

boards/microchip/samv71-xult-clickboards/init/rc.board_defaults
    — Add dshot start/config (or keep pwm_out for initial testing)
```

### 2.3 Existing Files (Read-Only Reference)

```
src/drivers/dshot/DShot.cpp                    # PX4 DShot output module (calls our API)
src/drivers/drv_dshot.h                        # Public API we must implement
platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c  # STM32 reference
platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_pwm.h   # PWMC registers
platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_xdmac.h # XDMAC registers
platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.h          # NuttX DMA API
```

---

## 3. Implementation Steps

### Step 1: DMA Buffer Design

**Goal:** Define the memory layout that XDMAC will transfer to PWMC DMAR.

PWMC Sync Mode distributes DMAR writes to synchronized channels in ascending
channel order. For 4 synchronized channels (CH0-CH3), each DMAR write cycle
consumes 4 words: one per channel.

```
DMA buffer layout (17 bit-periods × 4 channels = 68 uint32_t words):

Index 0:  CH0 bit15 duty    (MSB first)
Index 1:  CH1 bit15 duty
Index 2:  CH2 bit15 duty
Index 3:  CH3 bit15 duty
Index 4:  CH0 bit14 duty
Index 5:  CH1 bit14 duty
...
Index 60: CH0 bit0 duty
Index 61: CH1 bit0 duty
Index 62: CH2 bit0 duty
Index 63: CH3 bit0 duty
Index 64: CH0 reset (0)     ← ensures output goes low after frame
Index 65: CH1 reset (0)
Index 66: CH2 reset (0)
Index 67: CH3 reset (0)
```

**CRITICAL:** Channel order in the buffer must match the PWMC hardware channel
numbers (CH0, CH1, CH2, CH3) — NOT the motor index order. The `timer_io_channels[]`
array maps motor index to PWMC channel. The buffer packing must account for this
mapping.

Motor-to-channel mapping (from `timer_config.cpp`):
- Motor 1 (index 0) → CH3
- Motor 2 (index 1) → CH1
- Motor 3 (index 2) → CH2
- Motor 4 (index 3) → CH0

So in the DMA buffer, the channel order is: [CH0=Motor4, CH1=Motor2, CH2=Motor3, CH3=Motor1]

**Data structures:**

```c
#define DSHOT_CHANNELS_PER_TIMER    4
#define DSHOT_BITS_PER_FRAME        16
#define DSHOT_RESET_PERIODS         1
#define DSHOT_TOTAL_PERIODS         (DSHOT_BITS_PER_FRAME + DSHOT_RESET_PERIODS)
#define DSHOT_BUFFER_SIZE           (DSHOT_TOTAL_PERIODS * DSHOT_CHANNELS_PER_TIMER)

/* Per-timer state */
static uint32_t dshot_buffer[MAX_IO_TIMERS][DSHOT_BUFFER_SIZE]
    __attribute__((aligned(4), section(".nocache")));

/* Per-channel packet data (set by dshot_motor_data_set, consumed by trigger) */
static uint16_t dshot_packet[MAX_TIMER_IO_CHANNELS];

/* DMA handle */
static DMA_HANDLE dshot_dma_handle[MAX_IO_TIMERS];

/* State */
static bool dshot_armed = false;
static uint32_t dshot_channel_mask = 0;
static uint32_t dshot_cprd = 0;     /* Period for current DShot speed */
static uint32_t dshot_t1h = 0;      /* Duty for bit=1 */
static uint32_t dshot_t0h = 0;      /* Duty for bit=0 */
static uint32_t dshot_dma_errors[MAX_IO_TIMERS];  /* DMA error counter (ROIS/underrun) */
```

**Note on `.nocache`:** The DMA buffer MUST be in non-cacheable SRAM to avoid
coherency issues. The SAMV71 board already has a `nocache` memory region
(64 KB, see linker script). Use `__attribute__((section(".nocache")))`.

### Step 2: PWMC Sync Mode Configuration

**Goal:** Configure PWMC for synchronous operation with DMA-triggered updates.

Implement inside `io_timer_set_dshot_mode()` (currently a stub):

```c
int io_timer_set_dshot_mode(uint8_t timer, unsigned dshot_pwm_freq)
{
    uint32_t base = io_timers[timer].base;
    uint32_t mck = 150000000UL;

    /* 1. Disable all channels first */
    putreg32(0x0F, base + PWM_DIS_OFFSET);

    /* 2. Calculate timing from DShot frequency
     *    dshot_pwm_freq = DShot bit rate (e.g., 600000 for DShot600)
     *    CPRE = 1 (MCK/2 = 75MHz)
     *    CPRD = 75MHz / dshot_pwm_freq
     */
    uint32_t clk = mck / 2;  /* MCK/2 = 75 MHz */
    uint32_t cprd = clk / dshot_pwm_freq;
    uint32_t t1h = (cprd * 3) / 4;    /* 75% duty for bit=1 */
    uint32_t t0h = (cprd * 3) / 8;    /* 37.5% duty for bit=0 */

    /* Store for buffer packing */
    dshot_cprd = cprd;
    dshot_t1h = t1h;
    dshot_t0h = t0h;

    /* 3. Configure channel 0 (master): CPRE=1 (MCK/2), CPOL=1, CALG=0
     *    In sync mode, channels 1-3 inherit CMR and CPRD from channel 0.
     *    Confirmed by Harmony CSP (pwm_6343): sync channels only get CDTY,
     *    not CMR or CPRD — they inherit from the master channel.
     */
    uint32_t ch0_base = base + 0x200;
    putreg32((1 << 0) | PWM_CMR_CPOL, ch0_base + PWM_CMR_OFFSET);  /* CPRE=1 */
    putreg32(cprd, ch0_base + PWM_CPRD_OFFSET);

    /* Set initial duty to 0 (idle low) for all channels */
    for (int ch = 0; ch < 4; ch++) {
        uint32_t ch_base = base + 0x200 + (ch * 0x20);
        putreg32(0, ch_base + PWM_CDTY_OFFSET);
    }

    /* 4. Configure Sync Channels Mode (SCM)
     *    - SYNCx bits: synchronize CH0-CH3
     *    - UPDM = 2 (Mode 2: auto update at next period boundary)
     *    - PTRM = 0 (DMA request at end of period)
     *    - PTRCS = 0 (comparison trigger, not used, leave 0)
     */
    uint32_t scm = 0;
    scm |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);  /* Sync CH0-CH3 */
    scm |= (2 << 16);  /* UPDM = Mode 2 */
    /* PTRM = 0 (bit 20), PTRCS = 0 (bits 21-23) — default */
    putreg32(scm, base + SAMV7_PWM_SCM);

    /* 5. Enable Sync Channel Update Control */
    putreg32(1, base + SAMV7_PWM_SCUC);  /* UPDULOCK = 1 */

    /* 6. Enable all 4 channels */
    putreg32(0x0F, base + PWM_ENA_OFFSET);

    return OK;
}
```

**Register addresses (from `sam_pwm.h`):**
- SCM: base + 0x0020 (Sync Channels Mode)
- DMAR: base + 0x0024 (DMA Register — target for XDMAC writes)
- SCUC: base + 0x0028 (Sync Channel Update Control)

### Step 3: XDMAC Setup

**Goal:** Allocate and configure a DMA channel for memory-to-PWMC transfer.

```c
static int dshot_dma_init(uint8_t timer)
{
    uint8_t perid = io_timers[timer].dshot.xdmac_ch_tx;  /* 13 for PWM0 */

    /* Allocate DMA channel with peripheral ID */
    uint32_t flags = DMACH_FLAG_PERIPHPID(perid) |
                     DMACH_FLAG_PERIPHISPERIPH |
                     DMACH_FLAG_PERIPHWIDTH_32BITS |
                     DMACH_FLAG_PERIPHCHUNKSIZE_1 |
                     DMACH_FLAG_MEMWIDTH_32BITS |
                     DMACH_FLAG_MEMINCREMENT |
                     DMACH_FLAG_MEMBURST_1;

    dshot_dma_handle[timer] = sam_dmachannel(0, flags);

    if (dshot_dma_handle[timer] == NULL) {
        return -EBUSY;
    }

    return OK;
}
```

### Step 4: Packet Encoding + Buffer Fill

**Goal:** Implement `dshot_motor_data_set()` and the buffer packing in `up_dshot_trigger()`.

```c
/* Called by DShot module to set throttle for a motor */
void dshot_motor_data_set(unsigned channel, uint16_t throttle, bool telemetry)
{
    if (channel >= MAX_TIMER_IO_CHANNELS) return;

    /* Build 16-bit DShot packet: [throttle:11][telemetry:1][crc:4] */
    uint16_t packet = (throttle << 5) | (telemetry ? (1 << 4) : 0);

    /* CRC: XOR of three nibbles */
    uint16_t csum = 0;
    uint16_t cdata = packet >> 4;
    for (int i = 0; i < 3; i++) {
        csum ^= (cdata & 0x0F);
        cdata >>= 4;
    }
    packet |= (csum & 0x0F);

    dshot_packet[channel] = packet;
}

/* Trigger DMA transfer for all motors */
void up_dshot_trigger(void)
{
    if (!dshot_armed) return;

    /* For each timer (we have 1 timer = PWM0) */
    for (uint8_t timer = 0; timer < MAX_IO_TIMERS; timer++) {
        uint32_t *buf = dshot_buffer[timer];

        /* Build interleaved buffer: for each bit period, write 4 channel duties */
        for (int bit = 0; bit < DSHOT_BITS_PER_FRAME; bit++) {
            for (int ch = 0; ch < DSHOT_CHANNELS_PER_TIMER; ch++) {
                /* Find which motor index maps to this PWMC channel */
                int motor = channel_map_hw_to_motor[timer][ch];

                uint32_t duty = 0;
                if (motor >= 0 && (dshot_channel_mask & (1 << motor))) {
                    uint16_t pkt = dshot_packet[motor];
                    /* MSB first: bit 15 is sent first */
                    duty = (pkt & (1 << (15 - bit))) ? dshot_t1h : dshot_t0h;
                }

                buf[bit * DSHOT_CHANNELS_PER_TIMER + ch] = duty;
            }
        }

        /* Reset period: all channels output low */
        for (int ch = 0; ch < DSHOT_CHANNELS_PER_TIMER; ch++) {
            buf[DSHOT_BITS_PER_FRAME * DSHOT_CHANNELS_PER_TIMER + ch] = 0;
        }

        /* Ensure buffer writes are visible to DMA before starting transfer.
         * Harmony CSP (plib_xdmac.c) uses __DMB() here. Even with .nocache
         * buffer, the barrier ensures write ordering from CPU pipeline.
         */
        __DMB();

        /* Start DMA transfer */
        uint32_t dmar_addr = io_timers[timer].base + SAMV7_PWM_DMAR;
        sam_dmatxsetup(dshot_dma_handle[timer],
                       dmar_addr,
                       (uint32_t)buf,
                       DSHOT_BUFFER_SIZE * sizeof(uint32_t));
        sam_dmastart(dshot_dma_handle[timer], dshot_dma_callback, (void*)(uintptr_t)timer);
    }
}
```

**Channel mapping array** (built during `up_dshot_init()`):
```c
/* Maps PWMC hardware channel number → motor index (-1 if unused) */
static int8_t channel_map_hw_to_motor[MAX_IO_TIMERS][4];
```

This is populated by scanning `timer_io_channels[]` during init:
```c
for (int motor = 0; motor < MAX_TIMER_IO_CHANNELS; motor++) {
    uint8_t ti = timer_io_channels[motor].timer_index;
    uint8_t ch = timer_io_channels[motor].timer_channel;
    channel_map_hw_to_motor[ti][ch] = motor;
}
```

### Step 5: DMA Completion Callback

**Goal:** Handle DMA transfer complete, optionally re-arm for next frame.

```c
static void dshot_dma_callback(DMA_HANDLE handle, void *arg, int result)
{
    uint8_t timer = (uint8_t)(uintptr_t)arg;
    (void)handle;

    if (result < 0) {
        /* DMA error (RBEIS/WBEIS/ROIS from XDMAC_CIS).
         * ROIS = request overflow = PWMC DMAR underrun.
         * Log but don't crash — next trigger will retry.
         */
        dshot_dma_errors[timer]++;
    }

    /* Transfer complete — PWMC will hold last duty until next trigger.
     * With reset period at end, all outputs are low (idle state).
     * No re-arm needed; next up_dshot_trigger() call starts a new transfer.
     */
}
```

### Step 6: Init and Arm Functions

```c
int up_dshot_init(uint32_t channel_mask, unsigned dshot_pwm_freq,
                  bool enable_bidirectional_dshot)
{
    (void)enable_bidirectional_dshot;  /* Deferred to Phase 4+ */

    dshot_channel_mask = channel_mask;

    /* Initialize channel mapping */
    memset(channel_map_hw_to_motor, -1, sizeof(channel_map_hw_to_motor));

    for (unsigned ch = 0; ch < MAX_TIMER_IO_CHANNELS; ch++) {
        if (!(channel_mask & (1 << ch))) continue;

        uint8_t ti = timer_io_channels[ch].timer_index;
        uint8_t hwch = timer_io_channels[ch].timer_channel;
        channel_map_hw_to_motor[ti][hwch] = ch;

        /* Configure GPIO for PWMC output */
        io_timer_channel_init(ch, IOTimerChanMode_Dshot, NULL, NULL);
    }

    /* Configure each timer for DShot mode */
    for (uint8_t timer = 0; timer < MAX_IO_TIMERS; timer++) {
        io_timer_set_dshot_mode(timer, dshot_pwm_freq);
        dshot_dma_init(timer);
    }

    /* Clear buffers */
    memset(dshot_buffer, 0, sizeof(dshot_buffer));
    memset(dshot_packet, 0, sizeof(dshot_packet));

    return channel_mask;
}

int up_dshot_arm(bool armed)
{
    dshot_armed = armed;

    for (uint8_t timer = 0; timer < MAX_IO_TIMERS; timer++) {
        uint32_t base = io_timers[timer].base;

        if (armed) {
            putreg32(0x0F, base + PWM_ENA_OFFSET);
        } else {
            putreg32(0x0F, base + PWM_DIS_OFFSET);
            /* Force all outputs low */
            for (int ch = 0; ch < 4; ch++) {
                uint32_t ch_base = base + 0x200 + (ch * 0x20);
                putreg32(0, ch_base + PWM_CDTYUPD_OFFSET);
            }
        }
    }

    return OK;
}
```

### Step 7: Bidirectional Stubs

These return "not supported" for now — architecture reserved in `dshot_conf_t`:

```c
int up_bdshot_num_erpm_ready(void)  { return -ENOSYS; }
int up_bdshot_get_erpm(uint8_t channel, int *erpm) { return -ENOSYS; }
int up_bdshot_channel_status(uint8_t channel) { return -ENOSYS; }
void up_bdshot_status(void) { /* nothing */ }
```

### Step 8: io_timer_pwmc.c Updates

Fill the two stubs and add DShot channel mode:

**a) `io_timer_update_dma_req()`** — Enable/disable DMA trigger from PWMC:

```c
void io_timer_update_dma_req(uint8_t timer, bool enable)
{
    uint32_t base = io_timers[timer].base;

    if (enable) {
        /* Enable WRDY interrupt in IER2 — this triggers XDMAC via hardware request */
        putreg32(IR2_WRDY, base + SAMV7_PWM_IER2);
    } else {
        /* Disable WRDY interrupt */
        putreg32(IR2_WRDY, base + SAMV7_PWM_IDR2);
    }
}
```

**Note:** The XDMAC peripheral request for PWMC is triggered by the WRDY
(Write Ready) flag in ISR2, which fires when DMAR is ready for next value.
In UPDM=2 mode with PTRM=0, this happens at end-of-period.

**b) `IOTimerChanMode_Dshot`** — Add to `io_timer_channel_init()` switch:

```c
case IOTimerChanMode_Dshot:
case IOTimerChanMode_DshotInverted: {
    uint32_t gpio = timer_io_channels[channel].gpio_out;
    sam_configgpio(gpio);
    /* Actual PWMC config done in io_timer_set_dshot_mode() */
    break;
}
```

### Step 9: Build Integration

**a) New `CMakeLists.txt`:**

```cmake
# platforms/nuttx/src/px4/microchip/samv7/dshot/CMakeLists.txt
px4_add_library(arch_dshot
    dshot.c
)

target_link_libraries(arch_dshot
    PRIVATE
        nuttx_arch
        nuttx_drivers
        arch_io_pins
)
```

**b) Parent CMakeLists.txt update:**

In `platforms/nuttx/src/px4/microchip/samv7/CMakeLists.txt`, add:

```cmake
if(CONFIG_DRIVERS_DSHOT)
    add_subdirectory(dshot)
endif()
```

**c) Board config:**

In `boards/microchip/samv71-xult-clickboards/default.px4board`:

```
CONFIG_DRIVERS_DSHOT=y
```

---

## 4. PWMC Sync Mode — Register Sequence Detail

This is the exact register-level sequence for DShot operation:

```
INITIALIZATION:
  1. PWM_DIS  = 0x0F              — Disable all channels
  2. CMR[0-3] = CPRE=1 | CPOL=1  — MCK/2, output high at period start
  3. CPRD[0-3] = cprd             — Same period for all channels
  4. CDTY[0-3] = 0                — Start with output low (idle)
  5. SCM = SYNC0|SYNC1|SYNC2|SYNC3 | UPDM_MODE2  — Sync all, auto-update
  6. SCUC = UPDULOCK              — Unlock update
  7. PWM_ENA = 0x0F               — Enable all channels

EACH DSHOT FRAME (up_dshot_trigger):
  1. Pack 16-bit packets for each motor
  2. Fill DMA buffer: 17 periods × 4 channels = 68 words
  3. Configure XDMAC: source=buffer, dest=DMAR, count=68 words
  4. Start XDMAC transfer
  5. PWMC hardware triggers XDMAC at each period end
  6. XDMAC writes 4 words (one per sync channel) to DMAR per period
  7. After 17 periods (16 bits + 1 reset), XDMAC transfer complete
  8. Channels hold last duty (0 from reset period) → output low until next trigger
```

---

## 5. Risk Analysis & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| DMA buffer in cached SRAM | Corrupted duty values | Use `.nocache` section attribute |
| XDMAC PERID mismatch | No DMA triggers | Verify PERID=13 matches PWM0 TX in datasheet Table 47-1 |
| SCM channel sync order | Wrong motors get wrong duties | Build explicit hw_channel→motor mapping table |
| DMAR write timing | Underrun (UNRE flag) | Check ISR2.UNRE, log errors |
| DShot ESC compatibility | ESCs don't respond | Test with actual ESCs on scope, verify timing |
| Prescaler precision | Bit timing out of spec | MCK/2=75MHz gives exact integer periods for DShot150/300/600 |
| DMA transfer not completing | Stuck outputs | DMA completion callback with timeout watchdog |
| PWM↔DShot mode switch | Glitch on outputs | Disable channels, reconfigure, re-enable |

---

## 6. Testing Plan

### 6.1 Scope Verification (No ESCs)

1. Build with `CONFIG_DRIVERS_DSHOT=y`
2. Boot, run `dshot start` from NSH
3. Run `actuator_test set -m 1 -v 0.1` to set low throttle on Motor 1
4. Measure on oscilloscope:
   - **Period:** Should match DShot speed (e.g., 1.67 µs for DShot600)
   - **Bit-1 high time:** ~75% of period
   - **Bit-0 high time:** ~37.5% of period
   - **Frame:** 16 bit periods + reset (low)
   - **All 4 channels:** Verify independent duty patterns

### 6.2 ESC Smoke Test

1. Connect DShot-capable ESC to Motor 1 output
2. Power ESC from separate supply (NOT through flight controller)
3. `dshot start`
4. `up_dshot_arm(true)` via safety button
5. `actuator_test set -m 1 -v 0.05` — ESC should beep/respond
6. Increase to `0.1` — motor should spin slowly
7. `actuator_test set -m 1 -v 0.0` — motor stop

### 6.3 Multi-Motor Test

1. Connect 4 ESCs
2. `actuator_test set -m 1 -v 0.05` through `-m 4 -v 0.05`
3. Verify each motor spins independently
4. Verify no crosstalk (setting Motor 1 doesn't affect Motor 2)

### 6.4 DShot Command Test

1. `dshot reverse -m 1` — verify ESC responds to direction command
2. `dshot beep -m 1` — verify ESC beeps

---

## 7. Implementation Order

| # | Task | Files | Depends On | Verification |
|---|------|-------|------------|--------------|
| 1 | Create `dshot.c` with stubs for all `drv_dshot.h` functions | `dshot/dshot.c` | — | Builds, links |
| 2 | Create `CMakeLists.txt`, add to parent CMake | `dshot/CMakeLists.txt`, parent CMake | #1 | Builds with `CONFIG_DRIVERS_DSHOT=y` |
| 3 | Implement `up_dshot_init()` — channel mapping + GPIO config | `dshot.c` | #2 | `dshot start` doesn't crash |
| 4 | Implement `io_timer_set_dshot_mode()` — PWMC sync mode config | `io_timer_pwmc.c` | #3 | Scope shows synchronized periods on all 4 pins |
| 5 | Implement XDMAC allocation (`dshot_dma_init()`) | `dshot.c` | #4 | DMA handle acquired, no EBUSY |
| 6 | Implement `dshot_motor_data_set()` — packet encoding | `dshot.c` | #3 | Unit test: verify CRC for known values |
| 7 | Implement `up_dshot_trigger()` — buffer fill + DMA start | `dshot.c` | #5, #6 | Scope shows DShot waveform |
| 8 | Implement `io_timer_update_dma_req()` — WRDY enable | `io_timer_pwmc.c` | #7 | DMA transfers continuously triggered |
| 9 | Implement `up_dshot_arm()` — enable/disable | `dshot.c` | #7 | Safety button gates output |
| 10 | ESC integration test | — | #9 | Motor spins with `actuator_test` |
| 11 | Enable in board config, test with `pwm_out` DShot mode | `default.px4board` | #10 | Full flight stack DShot output |

---

## 8. Key Register Reference (Quick Lookup)

### PWMC (base = 0x40020000 for PWM0)

| Register | Offset | Purpose |
|----------|--------|---------|
| CLK | 0x0000 | Clock configuration (DIVA/DIVB — not used, we use CPRE) |
| ENA | 0x0004 | Channel enable |
| DIS | 0x0008 | Channel disable |
| SR | 0x000C | Status (which channels enabled) |
| IER2 | 0x0034 | Interrupt Enable Register 2 (WRDY) |
| IDR2 | 0x0038 | Interrupt Disable Register 2 |
| ISR2 | 0x0040 | Interrupt Status Register 2 (WRDY, UNRE) |
| SCM | 0x0020 | Sync Channels Mode |
| DMAR | 0x0024 | DMA Register (XDMAC writes here) |
| SCUC | 0x0028 | Sync Channel Update Control |
| CMR[n] | 0x200 + n×0x20 | Channel Mode (CPRE, CPOL, CALG) |
| CDTY[n] | 0x204 + n×0x20 | Channel Duty Cycle |
| CDTYUPD[n] | 0x208 + n×0x20 | Channel Duty Update (glitch-free) |
| CPRD[n] | 0x20C + n×0x20 | Channel Period |

### XDMAC

| Item | Value |
|------|-------|
| PWM0 TX PERID | 13 |
| PWM1 TX PERID | 39 |
| Transfer width | 32 bits (word) |
| Direction | Memory → Peripheral |
| Source increment | Yes (memory buffer) |
| Dest increment | No (fixed DMAR address) |
| Chunk size | 1 (single transfer per trigger) |

---

## 9. DShot Protocol Quick Reference

### Packet Format (16 bits, MSB first)

```
[15:5] Throttle (11 bits): 0-47 = commands, 48-2047 = throttle
[4]    Telemetry request (1 bit)
[3:0]  CRC (4 bits): XOR of three nibbles of bits [15:4]
```

### CRC Calculation

```c
uint16_t crc = 0;
uint16_t data = packet >> 4;   // bits [15:4]
crc ^= (data & 0xF); data >>= 4;  // nibble 0
crc ^= (data & 0xF); data >>= 4;  // nibble 1
crc ^= (data & 0xF);               // nibble 2
// For bidirectional: packet |= (~crc & 0xF)
// For unidirectional: packet |= (crc & 0xF)
```

---

## 10. Open Questions for Codex Review

1. **DMAR write granularity:** Does PWMC consume exactly N_sync_channels words
   per DMA request, or does it consume one word per request? The datasheet says
   "one word per update" but Sync Mode may batch. Need hardware verification.

2. **SCUC.UPDULOCK timing:** In UPDM=2, is UPDULOCK auto-set or must we write
   SCUC=1 before each DMA sequence? Datasheet says "written automatically" in
   Mode 2 — confirm.

3. **XDMAC chunk vs block:** Should we use single-block transfer (68 words) or
   linked-list descriptors? Single block is simpler for unidirectional DShot.

4. **DMA-to-DMAR vs DMA-to-CDTYUPD:** Alternative approach: instead of using
   SCM DMAR distribution, DMA could write directly to CDTYUPD[n] registers.
   This would require 4 separate DMA channels (one per motor). DMAR approach
   uses only 1 DMA channel — preferred.

5. **CPOL inversion:** With CPOL=1, CDTY=high means the output is HIGH for CDTY
   ticks and LOW for (CPRD-CDTY) ticks. Verify this matches DShot ESC expectations
   (bit-1 = long high pulse, bit-0 = short high pulse).

---

## 11. Harmony CSP Cross-Reference

The Microchip Harmony CSP (Chip Support Package) at `/media/bhanu1234/Development/csp` was analyzed
for reference code. The relevant peripherals are:

### 11.1 SAMV7 PWMC — `csp/peripheral/pwm_6343/`

**Files:**
- `templates/plib_pwm.c.ftl` — PWMC implementation (Freemarker template)
- `templates/plib_pwm.h.ftl` — PWMC header with inline `PWM_ChannelDutySet()`
- `config/pwm.py` — Config generator with all register field options

**Key Confirmations for DShot:**

1. **Sync Channel Mode Init** (plib_pwm.c.ftl, line 177):
   ```c
   PWM_REGS->PWM_SCM = PWM_SCM_SYNC0_Msk | PWM_SCM_SYNC1_Msk | ... | PWM_SCM_UPDM_MODE2;
   PWM_REGS->PWM_SCUP = PWM_SCUP_UPR(0);
   ```
   This confirms: SCM register with SYNCx bits + UPDM field. UPDM=2 is "Mode 2" (DMAR auto-update).

2. **Sync channels inherit CMR and CPRD from channel 0** (plib_pwm.c.ftl, lines 201-214):
   The template conditionally writes CMR and CPRD ONLY for non-sync channels:
   ```ftl
   <#if .vars[PWM_CH_SYNC_ENABLE] == false>
       // CMR and CPRD configured here
   </#if>
   // CDTY always configured for all channels
   ```
   **Impact:** Our Step 2 was corrected — configure CMR and CPRD only on channel 0,
   set CDTY for all 4 channels. Sync channels 1-3 inherit prescaler and period from CH0.

3. **SyncUpdateEnable** (plib_pwm.c.ftl, line 327):
   ```c
   PWM_REGS->PWM_SCUC = PWM_SCUC_UPDULOCK_Msk;
   ```
   Confirmed: UPDULOCK bit in SCUC register controls sync update locking.

4. **Duty via CDTYUPD** (plib_pwm.h.ftl, line 99):
   ```c
   __STATIC_INLINE void PWM_ChannelDutySet(PWM_CHANNEL_NUM channel, uint16_t duty) {
       PWM_REGS->PWM_CH_NUM[channel].PWM_CDTYUPD = duty;
   }
   ```
   Confirmed: Always use CDTYUPD (not CDTY) for glitch-free updates during operation.

5. **UPDM Configuration Options** (pwm.py):
   - `UPDM = 0`: Manual (SCUC.UPDULOCK triggers update)
   - `UPDM = 1`: Automatic at period boundary (UPR controls update period)
   - `UPDM = 2`: DMAR mode (XDMAC writes to DMAR trigger updates) ← **DShot uses this**

6. **Fault protection** — Harmony has full FPE/FPV1/FPV2/FMR configuration for fault
   pins. Phase 4 (production hardening) should add fault protection using these patterns.

### 11.2 XDMAC — `csp/peripheral/xdmac_11161/`

**Files:**
- `templates/plib_xdmac.c.ftl` — XDMAC implementation
- `templates/plib_xdmac.h.ftl` — XDMAC API
- `templates/plib_xdmac_common.h.ftl` — Data types, descriptor views, callbacks

**Key Confirmations for DShot:**

1. **DMA Channel Transfer** (plib_xdmac.c.ftl, lines 280-315):
   ```c
   // Transfer setup sequence (Harmony reference pattern):
   status = XDMAC_REGS->XDMAC_CHID[ch].XDMAC_CIS;  // Clear status by reading
   XDMAC_REGS->XDMAC_CHID[ch].XDMAC_CSA = (uint32_t)srcAddr;   // Source (RAM buffer)
   XDMAC_REGS->XDMAC_CHID[ch].XDMAC_CDA = (uint32_t)destAddr;  // Dest (PWMC DMAR)
   XDMAC_REGS->XDMAC_CHID[ch].XDMAC_CUBC = XDMAC_CUBC_UBLEN(blockSize);  // Count
   __DMB();                                                        // Memory barrier
   XDMAC_REGS->XDMAC_GE = (XDMAC_GE_EN0_Msk << ch);            // Enable channel
   ```
   **Note:** `__DMB()` before enable is critical for cache coherency. Our NuttX implementation
   uses `.nocache` buffer section, but the `__DMB()` should still be issued.

2. **Channel Configuration Register (CC)** (plib_xdmac.c.ftl, lines 220-238):
   For peripheral-triggered (MEM→PER) transfers:
   ```c
   XDMAC_CC = XDMAC_CC_TYPE_PER_TRAN       // Peripheral transfer (not memory-to-memory)
            | XDMAC_CC_PERID(13)             // PWM0 TX = PERID 13
            | XDMAC_CC_DSYNC_MEM2PER         // Memory → Peripheral direction
            | XDMAC_CC_SWREQ_HWR_CONNECTED   // Hardware request trigger (not software)
            | XDMAC_CC_DAM_FIXED_AM          // Dest fixed (write to same DMAR addr)
            | XDMAC_CC_SAM_INCREMENTED_AM    // Source incremented (scan through buffer)
            | XDMAC_CC_DWIDTH_WORD           // 32-bit word transfers
            | XDMAC_CC_CSIZE_CHK_1           // 1 transfer per hardware request
            | XDMAC_CC_MBSIZE_SINGLE;        // Single memory burst
   ```
   This maps directly to our `dshot_dma_init()` DMACH_FLAG configuration.

3. **Linked List Descriptors** (plib_xdmac_common.h.ftl, lines 222-340):
   Four descriptor views available:
   - **View 0**: NDA + UBC + DA (12 bytes, smallest)
   - **View 1**: NDA + UBC + SA + DA (16 bytes)
   - **View 2**: NDA + UBC + SA + DA + CFG (20 bytes, can change CC per descriptor)
   - **View 3**: Full (36 bytes, includes stride)

   For DShot, linked list is NOT needed initially (single-block transfer is sufficient).
   If we later need continuous frame output, View 0 descriptors with circular chain could
   auto-repeat the DMA transfer without CPU intervention.

4. **Completion Detection** (plib_xdmac.c.ftl, lines 85-133):
   - `XDMAC_CIS.BIS` = Block transfer complete (normal completion)
   - `XDMAC_CIS.RBEIS` = Read bus error
   - `XDMAC_CIS.WBEIS` = Write bus error
   - `XDMAC_CIS.ROIS` = Request overflow (underrun indicator)
   Our DMA callback should check for ROIS (overflow) — this maps to PWMC DMAR underrun.

5. **Channel Disable** (plib_xdmac.c.ftl, line 382):
   ```c
   XDMAC_REGS->XDMAC_GD = (XDMAC_GD_DI0_Msk << channel);
   ```
   Use `XDMAC_GD` to disable channel (not just clear enable).

### 11.3 Corrections Applied from CSP Review

| Item | Before CSP Review | After CSP Review | Impact |
|------|-------------------|------------------|--------|
| CMR config | All 4 channels configured | Only CH0 (master) | Correct — sync inherits |
| CPRD config | All 4 channels set | Only CH0 | Correct — sync inherits |
| CDTY init | Set in same loop as CMR | Separate loop, all 4 | Clarified intent |
| `__DMB()` | Not mentioned | Added per Harmony pattern | Cache safety |
| ROIS check | Not in plan | Added to DMA callback | Underrun detection |

### 11.4 Other CSP Peripherals Found (Not Used for DShot)

| Directory | Peripheral | Notes |
|-----------|-----------|-------|
| `pwm_04302` | dsPIC/PIC32 PWM | Different architecture (PGxCON), not applicable |
| `pwm_54` | Simpler PWM | Not SAMV7 |
| `pwm_6044` | Another PWM variant | Not SAMV7 |
| `mcpwm_01477` | Motor Control PWM | For dsPIC33/PIC24 |

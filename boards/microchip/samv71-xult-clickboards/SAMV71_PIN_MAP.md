# SAMV71-XULT PX4 Complete Pin Map

**Created:** January 2026
**Status:** DRAFT - For Review
**Purpose:** Pin assignment for minimum viable PX4 quadcopter on SAMV71-XULT

---

## Assumptions & Out of Scope

### Out of Scope (Not Used)
| Feature | Reason | Pins Freed |
|---------|--------|------------|
| **maXTouch LCD** | Not needed for flight controller | PC13, PB3, PA2, PC19 |
| **mikroBUS2 Socket** | Only mikroBUS1 used for sensors | PB0 (RST), PA6 (INT) |
| **UART0** | PA9 used for Safety Button (conflicts with UART0 RX on PA9) | PB0 (alt RX on J505) |
| **Audio Codec** | No audio in flight controller | PB0 (LRCLK) |
| **Camera Interface** | Not implemented | - |
| **Ethernet** | Not needed for quadcopter | - |
| **CAN Bus** | Future expansion only | - |

**Note:** PB0 has multiple alternate functions (mikroBUS2 RST, UART0 alt RX on J505, Audio LRCLK). All are out of scope, freeing PB0 for Motor 4 PWM.

### Design Assumptions
1. SD card is always inserted (no hardware card detect)
2. LCD pins (PB3, PA2, PC19) repurposed for sensors/PWM
3. mikroBUS2 pins repurposed: PB0 used for Motor 4 PWM, PA6 free
4. Single I2C bus sufficient for all sensors

---

## Table of Contents
1. [Pin Conflict Summary](#1-pin-conflict-summary)
2. [UART Configuration](#2-uart-configuration)
3. [SPI Configuration](#3-spi-configuration)
4. [I2C Configuration](#4-i2c-configuration)
5. [PWM Configuration (PWMC-based)](#5-pwm-configuration-pwmc-based)
6. [ADC Configuration](#6-adc-configuration)
7. [Safety IO](#7-safety-io)
8. [SD Card (HSMCI)](#8-sd-card-hsmci)
9. [QSPI Flash (Parameter Storage)](#9-qspi-flash-parameter-storage)
10. [LEDs](#10-leds)
11. [USB](#11-usb)
12. [Complete Pin Usage Table](#12-complete-pin-usage-table)
13. [Conflict Resolution](#13-conflict-resolution)

---

## 1. Pin Conflict Summary

### Original Conflicts (MUST FIX)

| Pin | Conflict 1 | Conflict 2 | Resolution |
|-----|------------|------------|------------|
| **PD25** | ICM20689 CS (SPI) | UART2 RX (GPS) | Move ICM20689 CS to **PB3** (rewire) |
| **PD18** | SD Card Detect (HW) | UART4 RX (RC) | **Disable CD** - assume card present |
| **PA14** | PWM (Motor 4) | QSPI_SCK (Flash) | Move Motor 4 - see [Section 5 Options](#5-pwm-configuration---three-options) |

### Resolved Pin Assignments (Using Option A)

| Function | Original Pin | New Pin | Status |
|----------|--------------|---------|--------|
| GPS UART2 RX | - | PD25 | Fixed (hardware) |
| GPS UART2 TX | - | PD26 | Fixed (hardware) |
| RC UART4 RX | - | PD18 | Fixed (hardware) |
| ICM20689 CS | PD25 | **PB3** | Requires rewiring |
| SD Card Detect | PD18 | **Disabled** | True HW conflict - assume card present |
| Motor 4 PWM | PA14 | **PB0** | PWM0_CH0 on EXT1 pin 13 (Option A) |
| mikroBUS2 RST | PB0 | **Removed** | PB0 now used for Motor 4 |

### Motor 4 Pin Selection

Three options are available for Motor 4 (see Section 5 for details):

| Option | Pin | Channel | Pros | Cons |
|--------|-----|---------|------|------|
| **A (Recommended)** | PB0 | PWM0_CH0 | PA0 free, all PWM0, 4 channels | Loses MB2 RST |
| B | PA0 | PWM0_CH0 | All PWM0, 4 channels | Loses mikroBUS1 INT |
| C | PC29 | TC5 | PA0 free, max flexibility | No DShot, jumper required |

### Why Certain Pins Cannot Be Used

| Pin | Reason |
|-----|--------|
| **PD7** | Not routed to any header on SAMV71-XULT |
| **PA14** | QSPI_SCK - required for parameter flash |
| **PA26** | HSMCI0 DA2 - required for SD card |

---

## 2. UART Configuration

### Available UARTs on SAMV71

| UART | RX Pin | TX Pin | Peripheral | PX4 Function |
|------|--------|--------|------------|--------------|
| UART0 | PA9 | PA10 | Peripheral A | *Available* (conflicts with Safety Button) |
| UART2 | **PD25** | **PD26** | Peripheral C | **GPS** |
| UART4 | **PD18** | PD19 | Peripheral C | **RC Input (SBUS)** |
| USART1 | PA21 | PB4 | Peripheral A/D | **Debug Console** |

### UART Pin Details

```
UART2 (GPS):
├── RX: PD25 (Peripheral C) - J505 connector
├── TX: PD26 (Peripheral C) - J505 connector
├── Baud: 38400 (default), configurable
└── Protocol: NMEA/UBX

UART4 (RC/SBUS):
├── RX: PD18 (Peripheral C)
├── TX: PD19 (Peripheral C) - not used for SBUS
├── Baud: 100000
├── Data bits: 8
├── Parity: Even (2)
├── Stop bits: 2
└── Protocol: SBUS (inverted)

USART1 (Debug Console):
├── RX: PA21 (Peripheral A)
├── TX: PB4 (Peripheral D)
└── Baud: 115200
```

### defconfig UART Settings

```
# GPS - UART2
CONFIG_SAMV7_UART2=y
CONFIG_UART2_BAUD=38400
CONFIG_UART2_BITS=8
CONFIG_UART2_PARITY=0
CONFIG_UART2_2STOP=0

# RC/SBUS - UART4
CONFIG_SAMV7_UART4=y
CONFIG_UART4_BAUD=100000
CONFIG_UART4_BITS=8
CONFIG_UART4_PARITY=2
CONFIG_UART4_2STOP=1

# Debug Console - USART1
CONFIG_SAMV7_USART1=y
CONFIG_USART1_BAUD=115200
CONFIG_USART1_SERIAL_CONSOLE=y
```

---

## 3. SPI Configuration

### SPI0 Bus (Sensors)

| Signal | Pin | Peripheral |
|--------|-----|------------|
| MISO | PD20 | Peripheral B |
| MOSI | PD21 | Peripheral B |
| SCK | PD22 | Peripheral B |

### SPI Chip Selects (GPIO-based)

| Device | Original CS | New CS | Location | Notes |
|--------|-------------|--------|----------|-------|
| ICM20689 | PD25 ❌ | **PB3** | EXT1 header | Requires rewiring |
| BMP388 | PD27 | PD27 | EXT2 header | No change |
| ICM45686 | TBD | TBD | mikroBUS | If used |
| BMI088 | TBD | TBD | mikroBUS | If used |

### SPI GPIO Definitions

```c
/* SPI0 Bus Signals (fixed) */
// MISO: PD20, MOSI: PD21, SCK: PD22 - configured by NuttX

/* SPI Chip Selects (GPIO) */
#define GPIO_SPI0_CS_ICM20689    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOB|GPIO_PIN3)   /* PB3 - NEW */
#define GPIO_SPI0_DRDY_ICM20689  (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_INT_FALLING|GPIO_PORT_PIOD|GPIO_PIN28)
#define GPIO_SPI0_DRDY_ICM20689_IRQ  SAM_IRQ_PD28

#define GPIO_SPI0_CS_BMP388      (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN27)  /* PD27 - unchanged */
```

---

## 4. I2C Configuration

### TWIHS0 (I2C0) - Primary Sensor Bus

| Signal | Pin | Peripheral |
|--------|-----|------------|
| SDA | PA3 | Peripheral A |
| SCL | PA4 | Peripheral A |

### I2C Sensor Details (Click Boards)

| Sensor | Click Board | I2C Address | Function | PX4 Driver |
|--------|-------------|-------------|----------|------------|
| **AK09916** | Compass 4 Click | 0x0C | Magnetometer | `CONFIG_DRIVERS_MAGNETOMETER_AKM_AK09916` |
| **BMM150** | GeoMagnetic Click | 0x10 (SDO=GND) / 0x13 (SDO=VDD) | Magnetometer | `CONFIG_DRIVERS_MAGNETOMETER_BOSCH_BMM150` |
| **DPS310** | Pressure 3 Click | 0x76 (SDO=GND) / 0x77 (SDO=VDD) | Barometer | `CONFIG_DRIVERS_BAROMETER_DPS310` |
| **BMP388** | Pressure 19 Click | 0x76 (SDO=GND) / 0x77 (SDO=VDD) | Barometer | `CONFIG_DRIVERS_BAROMETER_BMP388` |
| **BMI088 Accel** | IMU 13 Click | 0x18 (SDO=GND) / 0x19 (SDO=VDD) | Accelerometer | `CONFIG_DRIVERS_IMU_BOSCH_BMI088_I2C` |
| **BMI088 Gyro** | IMU 13 Click | 0x68 (SDO=GND) / 0x69 (SDO=VDD) | Gyroscope | `CONFIG_DRIVERS_IMU_BOSCH_BMI088_I2C` |

### Minimum Viable Sensor Set (I2C)

For a basic quadcopter, you need at minimum:

| Required | Sensor | Click Board | Address |
|----------|--------|-------------|---------|
| ✓ IMU | BMI088 | IMU 13 Click | 0x18 (Accel), 0x68 (Gyro) |
| ✓ Magnetometer | AK09916 | Compass 4 Click | 0x0C |
| ✓ Barometer | DPS310 | Pressure 3 Click | 0x77 |

### I2C Bus Scan Expected Output

```
nsh> i2cdetect -b 0
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- 0c -- -- --   <- AK09916 Magnetometer
10: -- -- -- -- -- -- -- -- 18 -- -- -- -- -- -- --   <- BMI088 Accelerometer
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- --   <- BMI088 Gyroscope
70: -- -- -- -- -- -- -- 77                           <- DPS310 Barometer
```

---

## 5. PWM Configuration - Three Options

This section presents **three options** for Motor 4 pin assignment. Motors 1-3 are the same across all options.

### Common Pin Assignments (All Options)

| Motor | Pin | PWMC | Channel | Peripheral | Location |
|-------|-----|------|---------|------------|----------|
| 1 | **PA7** | PWM0 | CH3 | Peripheral B | Arduino A1 |
| 2 | **PA2** | PWM0 | CH1 | Peripheral A | EXT2 pin 9 |
| 3 | **PC19** | PWM0 | CH2 | Peripheral B | EXT2 pin 7 |
| 4 | **See Options Below** | | | | |

### Rejected Pins for Motor 4

| Pin | Reason | Status |
|-----|--------|--------|
| **PA14** | QSPI_SCK conflict | ❌ Rejected |
| **PD7** | Not routed to any header on SAMV71-XULT | ❌ Rejected |
| **PA26** | HSMCI0 DA2 (SD card data line 2) | ❌ Rejected |

---

## Option A: PB0 for Motor 4 (RECOMMENDED)

### Overview
Use **PB0 (PWM0_H0)** on EXT1 pin 13 for Motor 4. Keeps PA0 free and all 4 motors on **different PWM0 channels**.

**IMPORTANT:**
- PA8 is NOT on any accessible header (Arduino D10 is PE2)
- PC13 is PWM0_CH3 - same channel as PA7 (Motor 1) - CANNOT use both independently
- Each motor MUST be on a different PWMC channel for independent control

### Motor 4 Assignment

| Motor | Pin | PWMC | Channel | Output | Peripheral | Location |
|-------|-----|------|---------|--------|------------|----------|
| 4 | **PB0** | PWM0 | **CH0** | H (High) | Peripheral A | EXT1 pin 13 |

### Complete PWM Table (Option A)

| Motor | Pin | PWMC | Channel | Peripheral | Location |
|-------|-----|------|---------|------------|----------|
| 1 | PA7 | PWM0 | **CH3** | Peripheral B | Arduino A1 |
| 2 | PA2 | PWM0 | **CH1** | Peripheral A | EXT2 pin 9 |
| 3 | PC19 | PWM0 | **CH2** | Peripheral B | EXT2 pin 7 |
| 4 | **PB0** | PWM0 | **CH0** | Peripheral A | **EXT1 pin 13** |

**All 4 channels used:** CH0, CH1, CH2, CH3 - full independent control!

### GPIO Definitions (Option A)

```c
/* PWMC-based PWM outputs - Option A (PB0 for Motor 4, all on PWM0, 4 different channels) */
#define GPIO_PWM0_H0_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOB | GPIO_PIN0)   /* Motor 4 - CH0 */
#define GPIO_PWM0_H1_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)   /* Motor 2 - CH1 */
#define GPIO_PWM0_H2_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)  /* Motor 3 - CH2 */
#define GPIO_PWM0_H3_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)   /* Motor 1 - CH3 */
```

### Advantages
| Advantage | Description |
|-----------|-------------|
| ✓ **PA0 remains free** | EXT1 pin 7 / mikroBUS1 INT available for sensor interrupts |
| ✓ **All motors on PWM0** | Single peripheral for DShot sync |
| ✓ **4 independent channels** | CH0, CH1, CH2, CH3 - full motor control |
| ✓ **EXT1 pin 13 accessible** | Header-accessible, easy wiring |

### Disadvantages
| Disadvantage | Description |
|--------------|-------------|
| ✗ **Loses PB0** | mikroBUS2 RST no longer available (MB2 out of scope) |
| ✗ **Loses UART0 RX** | PB0 is also UART0 RX (UART0 out of scope - conflicts with Safety Button PA9) |
| ✗ **Loses Audio LRCLK** | PB0 is audio codec LRCLK (audio out of scope) |

### Why PB0 is Available
1. **mikroBUS2 out of scope** - Only using mikroBUS1 for sensors (MB2 RST not needed)
2. **UART0 out of scope** - PA9 is Safety Button, conflicts with UART0 RX
3. **Audio codec out of scope** - No audio functionality in flight controller
4. **AFEC0_AD10 not needed** - Only using AD0 (voltage) and AD7 (current) for battery

### When to Choose Option A
- You need PA0 free for sensor interrupts (ICM20689 DRDY, etc.)
- You want all 4 motors independently controllable
- You want DShot support with synchronized updates
- mikroBUS2 is not needed

---

## Option B: PA0 for Motor 4 (All Motors on PWM0)

### Overview
Use **PA0 (PWM0_CH0)** for Motor 4. All 4 motors on single PWM0 peripheral for synchronized DShot.

### Motor 4 Assignment

| Motor | Pin | PWMC | Channel | Output | Peripheral | Location |
|-------|-----|------|---------|--------|------------|----------|
| 4 | **PA0** | PWM0 | CH0 | H (High) | Peripheral A | EXT1 pin 7 |

### Complete PWM Table (Option B)

| Motor | Pin | PWMC | Channel | Peripheral | Location |
|-------|-----|------|---------|------------|----------|
| 1 | PA7 | PWM0 | CH3 | Peripheral B | Arduino A1 |
| 2 | PA2 | PWM0 | CH1 | Peripheral A | EXT2 pin 9 |
| 3 | PC19 | PWM0 | CH2 | Peripheral B | EXT2 pin 7 |
| 4 | **PA0** | **PWM0** | CH0 | Peripheral A | **EXT1 pin 7** |

### GPIO Definitions (Option B)

```c
/* PWMC-based PWM outputs - Option B (PA0 for Motor 4, all on PWM0) */
#define GPIO_PWM0_CH0_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN0)   /* Motor 4 */
#define GPIO_PWM0_CH1_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)   /* Motor 2 */
#define GPIO_PWM0_CH2_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)  /* Motor 3 */
#define GPIO_PWM0_CH3_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)   /* Motor 1 */
```

### Advantages
| Advantage | Description |
|-----------|-------------|
| ✓ **Unified PWM0** | All 4 motors on single peripheral |
| ✓ **DShot sync easy** | Single update register for all channels |
| ✓ **Simpler driver** | One PWM peripheral to manage |

### Disadvantages
| Disadvantage | Description |
|--------------|-------------|
| ✗ **Loses PA0** | EXT1 pin 7 / mikroBUS1 INT no longer available |
| ✗ **EXT1 pin 7 used** | Can't use PA0 for sensor interrupts |

### When to Choose Option B
- You're implementing DShot protocol (synchronized updates critical)
- You're not using mikroBUS1 interrupt-based sensors
- Simplicity of single PWM peripheral is important

---

## Option C: TC-Based PWM (Timer/Counter)

### Overview
Use **Timer/Counter (TC)** peripheral instead of PWMC. This was the original approach before DShot consideration.

### Motor 4 Assignment

| Motor | Pin | TC | Channel | Output | Peripheral | Location |
|-------|-----|-----|---------|--------|------------|----------|
| 4 | **PC29** | TC1 | CH2 (TC5) | TIOA5 | Peripheral B | **Not on header - requires jumper** |

### Complete PWM Table (Option C)

| Motor | Pin | Timer | Channel | Output | Peripheral | Location |
|-------|-----|-------|---------|--------|------------|----------|
| 1 | PA15 | TC0 | CH1 (TC1) | TIOA1 | Peripheral B | Arduino D14 |
| 2 | PC23 | TC1 | CH0 (TC3) | TIOA3 | Peripheral B | Check schematic |
| 3 | PC26 | TC1 | CH1 (TC4) | TIOA4 | Peripheral B | Check schematic |
| 4 | **PC29** | TC1 | CH2 (TC5) | TIOA5 | Peripheral B | **Not on header - jumper required** |

**Note:** TC0 CH0 reserved for HRT (High Resolution Timer).

### GPIO Definitions (Option C)

```c
/* TC-based PWM outputs - Option C */
#define GPIO_TC1_TIOA    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN15)  /* Motor 1 - TC1 */
#define GPIO_TC3_TIOA    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN23)  /* Motor 2 - TC3 */
#define GPIO_TC4_TIOA    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN26)  /* Motor 3 - TC4 */
#define GPIO_TC5_TIOA    (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN29)  /* Motor 4 - TC5 */
```

### Advantages
| Advantage | Description |
|-----------|-------------|
| ✓ **PA0 remains free** | mikroBUS1 INT available |
| ✓ **PA8 remains free** | Arduino D10 available for other uses |
| ✓ **Independent timers** | Each motor has dedicated TC channel |
| ✓ **Existing PX4 support** | TC-based PWM well supported in PX4 |

### Disadvantages
| Disadvantage | Description |
|--------------|-------------|
| ✗ **No DShot support** | TC cannot do DShot protocol |
| ✗ **Multiple TC peripherals** | TC0 + TC1 used |
| ✗ **PC29 location** | May require jumper wires |
| ✗ **HRT conflict risk** | Must carefully avoid TC0 CH0 |

### When to Choose Option C
- You only need standard PWM (400Hz/490Hz), no DShot
- You want to preserve both PA0 and PA8
- You have existing TC-based PWM driver code

---

## Decision Matrix

| Criterion | Option A (PB0) | Option B (PA0) | Option C (TC) |
|-----------|----------------|----------------|---------------|
| **DShot Support** | ✓ Best (all PWM0, 4 channels) | ✓ Best (all PWM0, 4 channels) | ❌ No |
| **Standard PWM** | ✓ Yes | ✓ Yes | ✓ Yes |
| **PA0 (EXT1 pin 7)** | ✓ Free | ❌ Used (Motor 4) | ✓ Free |
| **PB0 (EXT1 pin 13)** | ❌ Used (Motor 4) | ✓ Free | ✓ Free |
| **Independent Channels** | ✓ 4 channels (CH0-CH3) | ✓ 4 channels (CH0-CH3) | ✓ 4 timers |
| **Implementation Complexity** | Low | Low | Medium |
| **Header Accessibility** | ✓ EXT1 pin 13 | ✓ EXT1 pin 7 | ⚠️ PC29 not on header |
| **Sensor INT available** | ✓ PA0 free | ❌ PA0 used | ✓ PA0 free |

### Recommendation Summary

| Use Case | Recommended Option |
|----------|-------------------|
| **Minimum viable quadcopter (400Hz PWM)** | **Option A** (PB0) |
| **DShot protocol required** | **Option A** (PB0) or **Option B** (PA0) |
| **Sensor interrupt needed on PA0** | **Option A** (PB0) |
| **Maximum pin flexibility** | **Option A** (PB0) |
| **Legacy TC-based PWM** | **Option C** (requires jumper) |

---

## Selected Configuration: Option A (PB0) - RECOMMENDED

For minimum viable PX4 quadcopter with 400Hz PWM or DShot, **Option A** is recommended:

### Final PWM Pin Assignment

| Motor | Pin | PWMC | Channel | Peripheral | Location | Wire Color (suggested) |
|-------|-----|------|---------|------------|----------|------------------------|
| 1 | **PA7** | PWM0 | **CH3** | Peripheral B | Arduino A1 | White |
| 2 | **PA2** | PWM0 | **CH1** | Peripheral A | EXT2 pin 9 | Yellow |
| 3 | **PC19** | PWM0 | **CH2** | Peripheral B | EXT2 pin 7 | Green |
| 4 | **PB0** | PWM0 | **CH0** | Peripheral A | EXT1 pin 13 | Blue |

**Key:** All 4 motors on PWM0 using 4 different channels (CH0-CH3) for full independent control and DShot sync.

### Pins to AVOID for PWM

| Pin | Reason |
|-----|--------|
| PA11-PA14, PA17 | QSPI flash signals |
| PA26 | HSMCI0 DA2 (SD card data line) |
| PA25-PA31 | HSMCI0 signals |
| PA3/PA4 | I2C0 bus |
| PD7 | Not routed to headers |
| PD31 | QSPI_IO3 |

---

## 6. ADC Configuration

### AFEC0 (ADC) Channels

| Channel | Pin | Function | Notes |
|---------|-----|----------|-------|
| AD0 | **PD30** | Battery Voltage | Primary |
| AD7 | **PA18** | Battery Current | Primary |
| AD11 | Internal | Temperature | Not configured |

### ADC GPIO Definitions

```c
/* AFEC0 ADC channels for battery monitoring */
#define ADC_BATTERY_VOLTAGE_CHANNEL  0    /* PD30 - AFEC0_AD0 */
#define ADC_BATTERY_CURRENT_CHANNEL  7    /* PA18 - AFEC0_AD7 */

#define ADC_CHANNELS ((1 << ADC_BATTERY_VOLTAGE_CHANNEL) | (1 << ADC_BATTERY_CURRENT_CHANNEL))

#define BOARD_ADC_BASE          SAM_AFEC0_BASE
#define BOARD_ADC_POS_REF_V     3.3f
```

### ADC Pin Conflicts to Avoid

| Pin | ADC Channel | Other Function | Status |
|-----|-------------|----------------|--------|
| PA18 | AFEC0_AD7 | PWMC1_EXTRG1 | OK - using for ADC |
| PA19 | AFEC0_AD8 | GPIO_MB1_RST | OK - using for RST |
| PB0 | AFEC0_AD10 | **PWM0_CH0 (Motor 4)** | **Used for PWM** (Option A) |
| PB3 | AFEC0_AD2 | ICM20689 CS (new) | **Used for CS** |

**Notes:**
- PB0 used for Motor 4 PWM (Option A) loses AFEC0_AD10 capability. Acceptable since AD10 not needed for battery monitoring.
- PB3 used for ICM20689 CS loses AFEC0_AD2 capability. Acceptable since AD2 not needed for battery monitoring.

---

## 7. Safety IO

### Safety Button

| Function | Pin | Configuration |
|----------|-----|---------------|
| Button Input | **PA9** | Active LOW, internal pullup, **polled** |

**Note:** The SafetyButton driver uses polling, not interrupts. No IRQ configuration needed.

### Safety LED

| Function | Pin | Configuration |
|----------|-----|---------------|
| LED Output | **PC9** | Active LOW (LED on when pin LOW) |

### Armed Status Output

| Function | Pin | Configuration |
|----------|-----|---------------|
| nARMED | **PA20** | Active LOW (LOW = armed) |

### Safety GPIO Definitions

```c
/* Safety Button: SW0 (PA9) - Active LOW */
#define GPIO_BTN_SAFETY       (GPIO_INPUT | GPIO_CFG_PULLUP | GPIO_PORT_PIOA | GPIO_PIN9)
#define BOARD_SAFETY_BUTTON_ACTIVE_LOW  1

/* Safety LED: LED1 (PC9) - Active LOW */
#define GPIO_LED_SAFETY       (GPIO_OUTPUT | GPIO_CFG_DEFAULT | GPIO_OUTPUT_SET | GPIO_PORT_PIOC | GPIO_PIN9)

/* Armed Status: PA20 - Active LOW */
#define GPIO_nARMED_INIT      (GPIO_OUTPUT | GPIO_CFG_PULLUP | GPIO_OUTPUT_SET | GPIO_PORT_PIOA | GPIO_PIN20)
#define GPIO_nARMED           (GPIO_OUTPUT | GPIO_CFG_DEFAULT | GPIO_OUTPUT_CLEAR | GPIO_PORT_PIOA | GPIO_PIN20)
```

**Note:** PA9 is also UART0_RX. Safety button takes priority; UART0 not used.

---

## 8. SD Card (HSMCI)

### HSMCI0 Data Lines (Fixed)

| Signal | Pin | Peripheral |
|--------|-----|------------|
| CLK | PA25 | Peripheral D |
| CMD | PA28 | Peripheral C |
| DA0 | PA30 | Peripheral C |
| DA1 | PA31 | Peripheral C |
| DA2 | PA26 | Peripheral C |
| DA3 | PA27 | Peripheral C |

### Card Detect - DISABLED

| Function | Hardware Pin | Status | Reason |
|----------|--------------|--------|--------|
| Card Detect | PD18 | **DISABLED** | True HW conflict with UART4 RX (RC) |

**Design Decision:** The SAMV71-XULT has card detect hardwired to PD18, which is also UART4_RX needed for RC/SBUS input. Since both functions require the same physical pin, **card detect is disabled** and the SD card is assumed to be always present.

This is common practice in flight controllers - most do not have hardware card detect.

### SD Card GPIO Definitions

```c
#ifdef CONFIG_SAMV7_HSMCI0
#  define HSMCI0_SLOTNO      0
#  define HSMCI0_MINOR       0
   /* Card Detect: DISABLED - PD18 used for UART4 RX (RC input)
    * Assume card always present (common in flight controllers)
    */
   // #define GPIO_HSMCI0_CD  - NOT DEFINED (card detect disabled)
   // #define IRQ_HSMCI0_CD   - NOT DEFINED
#endif
```

### Alternative: Hardware Modification
If card detect is critical, the CD signal trace can be cut and rerouted to a free GPIO. However, this requires PCB modification and is not recommended for this development board.

---

## 9. QSPI Flash (Parameter Storage)

### SAMV71-XULT On-Board Flash

The SAMV71-XULT has an **SST26VF032B** 32Mbit (4MB) QSPI flash on-board that can be used for parameter storage.

| Property | Value |
|----------|-------|
| Device | SST26VF032B |
| Capacity | 4MB (32Mbit) |
| Interface | Quad SPI (QSPI) |
| Max Speed | 104 MHz |

### QSPI Pin Assignment (Fixed on SAMV71-XULT)

| Signal | Pin | Peripheral | Notes |
|--------|-----|------------|-------|
| QSPI_SCK | **PA14** | Peripheral A | Clock - DO NOT USE FOR PWM |
| QSPI_CS | **PA11** | Peripheral A | Chip Select |
| QSPI_IO0 | **PA13** | Peripheral A | MOSI/Data0 |
| QSPI_IO1 | **PA12** | Peripheral A | MISO/Data1 |
| QSPI_IO2 | **PA17** | Peripheral A | Data2 |
| QSPI_IO3 | **PD31** | Peripheral A | Data3 |

### Why PA14 Cannot Be Used for PWM

PA14 is multiplexed between:
- **Peripheral A:** QSPI_SCK (clock for flash)
- **Peripheral C:** PWMC1_H1 (PWM high output)

Since we need QSPI for parameter storage, **Motor 4 must use an alternative pin**. See [Section 5](#5-pwm-configuration---three-options) for the three available options (PB0, PA0, or PC29).

### defconfig Settings for QSPI

```
# QSPI Flash Support
CONFIG_SAMV7_QSPI=y
CONFIG_SAMV7_QSPI_INTERRUPTS=y
CONFIG_SAMV7_QSPI_DMA=y

# MTD (Memory Technology Device) for Flash
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_SAMV7_QSPI_FLASHSIZE=4194304

# SmartFS or LittleFS for flash filesystem
CONFIG_FS_LITTLEFS=y
# or
# CONFIG_FS_SMARTFS=y
```

### Parameter Storage Configuration

For PX4 parameter storage on QSPI:

```c
/* board_config.h - Add for QSPI param storage */
#define FLASH_BASED_PARAMS
#define BOARD_HAS_QSPI_FLASH  1

/* Parameter file location */
// If using LittleFS on QSPI:
// CONFIG_BOARD_PARAM_FILE="/fs/qspi/params"
// CONFIG_BOARD_PARAM_BACKUP_FILE="/fs/qspi/params.bak"
```

### QSPI Pins - Reserved (Do Not Use)

| Pin | QSPI Function | Alternate | Decision |
|-----|---------------|-----------|----------|
| PA11 | QSPI_CS | PWM0_H0 | **Reserved for QSPI** |
| PA12 | QSPI_IO1 | PWM0_H1 | **Reserved for QSPI** |
| PA13 | QSPI_IO0 | PWM0_H2 | **Reserved for QSPI** |
| PA14 | QSPI_SCK | PWM1_H1 | **Reserved for QSPI** |
| PA17 | QSPI_IO2 | PWM0_H3 | **Reserved for QSPI** |
| PD31 | QSPI_IO3 | - | **Reserved for QSPI** |

---

## 10. LEDs

### Board LEDs

| LED | Pin | Function | Active |
|-----|-----|----------|--------|
| LED0 (Blue) | **PA23** | Status/Armed | LOW |
| LED1 (Yellow) | **PC9** | Safety LED | LOW |

### LED GPIO Definitions

```c
#define GPIO_nLED_BLUE       (GPIO_OUTPUT|GPIO_CFG_PULLUP|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN23)
```

---

## 11. USB

### USB High-Speed Device

| Signal | Pin | Notes |
|--------|-----|-------|
| USBHS | Internal | High-speed USB device |
| VBUS | - | Bus powered |

USB CDC/ACM provides /dev/ttyACM0 for MAVLink telemetry.

---

## 12. Complete Pin Usage Table

### Port A (PIOA)

| Pin | Function | Peripheral/GPIO | Notes |
|-----|----------|-----------------|-------|
| PA0 | EXT1 pin 7 / mikroBUS1 INT | GPIO Input | **Free** (Option A keeps this available) |
| PA2 | **PWM0_CH1 (Motor 2)** | Peripheral A | EXT2 pin 9 |
| PA3 | I2C0 SDA | Peripheral A | Sensor bus |
| PA4 | I2C0 SCL | Peripheral A | Sensor bus |
| PA5 | EXT1_RST | GPIO Output | EXT1 pin 10 reset line |
| PA7 | **PWM0_CH3 (Motor 1)** | Peripheral B | Arduino A1 |
| PA8 | Not routed | - | **Not on any header** |
| PA9 | **Safety Button** | GPIO Input | Active LOW |
| PA11 | **QSPI_CS** | Peripheral A | Flash chip select - RESERVED |
| PA12 | **QSPI_IO1** | Peripheral A | Flash data 1 - RESERVED |
| PA13 | **QSPI_IO0** | Peripheral A | Flash data 0 - RESERVED |
| PA14 | **QSPI_SCK** | Peripheral A | Flash clock - RESERVED |
| PA17 | **QSPI_IO2** | Peripheral A | Flash data 2 - RESERVED |
| PA18 | **ADC Current** | AFEC0_AD7 | Battery current |
| PA19 | mikroBUS1 RST | GPIO Output | Reset line |
| PA20 | **nARMED** | GPIO Output | Armed status |
| PA21 | Console RX | USART1 RX | Debug console |
| PA23 | **Blue LED** | GPIO Output | Status LED |
| PA24 | EXT2_RST | GPIO Output | Reset line |
| PA25 | SD CLK | HSMCI0 | SD card |
| PA26 | SD DA2 | HSMCI0 | SD card - DO NOT USE FOR PWM |
| PA27 | SD DA3 | HSMCI0 | SD card |
| PA28 | SD CMD | HSMCI0 | SD card |
| PA30 | SD DA0 | HSMCI0 | SD card |
| PA31 | SD DA1 | HSMCI0 | SD card |

### Port B (PIOB)

| Pin | Function | Peripheral/GPIO | Notes |
|-----|----------|-----------------|-------|
| PB0 | **PWM0_CH0 (Motor 4)** | Peripheral A | EXT1 pin 13 (Option A) |
| PB3 | **ICM20689 CS** | GPIO Output | Moved from PD25 (EXT1 pin 5) |
| PB4 | Console TX | USART1 TX | Debug console |

### Port C (PIOC)

| Pin | Function | Peripheral/GPIO | Notes |
|-----|----------|-----------------|-------|
| PC9 | **Safety LED** | GPIO Output | LED1, Active LOW |
| PC13 | EXT2 pin 4 / LCD RESET | GPIO | Available (LCD out of scope) |
| PC19 | **PWM0_CH2 (Motor 3)** | Peripheral B | EXT2 pin 7 |
| PC29 | TC5 TIOA5 | - | **Not on header** (Option C requires jumper) |

### Port D (PIOD)

| Pin | Function | Peripheral/GPIO | Notes |
|-----|----------|-----------------|-------|
| PD18 | **RC UART4 RX** | Peripheral C | SBUS input (also HW CD - disabled) |
| PD19 | UART4 TX | Peripheral C | Not used for SBUS |
| PD20 | SPI0 MISO | Peripheral B | Sensor SPI |
| PD21 | SPI0 MOSI | Peripheral B | Sensor SPI |
| PD22 | SPI0 SCK | Peripheral B | Sensor SPI |
| PD25 | **GPS UART2 RX** | Peripheral C | GPS input |
| PD26 | **GPS UART2 TX** | Peripheral C | GPS output |
| PD27 | BMP388 CS | GPIO Output | Barometer SPI CS |
| PD28 | ICM20689 DRDY | GPIO Input/IRQ | IMU data ready |
| PD30 | **ADC Voltage** | AFEC0_AD0 | Battery voltage |
| PD31 | **QSPI_IO3** | Peripheral A | Flash data 3 - RESERVED |

---

## 13. Conflict Resolution

### Hardware Modifications Required

1. **ICM20689 CS Rewiring:**
   - Disconnect CS from EXT1 Pin 15 (PD25)
   - Connect CS to **PB3 (EXT1 Pin 5)**
   - Use jumper wire on adapter board

### Software Changes Required (Option A - Recommended)

1. **board_config.h:**
   - Change `GPIO_SPI0_CS_ICM20689` from PD25 to PB3
   - **Remove** `GPIO_HSMCI0_CD` and `IRQ_HSMCI0_CD` (card detect disabled)
   - **Remove** `GPIO_MB2_RST` (PB0 now used for Motor 4 PWM)
   - Add `GPIO_BTN_SAFETY` to `PX4_GPIO_INIT_LIST`
   - Add PWM outputs for all 4 motors on PWM0

2. **PWM GPIO Definitions:**
```c
#define GPIO_PWM0_H0_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOB | GPIO_PIN0)   /* Motor 4 - CH0 */
#define GPIO_PWM0_H1_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN2)   /* Motor 2 - CH1 */
#define GPIO_PWM0_H2_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN19)  /* Motor 3 - CH2 */
#define GPIO_PWM0_H3_OUT  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN7)   /* Motor 1 - CH3 */
```

3. **defconfig:**
   - Enable CONFIG_SAMV7_UART2=y (GPS)
   - Enable CONFIG_SAMV7_UART4=y (RC)
   - Configure UART4 for SBUS: 100000/8/E/2
   - Enable CONFIG_SAMV7_PWM0=y
   - Optionally disable `CONFIG_MMCSD_HAVE_CARDDETECT` if issues arise

4. **default.px4board:**
   - Add CONFIG_DRIVERS_RC=y
   - Verify CONFIG_DRIVERS_GPS=y
   - Verify CONFIG_DRIVERS_PWM_OUT=y

### Software Changes for Alternative Options

**Option B (PA0 for Motor 4):**
```c
// board_config.h - use PA0 instead of PB0 for CH0
#define GPIO_PWM0_H0_OUT  (GPIO_PERIPHA | GPIO_CFG_DEFAULT | GPIO_PORT_PIOA | GPIO_PIN0)
// Note: Loses EXT1 pin 7 / mikroBUS1 INT capability
```

**Option C (TC-based PWM):**
```c
// board_config.h - use TC instead of PWMC
#define GPIO_TC5_TIOA  (GPIO_PERIPHB | GPIO_CFG_DEFAULT | GPIO_PORT_PIOC | GPIO_PIN29)
// Note: PC29 NOT on header - requires jumper wire to test point
// Note: Requires TC-based PWM driver, no DShot support
```

---

## Summary Checklist

### UART/Communication
- [ ] GPS on UART2 (PD25/PD26) - No hardware change
- [ ] RC on UART4 (PD18) - No hardware change
- [ ] Debug Console on USART1 (PA21/PB4) - No change

### Sensors (I2C)
- [ ] I2C bus on PA3/PA4 - No change
- [ ] AK09916 Magnetometer @ 0x0C
- [ ] BMI088 Accel @ 0x18, Gyro @ 0x68
- [ ] DPS310 Barometer @ 0x77

### Sensors (SPI)
- [ ] SPI bus on PD20/PD21/PD22 - No change
- [ ] ICM20689 CS moved to PB3 - **Hardware rewiring required**
- [ ] BMP388 CS on PD27 - No change

### Storage
- [ ] SD Card on HSMCI0 (PA25-31) - No change
- [ ] SD Card Detect - **DISABLED** (PD18 used for RC)
- [ ] QSPI Flash on PA11-PA14/PA17/PD31 - **RESERVED for params**

### PWM Motors (Option A - Recommended)
- [ ] Motor 1: PA7 (PWM0_CH3) - Arduino A1
- [ ] Motor 2: PA2 (PWM0_CH1) - EXT2 pin 9
- [ ] Motor 3: PC19 (PWM0_CH2) - EXT2 pin 7
- [ ] Motor 4: **PB0 (PWM0_CH0)** - EXT1 pin 13

### Alternative PWM Options
- [ ] Option B: Motor 4 on PA0 (PWM0_CH0, EXT1 pin 7) - loses sensor INT
- [ ] Option C: TC-based PWM (PC29 for Motor 4) - **requires jumper wire**

### ADC & Safety
- [ ] ADC Voltage on PD30 - No change
- [ ] ADC Current on PA18 - No change
- [ ] Safety Button on PA9 - **Polled** (no IRQ)
- [ ] Safety LED on PC9 - No change
- [ ] Armed Status on PA20 - No change

### Preserved Resources (Option A - PB0)
- [ ] PA0 (EXT1 pin 7 / mikroBUS1 INT) - **FREE** for sensor interrupts
- [ ] PC13 (EXT2 pin 4) - **FREE** (not used for PWM)
- [ ] PA8 - Not accessible (not on any header)

### Out of Scope (Explicitly Not Used)
- [ ] maXTouch LCD - pins repurposed for sensors/PWM
- [ ] Hardware Card Detect - disabled due to PD18 conflict

---

**Document Status:** DRAFT - Awaiting Review
**Last Updated:** January 2026

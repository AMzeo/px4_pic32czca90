# SAMV71Q21B PX4 Drone Pin Mapping Reference

> **Board:** SAM V71 Xplained Ultra (SAMV71Q21B, 144-pin LQFP)
> **Project:** PX4 Autopilot Port — Test Drone Configuration
> **Purpose:** Reference for Claude Code to verify pin assignments in the codebase before making changes

---

## 1. Drone Function Summary

These are the **active pins assigned to drone functions** on the test drone build. This is the primary reference for validating PX4 driver configurations.

### 1.1 PWM Motor Outputs

| Motor | Pin Name | Pin # | Peripheral Used | PIO Mux | Notes |
|-------|----------|-------|-----------------|---------|-------|
| Motor 1 | PB0 | 21 | PWMC0_PWMH0 | Peripheral A | PWM Channel 0, High output |
| Motor 2 | PA2 | 93 | PWMC0_PWMH1 | Peripheral A | PWM Channel 1, High output |
| Motor 3 | PC19 | 117 | PWMC0_PWMH2 | Peripheral B | PWM Channel 2, High output |
| Motor 4 | PC13 | 19 | PWMC0_PWMH3 | Peripheral D | PWM Channel 3, High output |

> **Note:** PA7 (Pin 35) also lists "PWM Motor" in the spreadsheet but is **conflicted with the 32.768kHz crystal input on the EVB**. It should NOT be used for motor PWM unless the crystal is bypassed/removed.

### 1.2 Serial / UART Interfaces

| Function | TX Pin | RX Pin | Peripheral | PIO Mux | External Device |
|----------|--------|--------|------------|---------|-----------------|
| Debug Console | PB4 (Pin 105) | PA21 (Pin 32) | USART1 (PB4=TXD via Periph C, PA21=RXD via Periph A) | C / A | Serial debug output |
| GPS | PD26 (Pin 53) | PD25 (Pin 52) | UART2 (PD26=UTXD2 via Periph C, PD25=URXD2 via Periph C) | C / C | Ready to Sky GPS Module |
| RC Receiver | — | PD18 (Pin 69) | UART4 (URXD4 via Periph A) | A | RadioMaster TX-16S with R81 V2 receiver |
| MAVLink Telemetry | — | PC16 (Pin 100) | USB Host (VBUS) | — | USB connection (EVB USB port) |

### 1.3 I2C Bus (TWI0)

| Function | Pin Name | Pin # | Peripheral | PIO Mux | Connected Devices |
|----------|----------|-------|------------|---------|-------------------|
| SDA | PA3 | 91 | TWD0 | Peripheral A | BMP388 (Pressure/Baro), BMM150 (Geomagnetic/Compass) |
| SCL | PA4 | 77 | TWCK0 | Peripheral A | BMP388 (Pressure/Baro), BMM150 (Geomagnetic/Compass) |

> Both sensors are on the **same I2C0 bus** via Click board interface (Pressure5 Click + GeoMagnetic Click).

### 1.4 SPI Bus (SPI0) — IMU

| Function | Pin Name | Pin # | Peripheral | PIO Mux | Connected Device |
|----------|----------|-------|------------|---------|------------------|
| MISO | PD20 | 65 | SPI0_MISO | Peripheral B | 6DOF IMU 27 Click (ICM-42688-P or similar) |
| MOSI | PD21 | 63 | SPI0_MOSI | Peripheral B | 6DOF IMU 27 Click |
| SCK | PD22 | 60 | SPI0_SPCK | Peripheral B | 6DOF IMU 27 Click |
| SS/CS | PD27 | 47 | GPIO (manual CS) | — | 6DOF IMU 27 Click |

> SPI0 is dedicated to the IMU. PD27 is used as a **GPIO chip select** (active low), not through hardware NPCS.

### 1.5 Analog Inputs (ADC/AFE)

| Function | Pin Name | Pin # | ADC Channel | Notes |
|----------|----------|-------|-------------|-------|
| Battery Voltage | PD30 | 34 | AFE0_AD0 | Voltage divider input for battery monitoring |
| Battery Current | PA18 | 24 | AFE0_AD7 | Current sense input for power monitoring |

### 1.6 GPIO — Status & Safety

| Function | Pin Name | Pin # | Direction | Notes |
|----------|----------|-------|-----------|-------|
| Safety Button | PA9 | 75 | Input | Physical safety switch (EVB: SWO — potential conflict) |
| Armed State LED | PA23 | 46 | Output | Indicates armed/disarmed state (EVB: LED0 — shared) |
| Safety LED | PC9 | 86 | Output | Safety status indicator (EVB: LED1 — shared) |
| nArmed Signal | PA20 | 22 | Output | Active-low armed indicator (EVB: SDRAM — **conflict!**) |

### 1.7 SD Card / Data Logging (HSMCI)

| Function | Pin Name | Pin # | Peripheral | PIO Mux |
|----------|----------|-------|------------|---------|
| MCDA2 | PA26 | 62 | HSMCI_MCDA2 | Peripheral B |
| MCDA3 | PA27 | 70 | HSMCI_MCDA3 | Peripheral B |
| MCCDA (CMD) | PA28 | 112 | HSMCI_MCCDA | Peripheral B |
| MCDA0 | PA30 | 116 | HSMCI_MCDA0 | Peripheral B |

> These pins are shared with the EVB SD card slot. Used for PX4 flight log data logging.

---

## 2. Pin Conflict Analysis

### 2.1 Critical Conflicts (Drone vs EVB)

| Pin | Pin # | Drone Function | EVB Function | Severity | Resolution |
|-----|-------|----------------|--------------|----------|------------|
| PA7 | 35 | PWM Motor (listed) | 32.768kHz Crystal In | **CRITICAL** | Do NOT use for PWM — crystal takes priority for RTC |
| PA8 | 36 | — | 32.768kHz Crystal Out | Info | Crystal pair with PA7 |
| PA20 | 22 | nArmed Signal | SDRAM | **HIGH** | SDRAM must be disabled or pin reassigned |
| PA9 | 75 | Safety Button | SWO (debug trace) | MEDIUM | SWO can be disabled in production |
| PA23 | 46 | Armed State LED | LED0 | LOW | Shared use — both indicate status |
| PC9 | 86 | Safety LED | LED1 | LOW | Shared use — both indicate status |
| PC16 | 100 | MAVLink Telemetry | USB | MEDIUM | Using USB for MAVLink — functional overlap |
| PD18 | 69 | RC UART4 RX | Card Detect | MEDIUM | SD Card detect sacrificed for RC input |

### 2.2 SDRAM Pin Usage Warning

The following pins are used by the EVB for SDRAM. If any drone functions are assigned to these pins, **SDRAM must be disabled** or the pins must be freed:

PA15, PA16, PA20 (conflict!), PC0–PC7, PC15, PC18, PC20–PC29, PD13–PD15, PD17, PD29, PE0–PE5

> **PA20 (nArmed Signal)** is the only drone function pin that conflicts with SDRAM. If SDRAM is needed, nArmed must be moved to another GPIO.

### 2.3 QSPI Flash Pins (EVB)

PA11, PA12, PA13, PA14, PA17, PA31, PD31 — all used by EVB QSPI Flash. **No drone function conflicts.**

### 2.4 Ethernet Pins (EVB)

PD0, PD9, PA19 (interrupt), PA29 (SigDet), PC10 (reset) — all used by EVB Ethernet. **No drone function conflicts.**

---

## 3. Peripheral Mux Configuration Summary

This table shows exactly which PIO peripheral mux (A/B/C/D) must be set for each drone function pin. **This is critical for validating the PX4 board config and pin mux initialization code.**

| Pin | Function | Required Mux | Peripheral Register |
|-----|----------|-------------|---------------------|
| PB0 | PWM Motor 1 (PWMC0_PWMH0) | **Peripheral A** | PIOB → ABCDSR1[0]=0, ABCDSR2[0]=0 |
| PA2 | PWM Motor 2 (PWMC0_PWMH1) | **Peripheral A** | PIOA → ABCDSR1[2]=0, ABCDSR2[2]=0 |
| PC19 | PWM Motor 3 (PWMC0_PWMH2) | **Peripheral B** | PIOC → ABCDSR1[19]=1, ABCDSR2[19]=0 |
| PC13 | PWM Motor 4 (PWMC0_PWMH3) | **Peripheral D** | PIOC → ABCDSR1[13]=1, ABCDSR2[13]=1 |
| PA3 | I2C SDA (TWD0) | **Peripheral A** | PIOA → ABCDSR1[3]=0, ABCDSR2[3]=0 |
| PA4 | I2C SCL (TWCK0) | **Peripheral A** | PIOA → ABCDSR1[4]=0, ABCDSR2[4]=0 |
| PD20 | SPI MISO (SPI0_MISO) | **Peripheral B** | PIOD → ABCDSR1[20]=1, ABCDSR2[20]=0 |
| PD21 | SPI MOSI (SPI0_MOSI) | **Peripheral B** | PIOD → ABCDSR1[21]=1, ABCDSR2[21]=0 |
| PD22 | SPI SCK (SPI0_SPCK) | **Peripheral B** | PIOD → ABCDSR1[22]=1, ABCDSR2[22]=0 |
| PD27 | SPI CS (GPIO) | **GPIO Output** | PIOD → PER[27]=1, OER[27]=1 |
| PD25 | GPS RX (URXD2) | **Peripheral C** | PIOD → ABCDSR1[25]=0, ABCDSR2[25]=1 |
| PD26 | GPS TX (UTXD2) | **Peripheral C** | PIOD → ABCDSR1[26]=0, ABCDSR2[26]=1 |
| PD18 | RC Input (URXD4) | **Peripheral A** | PIOD → ABCDSR1[18]=0, ABCDSR2[18]=0 |
| PA21 | Debug RX (USART1_RXD) | **Peripheral A** | PIOA → ABCDSR1[21]=0, ABCDSR2[21]=0 |
| PB4 | Debug TX (USART1_TXD) | **Peripheral C** | PIOB → ABCDSR1[4]=0, ABCDSR2[4]=1 |
| PD30 | Battery Voltage (AFE0_AD0) | **Extra Function** | Controlled via AFE0, not PIO mux |
| PA18 | Battery Current (AFE0_AD7) | **Extra Function** | Controlled via AFE0, not PIO mux |
| PA9 | Safety Button | **GPIO Input** | PIOA → PER[9]=1, ODR[9]=1 (+ pull-up) |
| PA23 | Armed LED | **GPIO Output** | PIOA → PER[23]=1, OER[23]=1 |
| PC9 | Safety LED | **GPIO Output** | PIOC → PER[9]=1, OER[9]=1 |
| PA20 | nArmed Signal | **GPIO Output** | PIOA → PER[20]=1, OER[20]=1 |
| PA26 | SD MCDA2 (HSMCI) | **Peripheral B** | PIOA → ABCDSR1[26]=1, ABCDSR2[26]=0 |
| PA27 | SD MCDA3 (HSMCI) | **Peripheral B** | PIOA → ABCDSR1[27]=1, ABCDSR2[27]=0 |
| PA28 | SD MCCDA (HSMCI) | **Peripheral B** | PIOA → ABCDSR1[28]=1, ABCDSR2[28]=0 |
| PA30 | SD MCDA0 (HSMCI) | **Peripheral B** | PIOA → ABCDSR1[30]=1, ABCDSR2[30]=0 |

---

## 4. EVB Connector / Header Pin Assignments

### 4.1 J501 — Power Header

| Pin | Signal | Function |
|-----|--------|----------|
| VBAT | Battery Voltage | — |
| VCC_TARGET_P3V3 | 3.3V Rail | — |
| NRST | Reset | — |
| 3.3V | Power | — |
| GND | Ground | — |
| VIN | Input Voltage | — |

### 4.2 J502 — Analog Low

| Pin | Signal | Function |
|-----|--------|----------|
| PD26 | GPS UART2 TX | — |
| PC31 | — | — |
| PA19 | — | — |
| PD30 | — | — |
| PC13 | PWM Motor 4 | — |
| PE00 | — | — |
| PE03 | — | — |
| PE04 | — | — |

### 4.3 J503 — Digital Low

| Pin | Signal | Function |
|-----|--------|----------|
| PD28 | — | — |
| PD30 | — | — |
| PA00 | — | — |
| PA06 | — | — |
| PD27 | — | — |
| PD11 | — | — |
| PC19 | PWM Motor 3 | — |
| PA02 | PWM Motor 2 | — |

### 4.4 J504 — Analog High

| Pin | Signal | Function |
|-----|--------|----------|
| PD24 | — | — |
| PA10 | — | — |
| PA22 | — | — |
| PE05 | — | — |
| PB13 | — | — |
| PD00 | — | — |
| PB03 | — | — |
| PB02 | — | — |

### 4.5 J505 — Communication

| Pin | Signal | Function |
|-----|--------|----------|
| PD28 | — | — |
| PD27 | — | — |
| PD18 | RC UART4 RX | RadioMaster R81 V2 |
| PD19 | — | — |
| PD15 | — | — |
| PD16 | — | — |
| PB00 | PWM Motor 1 | — |
| PB01 | — | — |

### 4.6 J506 — SPI

| Pin | Signal | Function |
|-----|--------|----------|
| PD20 | SPI0 MISO | 6DOF IMU 27 Click |
| 5V | Power | — |
| PD22 | SPI0 SCK | 6DOF IMU 27 Click |
| PD21 | SPI0 MOSI | 6DOF IMU 27 Click |
| PD22 | SPI0 SCK (duplicate) | — |
| NRST | Reset | — |
| GND | Ground | — |

### 4.7 J507 — Digital Extra

| Pin | Signal | Function |
|-----|--------|----------|
| 5V | Power | — |
| 5V | Power | — |
| PA18 | Battery Current (ADC) | — |
| PB01 | — | — |
| PB00 | PWM Motor 1 | — |
| PD19 | — | — |
| PD18 | RC UART4 RX | — |

### 4.8 USB Connector

| Pin | Signal | Function |
|-----|--------|----------|
| PC16 | VBUS Host Enable | MAVLink Telemetry |
| HSDM | USB D+ | — |
| HSDP | USB D- | — |

### 4.9 Extension Header EXT1

| Pin # | Signal | Function |
|-------|--------|----------|
| 3 | PC31 | ADC+ |
| 4 | PA19 | ADC- |
| 5 | PB03 | GPIO1 |
| 6 | PB02 | GPIO2 |
| 7 | PA00 | PWM+ |
| 8 | PC30 | PWM- |
| 9 | PD28 | IRQ/GPIO |
| 10 | PA05 | SPI_SS_B/GPIO |
| 11 | PA03 | TWI_SDA (Pressure5 Click, Geomagnetic Click) |
| 12 | PA04 | TWI_SCL (Pressure5 Click, Geomagnetic Click) |
| 13 | PB00 | USART_RX |
| 14 | PB01 | USART_TX |
| 15 | PD25 | SPI_SS_A (GPS UART2 RX) |
| 16 | PD21 | SPI_MOSI (6DOF IMU 27 Click) |
| 17 | PD20 | SPI_MISO (6DOF IMU 27 Click) |
| 18 | PD22 | SPI_SCK (6DOF IMU 27 Click) |
| 19 | GND | Ground |
| 20 | 3V3 | VCC |

### 4.10 Extension Header EXT2

| Pin # | Signal | Function |
|-------|--------|----------|
| 3 | PD30 | ADC+ |
| 4 | PC13 | ADC- |
| 5 | PA06 | GPIO1 |
| 6 | PD11 | GPIO2 |
| 7 | PC19 | PWM+ |
| 8 | PD26 | PWM- (GPS UART2 TX) |
| 9 | PA02 | IRQ/GPIO |
| 10 | PA24 | SPI_SS_B/GPIO |
| 11 | PA03 | TWI_SDA |
| 12 | PA04 | TWI_SCL |
| 13 | PA21 | USART_RX |
| 14 | PB04 | USART_TX |
| 15 | PD27 | SPI_SS_A (6DOF IMU 27 Click) |
| 16 | PD21 | SPI_MOSI |
| 17 | PD20 | SPI_MISO |
| 18 | PD22 | SPI_SCK |
| 19 | GND | Ground |
| 20 | VCC | VCC |

### 4.11 Camera Interface

| Pin | Signal |
|-----|--------|
| 3V3, GND | Power |
| PB13 | — |
| PC19 | — |
| PA04 | — |
| PA03 | — |
| GND, PA06, GND | — |
| PD25, GND | — |
| PD24, GND | — |
| PA24, GND | — |
| PD22, PD21 | — |
| PB03 | — |
| PA09, PA05 | — |
| PD11, PD12 | — |
| PA27 | — |
| PD27, PD28 | — |
| PD30, PD31 | — |
| GND | — |

---

## 5. External Parts / Click Boards

| Click Board | Interface | Pins Used | Function |
|-------------|-----------|-----------|----------|
| **Pressure5 Click** (BMP388) | I2C (TWI0) | PA3 (SDA), PA4 (SCL) | Barometric pressure / altitude |
| **GeoMagnetic Click** (BMM150) | I2C (TWI0) | PA3 (SDA), PA4 (SCL) | Compass / magnetometer |
| **6DOF IMU 27 Click** (ICM-series) | SPI0 | PD20 (MISO), PD21 (MOSI), PD22 (SCK), PD27 (CS) | Accelerometer + Gyroscope |
| **Ready to Sky GPS Module** | UART2 | PD25 (RX), PD26 (TX) | GPS/GNSS positioning |
| **RadioMaster R81 V2 Receiver** | UART4 | PD18 (RX only) | RC control input |

---

## 6. Code Verification Checklist

When reviewing the PX4 codebase for SAMV71 board support, verify the following against this pin map:

### Board Config (`board.h` / `board_config.h`)
- [ ] PWM output pins match: PB0 (CH0), PA2 (CH1), PC19 (CH2), PC13 (CH3)
- [ ] PWM peripheral mux settings correct (A, A, B, D respectively)
- [ ] HSMCI pins for SD card: PA26, PA27, PA28, PA30 all on Peripheral B
- [ ] I2C0/TWI0 on PA3/PA4 (Peripheral A) — bus scan should find BMP388 + BMM150
- [ ] SPI0 on PD20/PD21/PD22 (Peripheral B) with PD27 as GPIO CS
- [ ] UART2 for GPS on PD25/PD26 (Peripheral C)
- [ ] UART4 for RC on PD18 (Peripheral A) — RX only
- [ ] Debug USART1: PA21 (RX, Periph A) + PB4 (TX, Periph C)
- [ ] ADC channels: PD30 → AFE0_AD0 (battery voltage), PA18 → AFE0_AD7 (battery current)

### GPIO Initialization
- [ ] PA9 configured as input with pull-up (safety button)
- [ ] PA23 configured as output (armed LED)
- [ ] PC9 configured as output (safety LED)
- [ ] PA20 configured as output (nArmed signal) — **verify SDRAM is disabled**
- [ ] PD27 configured as output, default HIGH (SPI CS deselected)

### Clock / Power
- [ ] PA7/PA8 reserved for 32.768kHz crystal — NOT available for PWM
- [ ] PWMC0 clock enabled in PMC
- [ ] SPI0 clock enabled in PMC
- [ ] TWI0 clock enabled in PMC
- [ ] UART2, UART4, USART1 clocks enabled in PMC
- [ ] AFE0 clock enabled in PMC

### Known Issues to Check
1. **PA7 "PWM Motor" ambiguity** — The spreadsheet lists PA7 as "PWM Motor" but it's the 32.768kHz crystal input on the EVB. Confirm code does NOT configure PA7 for PWM output.
2. **PA20 vs SDRAM** — nArmed signal conflicts with SDRAM. Verify SDRAM controller is disabled or PA20 is remapped.
3. **PD18 vs Card Detect** — RC UART4 RX replaces SD card detect. Verify HSMCI driver doesn't rely on card detect GPIO.
4. **Debug console cross-port** — TX (PB4/USART1) and RX (PA21/USART1) are on different PIO ports. Verify driver handles this correctly with proper mux on each port.
5. **Motor PWM channels span 3 PIO ports** — PB0 (PIOB), PA2 (PIOA), PC19 (PIOC), PC13 (PIOC). Verify all three PIO controllers have peripheral mux configured.

---

## 7. Full Pin Table (144 Pins)

<details>
<summary>Click to expand complete pin listing</summary>

| # | Pin | Name | Type | Periph A | Periph B | Periph C | Periph D | Extra | EVB Function | Drone Function | Parts |
|---|-----|------|------|----------|----------|----------|----------|-------|-------------|----------------|-------|
| 1 | 102 | PA0 | I/O | PWMC0_PWMH0 | TIOA0 | QSPI_QSCK | USART0_CTS | AFE0_AD10/WKUP0 | — | — | — |
| 2 | 99 | PA1 | I/O | PWMC0_PWML0 | TIOB0 | QSPI_QCS | USART0_RTS | AFE0_AD0 | — | — | — |
| 3 | 93 | PA2 | I/O | PWMC0_PWMH1 | TWD0 | QSPI_QIO1 | — | AFE0_AD1/WKUP1 | — | **PWM Motor 2** | — |
| 4 | 91 | PA3 | I/O | TWD0 | SPI0_NPCS3 | LCDDAT2 | USART0_RXD | PWMC0_PWMEXTRG0 | — | **I2C SDA** | BMP388, BMM150 |
| 5 | 77 | PA4 | I/O | TWCK0 | SPI0_NPCS3 | LCDDAT3 | USART0_TXD | PWMC0_PWMEXTRG1 | — | **I2C SCL** | — |
| 6 | 73 | PA5 | I/O | PWMC1_PWML3 | ISI_D4 | LCDDAT4 | USART0_RXD | — | — | — | — |
| 7 | 114 | PA6 | I/O | PCK0 | ISI_D5 | LCDDAT5 | USART0_TXD | — | — | — | — |
| 8 | 35 | PA7 | I/O | PWMC0_PWMH3 | ISI_D6 | LCDDAT6 | USART0_CLK | AFE0_AD5 | **32.768kHz Xtal In** | ~~PWM Motor~~ | — |
| 9 | 36 | PA8 | I/O | PWMC0_PWMH3 | AFE0_ADTRG | LCDDAT7 | USART0_CTS | AFE0_AD2 | **32.768kHz Xtal Out** | — | — |
| 10 | 75 | PA9 | I/O | PWMC0_PWML0 | ISI_D3 | LCDDAT10 | UART0_RXD | PWMC0_PWMFI0 | SWO | **Safety Button** | — |
| 11 | 66 | PA10 | I/O | PWMC0_PWML1 | ISI_D8 | LCDDAT11 | UART0_TXD | PWMC0_PWMFI1 | — | — | — |
| 12 | 64 | PA11 | I/O | PWMC0_PWML2 | QSPI_QCS | LCDDAT12 | UART0_CTS | AFE0_AD6 | QSPI Flash | — | — |
| 13 | 68 | PA12 | I/O | PWMC0_PWMH1 | QSPI_QIO1 | LCDDAT13 | UART0_RTS | AFE0_AD7 | QSPI Flash | — | — |
| 14 | 42 | PA13 | I/O | PWMC0_PWMH2 | QSPI_QIO0 | LCDDAT14 | — | AFE0_AD3 | QSPI Flash | — | — |
| 15 | 51 | PA14 | I/O | PWMC0_PWMH3 | QSPI_QSCK | LCDDAT15 | — | AFE0_AD4 | QSPI Flash | — | — |
| 16 | 49 | PA15 | I/O | TIOA1 | SPI0_NPCS2 | LCDDAT16 | TXD4 | — | SDRAM | — | — |
| 17 | 45 | PA16 | I/O | TIOB1 | SPI0_MISO | LCDDAT17 | RXD4 | — | SDRAM | — | — |
| 18 | 25 | PA17 | I/O | PWMC0_PWMH3 | QSPI_QIO2 | PCK1 | USART2_CTS | AFE1_AD6 | QSPI Flash | — | — |
| 19 | 24 | PA18 | I/O | TIOA2 | PWMC1_EXTRG1 | PCK2 | USART2_RTS | AFE0_AD7 | — | **Battery Current** | — |
| 20 | 23 | PA19 | I/O | TIOB2 | PWMC0_PWML0 | LCDDAT19 | USART2_RXD | AFE0_AD8/WKUP9 | Ethernet Int | — | — |
| 21 | 22 | PA20 | I/O | TCLK2 | PWMC0_PWML1 | LCDDAT20 | USART2_TXD | AFE0_AD9/WKUP10 | SDRAM | **nArmed Signal** | — |
| 22 | 32 | PA21 | I/O | USART1_RXD | PCK1 | ISI_D8 | TWD1 | PWMC0_PWMFI2 | — | **Debug Console** | — |
| 23 | 37 | PA22 | I/O | USART1_TXD | SPI0_MISO | ISI_D9 | TWCK1 | — | — | — | — |
| 24 | 46 | PA23 | I/O | USART1_SCK | SPI0_MOSI | ISI_D10 | TWD0 | PWMC0_PWML3 | LED0 | **ARMED STATE LED** | — |
| 25 | 56 | PA24 | I/O | USART1_RTS | SPI0_NPCS1 | ISI_D11 | TWCK0 | — | — | — | — |
| 26 | 59 | PA25 | I/O | SPI0_NPCS1 | HSMCI_MCCK | LCDDAT21 | — | — | — | — | — |
| 27 | 62 | PA26 | I/O | SPI0_NPCS2 | HSMCI_MCDA2 | LCDDAT22 | — | — | SD Card | **Data Logging** | — |
| 28 | 70 | PA27 | I/O | SPI0_NPCS3 | HSMCI_MCDA3 | LCDDAT23 | USART1_CTS | — | SD Card | **Data Logging** | — |
| 29 | 112 | PA28 | I/O | USART1_CTS | HSMCI_MCCDA | LCDDAT24 | SPI0_NPCS3 | — | SD Card | **Data Logging** | — |
| 30 | 129 | PA29 | I/O | USART2_CTS | HSMCI_MCDA0 | LCDDAT25 | PWMC0_PWMH1 | AFE1_AD1/WKUP11 | Ethernet SigDet | — | — |
| 31 | 116 | PA30 | s | USART2_RTS | HSMCI_MCDA0 | LCDDAT26 | PWMC0_PWMH2 | WKUP12 | SD Card | **Data Logging** | — |
| 32 | 118 | PA31 | I/O | SPI0_NPCS1 | HSMCI_MCDA1 | LCDDAT27 | PWMC0_PWMH3 | — | QSPI Flash | — | — |
| 33 | 21 | PB0 | I/O | PWMC0_PWMH0 | AFE0_ADTRG | UART0_RXD | USART0_RTS | AFE0_AD10 | — | **PWM Motor 1** | — |
| 34 | 20 | PB1 | I/O | PWMC0_PWMH1 | GTXCK | UART0_TXD | USART0_CTS | AFE1_AD0 | — | — | — |
| 35 | 26 | PB2 | I/O | URXD1 | ISI_D10 | GMDC | SPI0_NPCS2 | AFE1_AD3 | — | — | — |
| 36 | 31 | PB3 | I/O | UTXD1 | ISI_D11 | GMDIO | SPI0_NPCS3 | AFE1_AD2 | — | — | — |
| 37 | 105 | PB4 | I/O | TIOA3 | TWD1 | USART1_TXD | USART2_RXD | — | — | **Debug Console** | — |
| 38 | 109 | PB5 | I/O | TIOB3 | TWCK1 | USART1_RXD | USART2_TXD | — | — | — | — |
| 39 | 79 | PB6 | I/O | TCLK3 | TIOA4 | USART1_SCK | SPI0_NPCS3 | — | — | — | — |
| 40 | 89 | PB7 | I/O | TIOA5 | TIOB4 | USART1_RTS | QSPI_QIO2 | — | — | — | — |
| 41 | 141 | PB8 | I/O | TIOB5 | TCLK4 | USART1_CTS | QSPI_QIO3 | — | — | — | — |
| 42 | 142 | PB9 | I/O | TCLK5 | TIOA0 | LCDDAT4 | QSPI_QIO0 | — | — | — | — |
| 43 | 87 | PB12 | I/O | PWMC0_PWML1 | PCK1 | GMDC | SPI0_MISO | — | Chip Erase | — | — |
| 44 | 144 | PB13 | I/O | PWMC0_PWML2 | PCK2 | GMDIO | SPI0_MOSI | — | — | — | — |
| 45 | 11 | PC0 | I/O | LCDDAT0 | PWMC0_PWMH0 | — | — | — | SDRAM | — | — |
| 46 | 38 | PC1 | I/O | LCDDAT1 | PWMC0_PWMH1 | ISI_D8 | USART1_CTS | — | SDRAM | — | — |
| 47 | 39 | PC2 | I/O | LCDDAT2 | PWMC0_PWMH2 | ISI_D9 | URXD1 | — | SDRAM | — | — |
| 48 | 40 | PC3 | I/O | LCDDAT3 | PWMC0_PWMH3 | ISI_D10 | UTXD1 | — | SDRAM | — | — |
| 49 | 41 | PC4 | I/O | LCDDAT4 | PWMC0_PWML0 | ISI_D11 | USART1_RTS | — | SDRAM | — | — |
| 50 | 58 | PC5 | I/O | LCDDAT5 | PWMC0_PWML1 | LCDDAT21 | — | — | SDRAM | — | — |
| 51 | 54 | PC6 | I/O | LCDDAT6 | PWMC0_PWML2 | LCDDAT22 | USART2_RXD | — | SDRAM | — | — |
| 52 | 48 | PC7 | I/O | LCDDAT7 | PWMC0_PWML3 | LCDDAT23 | USART2_TXD | — | SDRAM | — | — |
| 53 | 82 | PC8 | I/O | LCDDAT8 | TIOA3 | LCDDAT0 | PWMC0_PWMH0 | — | — | — | — |
| 54 | 86 | PC9 | I/O | LCDDAT9 | TIOB3 | LCDDAT1 | PWMC0_PWMH1 | — | LED1 | **Safety LED** | — |
| 55 | 90 | PC10 | I/O | LCDDAT10 | TCLK3 | LCDDAT2 | PWMC0_PWMH2 | — | Ethernet RESET | — | — |
| 56 | 94 | PC11 | I/O | LCDDAT11 | TIOA4 | LCDDAT3 | USART3_RXD | — | — | — | — |
| 57 | 17 | PC12 | I/O | LCDDAT12 | TIOB4 | LCDDAT4 | USART3_TXD | — | CANRX1 | — | — |
| 58 | 19 | PC13 | I/O | LCDDAT13 | TCLK4 | LCDDAT5 | PWMC0_PWMH3 | — | — | **PWM Motor 4** | — |
| 59 | 97 | PC14 | I/O | LCDDAT14 | TIOA5 | LCDDAT6 | USART3_SCK | — | CANTX1 | — | — |
| 60 | 18 | PC15 | I/O | LCDDAT15 | TIOB5 | LCDDAT7 | USART3_RTS | — | SDRAM | — | — |
| 61 | 100 | PC16 | I/O | LCDDAT16 | SPI0_NPCS2 | LCDDAT18 | USART3_CTS | — | USB | **MAVLink Telemetry** | — |
| 62 | 103 | PC17 | I/O | LCDDAT17 | SPI0_MISO | LCDDAT19 | TWD0 | — | — | — | — |
| 63 | 111 | PC18 | I/O | LCDDAT18 | SPI0_MOSI | LCDDAT20 | TWCK0 | — | SDRAM | — | — |
| 64 | 117 | PC19 | I/O | LCDDAT19 | PWMC0_PWMH2 | LCDDAT21 | — | AFE1_AD7 | — | **PWM Motor 3** | — |
| 65 | 120 | PC20 | I/O | LCDDAT20 | PWMC0_PWML0 | LCDDAT22 | QSPI_QIO0 | — | SDRAM | — | — |
| 66 | 122 | PC21 | I/O | LCDDAT21 | PWMC0_PWML1 | LCDDAT23 | QSPI_QIO1 | — | SDRAM | — | — |
| 67 | 124 | PC22 | I/O | LCDDAT22 | PWMC0_PWML2 | LCDDAT24 | QSPI_QIO2 | — | SDRAM | — | — |
| 68 | 127 | PC23 | I/O | LCDDAT23 | TIOA3 | LCDDAT25 | QSPI_QIO3 | — | SDRAM | — | — |
| 69 | 130 | PC24 | I/O | LCDDAT24 | PWMC0_PWML3 | LCDDAT26 | QSPI_QSCK | — | SDRAM | — | — |
| 70 | 133 | PC25 | I/O | LCDPWM | TIOB3 | LCDDAT27 | QSPI_QCS | — | SDRAM | — | — |
| 71 | 13 | PC26 | I/O | LCDDISP | TIOA4 | LCDDAT0 | USART2_CTS | — | SDRAM | — | — |
| 72 | 12 | PC27 | I/O | LCDVSYNC | TIOB4 | LCDDAT1 | SPI0_NPCS2 | — | SDRAM | — | — |
| 73 | 76 | PC28 | I/O | LCDHSYNC | TCLK4 | LCDDAT2 | SPI0_NPCS3 | — | SDRAM | — | — |
| 74 | 16 | PC29 | I/O | LCDPCK | TIOA5 | LCDDAT3 | USART2_RTS | — | SDRAM | — | — |
| 75 | 15 | PC30 | I/O | LCDDEN | TIOB5 | LCDDAT4 | URXD1 | — | — | — | — |
| 76 | 14 | PC31 | I/O | LCDCC | TCLK5 | LCDDAT5 | UTXD1 | — | — | — | — |
| 77 | 1 | PD0 | I/O | GTXCK | PWMC0_PWMH0 | TIOA1 | USART2_RTS | — | Ethernet | — | — |
| 78 | 132 | PD1 | I/O | GTXEN | PWMC0_PWMH1 | TIOB1 | USART2_CTS | — | — | — | — |
| 79 | 131 | PD2 | I/O | GTX0 | PWMC0_PWMH2 | TCLK1 | USART2_RXD | — | — | — | — |
| 80 | 128 | PD3 | I/O | GTX1 | PWMC0_PWMH3 | TIOA2 | USART2_TXD | — | — | — | — |
| 81 | 126 | PD4 | I/O | GRXDV | PWMC0_PWML0 | TIOB2 | USART2_SCK | — | — | — | — |
| 82 | 125 | PD5 | I/O | GRX0 | PWMC0_PWML1 | TCLK2 | USART3_RXD | — | — | — | — |
| 83 | 121 | PD6 | I/O | GRX1 | PWMC0_PWML2 | TIOA0 | USART3_TXD | — | — | — | — |
| 84 | 119 | PD7 | I/O | GRXER | PWMC0_PWML3 | TIOB0 | USART3_SCK | — | — | — | — |
| 85 | 113 | PD8 | I/O | GMDC | PWMC0_PWMFI0 | TCLK0 | USART3_RTS | — | — | — | — |
| 86 | 110 | PD9 | I/O | GMDIO | PWMC0_PWMFI1 | URXD0 | USART3_CTS | — | Ethernet | — | — |
| 87 | 101 | PD10 | I/O | GCRS | PWMC0_PWMFI2 | UTXD0 | ISI_HSYNC | AFE1_AD4 | — | — | — |
| 88 | 98 | PD11 | I/O | GRX2 | PWMC0_PWMH0 | ISI_D0 | ISI_VSYNC | — | — | — | — |
| 89 | 92 | PD12 | I/O | GRX3 | PWMC0_PWMH1 | ISI_D1 | ISI_PCK | — | — | — | — |
| 90 | 88 | PD13 | I/O | GCOL | PWMC0_PWMH2 | ISI_D2 | USART1_CTS | — | SDRAM | — | — |
| 91 | 84 | PD14 | I/O | GRXCK | PWMC0_PWMH3 | ISI_D3 | USART1_RTS | — | SDRAM | — | — |
| 92 | 106 | PD15 | I/O | GTX2 | PWMC0_PWML0 | ISI_D4 | USART2_RXD | — | SDRAM | — | — |
| 93 | 78 | PD16 | I/O | GTX3 | PWMC0_PWML1 | ISI_D5 | USART2_TXD | — | — | — | — |
| 94 | 74 | PD17 | I/O | GTXER | PWMC0_PWML2 | ISI_D6 | USART2_SCK | — | SDRAM | — | — |
| 95 | 69 | PD18 | I/O | URXD4 | PWMC0_PWML3 | ISI_D7 | HSMCI_CD | — | Card Detect | **RC UART4 RX** | RadioMaster R81 V2 |
| 96 | 67 | PD19 | I/O | UTXD4 | PWMC0_PWMFI0 | ISI_D8 | — | — | — | — | — |
| 97 | 65 | PD20 | I/O | URXD2 | SPI0_MISO | ISI_D9 | USART1_RXD | — | — | **SPI0 MISO** | 6DOF IMU 27 |
| 98 | 63 | PD21 | I/O | UTXD2 | SPI0_MOSI | ISI_D10 | USART1_TXD | — | — | **SPI0 MOSI** | — |
| 99 | 60 | PD22 | I/O | URXD3 | SPI0_SPCK | ISI_D11 | USART1_SCK | — | — | **SPI0 SCK** | — |
| 100 | 57 | PD23 | I/O | UTXD3 | SPI0_NPCS0 | ISI_PCK | USART1_RTS | — | SDRAM | — | — |
| 101 | 55 | PD24 | I/O | TIOA1 | SPI0_NPCS1 | ISI_VSYNC | USART1_CTS | — | — | — | — |
| 102 | 52 | PD25 | I/O | TIOB1 | SPI0_NPCS2 | URXD2 | ISI_HSYNC | — | — | **GPS UART2 RX** | Ready to Sky GPS |
| 103 | 53 | PD26 | I/O | TCLK1 | SPI0_NPCS3 | UTXD2 | USART0_RXD | — | — | **GPS UART2 TX** | — |
| 104 | 47 | PD27 | I/O | TIOA2 | PWMC0_PWMFI0 | URXD3 | USART0_TXD | — | — | **SPI0 SS** | 6DOF IMU 27 |
| 105 | 71 | PD28 | I/O | TIOB2 | PWMC0_PWMFI1 | UTXD3 | USART0_SCK | — | — | — | — |
| 106 | 108 | PD29 | I/O | TCLK2 | PWMC0_PWMFI2 | URXD0 | USART0_RTS | — | SDRAM | — | — |
| 107 | 34 | PD30 | I/O | TIOA0 | PWMC0_PWMEXTRG0 | UTXD0 | USART0_CTS | AFE0_AD0 | — | **Battery Voltage** | — |
| 108 | 2 | PD31 | I/O | TIOB0 | PWMC0_PWMEXTRG1 | URXD4 | QSPI_QIO3 | AFE1_AD5 | QSPI Flash | — | — |
| 109 | 4 | PE0 | I/O | TCLK0 | ISI_D2 | UTXD4 | HSMCI_WP | — | SDRAM | — | — |
| 110 | 6 | PE1 | I/O | TIOA3 | ISI_D3 | URXD1 | SPI0_NPCS0 | — | SDRAM | — | — |
| 111 | 7 | PE2 | I/O | TIOB3 | ISI_D4 | UTXD1 | SPI0_NPCS1 | — | SDRAM | — | — |
| 112 | 10 | PE3 | I/O | TCLK3 | ISI_D5 | USART1_SCK | SPI0_NPCS2 | — | SDRAM | — | — |
| 113 | 27 | PE4 | I/O | TIOA4 | ISI_D6 | USART1_RTS | SPI0_NPCS3 | — | SDRAM | — | — |
| 114 | 28 | PE5 | I/O | TIOB4 | ISI_D7 | USART1_CTS | QSPI_QIO0 | — | SDRAM | — | — |

</details>

---

## 8. Quick-Reference: PX4 Driver ↔ Pin Mapping

| PX4 Driver | Interface | Pins | Peripheral Config |
|------------|-----------|------|-------------------|
| `pwm_out` | PWMC0 CH0-CH3 | PB0, PA2, PC19, PC13 | Mux: A, A, B, D |
| `bmp388` (baro) | TWI0 / I2C | PA3 (SDA), PA4 (SCL) | Mux: A, A |
| `bmm150` (mag) | TWI0 / I2C | PA3 (SDA), PA4 (SCL) | Mux: A, A (shared bus) |
| `icm42688p` (IMU) | SPI0 | PD20, PD21, PD22, PD27 (CS) | Mux: B, B, B, GPIO |
| `gps` | UART2 | PD25 (RX), PD26 (TX) | Mux: C, C |
| `rc_input` | UART4 | PD18 (RX) | Mux: A |
| `mavlink` | USB | PC16 (VBUS) | USB peripheral |
| `battery_status` | AFE0 (ADC) | PD30 (V), PA18 (I) | ADC channels AD0, AD7 |
| `safety_button` | GPIO | PA9 (input) | PIO, pull-up |
| `led` (armed) | GPIO | PA23 (output) | PIO |
| `led` (safety) | GPIO | PC9 (output) | PIO |
| `tone_alarm` / nArmed | GPIO | PA20 (output) | PIO |
| `logger` (SD) | HSMCI | PA26, PA27, PA28, PA30 | Mux: B, B, B, B |
| `serial_console` | USART1 | PA21 (RX), PB4 (TX) | Mux: A, C |

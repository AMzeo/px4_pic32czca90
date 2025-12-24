# SPI Sensor Integration for SAMV71-XULT with Click Boards

This document describes how SPI sensors were integrated on the SAMV71-XULT Xplained Ultra board using Xplained Pro extension headers (EXT1, EXT2) with mikroBUS Click board adapters.

## Table of Contents
1. [Hardware Configuration](#hardware-configuration)
2. [Working Sensors](#working-sensors)
3. [Code Modifications](#code-modifications)
4. [Adding New SPI Sensors](#adding-new-spi-sensors)
5. [Known Issues and Limitations](#known-issues-and-limitations)
6. [Troubleshooting](#troubleshooting)

---

## Hardware Configuration

### SPI Bus Overview

The SAMV71-XULT uses **SPI0** for sensor communication. Both EXT1 and EXT2 extension headers share this SPI bus with different chip select (CS) pins.

| Signal | SAMV71 Pin | Function |
|--------|------------|----------|
| MOSI   | PD21       | SPI0 MOSI |
| MISO   | PD20       | SPI0 MISO |
| SCK    | PD22       | SPI0 SPCK |

### Extension Header Pin Mapping

#### EXT1 Header (Primary IMU location)
| Function | EXT1 Pin | SAMV71 GPIO |
|----------|----------|-------------|
| CS       | Pin 15   | PD25        |
| IRQ/DRDY | Pin 9    | PD28        |
| RST      | Pin 10   | PA5         |

#### EXT2 Header (Secondary sensor location)
| Function | EXT2 Pin | SAMV71 GPIO |
|----------|----------|-------------|
| CS       | Pin 15   | PD27        |
| IRQ/DRDY | Pin 9    | PA2         |
| RST      | Pin 10   | PA24        |

### Current Sensor Assignment
- **EXT1**: ICM-20689 IMU (6DOF IMU 6 Click - MIKROE-4044)
- **EXT2**: BMP388 Barometer (Pressure 5 Click - MIKROE-3566)

---

## Working Sensors

### ICM-20689 (6DOF IMU 6 Click) on EXT1
- **Status**: Working
- **Interface**: SPI
- **CS Pin**: PD25 (EXT1 Pin 15)
- **DRDY Pin**: PD28 (EXT1 Pin 9) - configured but not receiving interrupts
- **Start Command**: `icm20689 start -s`
- **Notes**: Operating in FIFO polling mode due to DRDY not connected on Click adapter

### BMP388 (Pressure 5 Click) on EXT2
- **Status**: Working (after SPI driver fix)
- **Interface**: SPI
- **CS Pin**: PD27 (EXT2 Pin 15)
- **Start Command**: `bmp388 start -s`
- **Notes**: Required driver fix for SPI dummy byte protocol

---

## Code Modifications

### 1. BMP388 SPI Driver Fix (CRITICAL)

**File**: `src/drivers/barometer/bmp388/bmp388_spi.cpp`

**Problem**: The BMP388 SPI protocol requires a **dummy byte** after the address byte on reads. The original driver was reading the dummy byte instead of actual data, causing CHIP_ID to read as 0x00.

**BMP388 SPI Read Protocol**:
```
TX: [addr | 0x80] [dummy] [dummy...]
RX: [don't care]  [don't care] [data...]
```

**Fix Applied**:

```cpp
// BEFORE (broken):
int BMP388_SPI::get_reg(uint8_t addr, uint8_t *value)
{
    uint8_t cmd[2] = { (uint8_t)(addr | DIR_READ), 0};
    int ret = transfer(&cmd[0], &cmd[0], 2);
    *value = cmd[1];  // Reading dummy byte!
    return ret;
}

// AFTER (fixed):
int BMP388_SPI::get_reg(uint8_t addr, uint8_t *value)
{
    // BMP388 SPI read: TX [addr|0x80, dummy, dummy], RX [x, x, data]
    uint8_t cmd[3] = { (uint8_t)(addr | DIR_READ), 0, 0};
    int ret = transfer(&cmd[0], &cmd[0], 3);
    if (ret == OK) {
        *value = cmd[2];  // Data is in 3rd byte
    }
    return ret;
}
```

**Similar fix for `get_reg_buf()`**:
```cpp
// Transfer length changed from len+1 to len+2
// Data copied from rx[2] instead of rx[1]
const size_t transfer_len = static_cast<size_t>(len) + 2;
memcpy(buf, &rx[2], len);
```

**Struct modifications**:
```cpp
#pragma pack(push,1)
struct spi_data_s {
    uint8_t addr;
    uint8_t dummy;  // Added dummy byte field
    struct data_s data;
};
struct spi_calibration_s {
    uint8_t addr;
    uint8_t dummy;  // Added dummy byte field
    struct calibration_s cal;
};
#pragma pack(pop)
```

### 2. Board GPIO Configuration

**File**: `boards/microchip/samv71-xult-clickboards/src/board_config.h`

```c
/* ICM20689 on EXT1 header */
#define GPIO_SPI0_CS_ICM20689    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN25)
#define GPIO_SPI0_DRDY_ICM20689  (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_INT_FALLING|GPIO_PORT_PIOD|GPIO_PIN28)
#define GPIO_SPI0_DRDY_ICM20689_IRQ  SAM_IRQ_PD28

/* BMP388 on EXT2 header */
#define GPIO_SPI0_CS_BMP388      (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN27)

/* Reset pins for Click board adapters */
#define GPIO_EXT1_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN5)
#define GPIO_EXT2_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN24)
```

### 3. SPI Bus Device Registration

**File**: `boards/microchip/samv71-xult-clickboards/src/spi.cpp`

```cpp
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    {
        .devices = {
            make_spidev(DRV_IMU_DEVTYPE_ICM20689, GPIO_SPI0_CS_ICM20689, GPIO_SPI0_DRDY_ICM20689),
            make_spidev(DRV_BARO_DEVTYPE_BMP388, GPIO_SPI0_CS_BMP388),  // No DRDY for BMP388
        },
        .power_enable_gpio = 0,
        .bus = static_cast<int8_t>(SPI::Bus::SPI0),
        .is_external = false,
        .requires_locking = false,
    },
};
```

### 4. Sensor Startup Script

**File**: `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`

```sh
# ICM-20689 IMU - 6DOF IMU 6 Click (MIKROE-4044)
# SPI bus 1 (PX4 numbering), CS=PD25 (EXT1), DRDY=PD28
icm20689 start -s

# BMP388 Barometer - Pressure 5 Click (MIKROE-3566)
# SPI on EXT2 header, CS=PD27
bmp388 start -s
```

---

## Adding New SPI Sensors

### Step 1: Define GPIO Pins

In `board_config.h`, add CS and optionally DRDY pin definitions:

```c
/* New sensor on EXT1 or EXT2 */
#define GPIO_SPI0_CS_NEWSENSOR   (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOx|GPIO_PINy)
#define GPIO_SPI0_DRDY_NEWSENSOR (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_INT_FALLING|GPIO_PORT_PIOx|GPIO_PINz)
```

**Available CS pins**:
- EXT1: PD25 (currently used by ICM-20689)
- EXT2: PD27 (currently used by BMP388)
- mikroBUS Socket 1: Available
- mikroBUS Socket 2: Available

### Step 2: Register Device in SPI Bus

In `spi.cpp`, add the device to `px4_spi_buses`:

```cpp
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    {
        .devices = {
            make_spidev(DRV_IMU_DEVTYPE_ICM20689, GPIO_SPI0_CS_ICM20689, GPIO_SPI0_DRDY_ICM20689),
            make_spidev(DRV_BARO_DEVTYPE_BMP388, GPIO_SPI0_CS_BMP388),
            make_spidev(DRV_xxx_DEVTYPE_NEWSENSOR, GPIO_SPI0_CS_NEWSENSOR, GPIO_SPI0_DRDY_NEWSENSOR),
        },
        // ...
    },
};
```

Find device types in `src/drivers/drv_sensor.h`.

### Step 3: Add GPIO to Init List

In `board_config.h`, add to `PX4_GPIO_INIT_LIST`:

```c
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
    GPIO_SPI0_CS_BMP388,      \
    GPIO_SPI0_CS_NEWSENSOR,   \  /* Add new CS */
    GPIO_SPI0_DRDY_NEWSENSOR, \  /* Add new DRDY if used */
    // ...
}
```

### Step 4: Enable Driver in Kconfig/cmake

In `boards/microchip/samv71-xult-clickboards/default.px4board`:

```cmake
CONFIG_DRIVERS_BAROMETER_BMP388=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y
CONFIG_DRIVERS_XXX_NEWSENSOR=y   # Add new driver
```

### Step 5: Add Startup Command

In `init/rc.board_sensors`:

```sh
# New Sensor - Description
# CS=PD??, DRDY=PD??
newsensor start -s
```

### Step 6: Rebuild and Flash

```bash
make microchip_samv71-xult-clickboards_default
# Flash using OpenOCD or EDBG
```

---

## Known Issues and Limitations

### 1. DRDY Interrupt Not Working (ICM-20689)

**Symptom**: `icm20689 status` shows high "DRDY missed" count.

**Cause**: The Xplained Pro to mikroBUS adapter may not route the IRQ pin from the Click board to the EXT header IRQ pin.

**Impact**: Sensor works in FIFO polling mode instead of interrupt-driven mode. Slightly higher latency but functional.

**Workaround**: None required for basic operation. For optimal performance, verify physical connection of DRDY signal.

### 2. Single SPI Bus Limitation

All SPI sensors share SPI0. The PX4 SPI driver handles synchronization internally. If bus contention issues occur with high-frequency sensors, consider setting `requires_locking = true`.

### 3. BMP388 SPI Mode Specifics

The BMP388 requires specific SPI handling:
- Mode 0 (CPOL=0, CPHA=0)
- Dummy byte after address on reads
- Maximum SPI clock: 10 MHz

### 4. Extension Header Conflicts

- EXT1 and EXT2 share some Arduino header pins
- PA26 (TC0 CH2) conflicts with HSMCI0 DA2 (SD card) - cannot use for PWM

### 5. Reset Pin Management

Click board adapters may require RST pin to be held HIGH. The board initializes these in `init.c`:

```c
sam_configgpio(GPIO_EXT1_RST);  // PA5 - HIGH
sam_configgpio(GPIO_EXT2_RST);  // PA24 - HIGH
```

---

## Troubleshooting

### Sensor Not Detected (CHIP_ID = 0x00)

1. **Check SPI wiring**: Verify MOSI, MISO, SCK, CS connections
2. **Check CS pin configuration**: Ensure correct GPIO is defined in `board_config.h`
3. **Check device registration**: Verify device is in `px4_spi_buses` in `spi.cpp`
4. **Check driver SPI protocol**: Some sensors need dummy bytes (like BMP388)

### Sensor Detected But Wrong Values

1. **Check SPI mode**: Verify CPOL/CPHA settings match sensor datasheet
2. **Check SPI frequency**: Some sensors have maximum frequency limits
3. **Check byte ordering**: Verify endianness in data parsing

### Multiple Sensors Conflicting

1. **Check CS pin uniqueness**: Each sensor needs its own CS GPIO
2. **Enable bus locking**: Consider setting `requires_locking = true` in `px4_spi_buses` if contention occurs
3. **Check for pin conflicts**: Verify no GPIO overlap between sensors

### Debug Commands

```bash
# Check sensor status
icm20689 status
bmp388 status

# View published data
listener sensor_accel
listener sensor_gyro
listener sensor_baro

# Check SPI bus
spi status
```

---

## References

- [SAMV71-XULT User Guide](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-44003-32-bit-Cortex-M7-Microcontroller-SAM-V71-Xplained-Ultra_User-Guide.pdf)
- [BMP388 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp388/)
- [ICM-20689 Datasheet](https://invensense.tdk.com/products/motion-tracking/6-axis/icm-20689/)
- [PX4 Driver Development Guide](https://docs.px4.io/main/en/middleware/drivers.html)

---

## Changelog

- **2024-12**: Initial SPI sensor integration
  - Fixed BMP388 SPI dummy byte issue
  - Configured ICM-20689 on EXT1, BMP388 on EXT2
  - Documented hardware mapping and code changes

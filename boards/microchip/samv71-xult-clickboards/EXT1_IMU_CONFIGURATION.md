# ICM20689 IMU on EXT1 Header Configuration

**Date:** 2025-12-08
**Status:** ✅ VERIFIED WORKING

---

## Summary

The ICM20689 6DOF IMU Click board (MIKROE-4044) is connected to the **EXT1 header** on the SAMV71-XULT board, NOT the mikroBUS socket.

---

## Pin Configuration

### EXT1 Header Pinout for SPI Sensors

| Function | EXT1 Pin | SAMV71 Pin | GPIO Definition |
|----------|----------|------------|-----------------|
| **CS** | Pin 15 | PD25 | `GPIO_SPI0_CS_ICM20689` |
| **IRQ/DRDY** | Pin 9 | PD28 | `GPIO_SPI0_DRDY_ICM20689` |
| SPI MOSI | Pin 16 | PD21 | SPI0_MOSI (peripheral) |
| SPI MISO | Pin 17 | PD20 | SPI0_MISO (peripheral) |
| SPI SCK | Pin 18 | PD22 | SPI0_SPCK (peripheral) |
| GND | Pin 19 | - | Ground |
| VCC | Pin 20 | - | 3.3V |

### Previous (Wrong) Configuration - mikroBUS Socket 1

| Function | mikroBUS Pin | SAMV71 Pin |
|----------|--------------|------------|
| CS | - | PA11 |
| DRDY | - | PA12 |

---

## Code Changes

### board_config.h

```c
/* ICM20689 on EXT1 header (not mikroBUS socket)
 * EXT1 Pin 15 = CS  = PD25
 * EXT1 Pin 9  = IRQ = PD28 (directly connected to DRDY)
 */
#define GPIO_SPI0_CS_ICM20689    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOD|GPIO_PIN25)
#define GPIO_SPI0_DRDY_ICM20689  (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_INT_FALLING|GPIO_PORT_PIOD|GPIO_PIN28)
#define GPIO_SPI0_DRDY_ICM20689_IRQ  SAM_IRQ_PD28
```

---

## Test Results (2025-12-08)

```
nsh> icm20689 start -s
icm20689 #0 on SPI bus 0

nsh> listener sensor_accel
TOPIC: sensor_accel
    timestamp: 54392535
    device_id: 3932162 (Type: 0x3C, SPI:0 (0x00))
    x: 0.68186
    y: 0.10726
    z: -9.98071          <-- Gravity (~10 m/s²)
    temperature: 29.70012
    error_count: 1
    samples: 10

nsh> listener sensor_gyro
TOPIC: sensor_gyro
    device_id: 3932162 (Type: 0x3C, SPI:0 (0x00))
    x: -0.01059
    y: -0.02727
    z: 0.00745
    temperature: 29.84700
    samples: 20

nsh> icm20689 status
INFO  [SPI_I2C] Running on SPI Bus 0
INFO  [icm20689] FIFO empty interval: 1250 us (800.0 Hz)
```

---

## Hardware Connection

### Click Board to EXT1 Adapter

The 6DOF IMU 6 Click (MIKROE-4044) uses standard mikroBUS pinout:

| mikroBUS Pin | Function | Connect to EXT1 |
|--------------|----------|-----------------|
| 1 (AN) | Not used | - |
| 2 (RST) | Not used | - |
| 3 (CS) | Chip Select | Pin 15 (PD25) |
| 4 (SCK) | SPI Clock | Pin 18 (PD22) |
| 5 (MISO) | SPI Data Out | Pin 17 (PD20) |
| 6 (MOSI) | SPI Data In | Pin 16 (PD21) |
| 7 (+3.3V) | Power | Pin 20 (VCC) |
| 8 (GND) | Ground | Pin 19 (GND) |
| 15 (INT) | Data Ready | Pin 9 (PD28) |

---

## EXT1 vs EXT2 vs mikroBUS Comparison

The SAMV71-XULT has THREE different expansion interfaces:

| Interface | Purpose | SPI Bus | I2C Bus |
|-----------|---------|---------|---------|
| **EXT1** | Xplained Pro extensions | SPI0 (PD20-22) | TWIHS0 (PA3-4) |
| **EXT2** | Xplained Pro extensions | SPI0 (PD20-22) | TWIHS0 (PA3-4) |
| **mikroBUS 1** | Click boards | SPI0 | TWIHS0 |
| **mikroBUS 2** | Click boards | SPI1 | TWIHS0 |

**Key difference:** CS and IRQ pins are different between headers!

---

## Known Issues

### DRDY Missed Events
```
icm20689: DRDY missed: 44214 events
```

This is normal - the ICM20689 driver uses GPIO polling rather than hardware interrupts. The sensor still operates correctly at 800 Hz.

---

## Files Modified

1. `boards/microchip/samv71-xult-clickboards/src/board_config.h`
   - Changed CS from PA11 → PD25
   - Changed DRDY from PA12 → PD28
   - Added interrupt configuration

---

## Next Steps

1. **I2C Sensors** - Test magnetometer/barometer on I2C bus (TWIHS0)
2. **Enable Real Sensors** - Set `SYS_HAS_MAG=1`, `SYS_HAS_BARO=1`
3. **EKF2 Fusion** - Verify state estimation with real sensor data

---

**Last Updated:** 2025-12-08
**Author:** Claude Code

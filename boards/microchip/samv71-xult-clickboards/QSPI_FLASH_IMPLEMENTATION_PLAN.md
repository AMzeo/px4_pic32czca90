# QSPI Flash Implementation Plan for SAMV71-XULT

## Executive Summary

This document provides a detailed plan to implement QSPI flash storage on the SAMV71-XULT board. The board has an onboard **SST26VF064B 8MB QSPI flash** that will be used for:
- Parameter storage (`/fs/mtd_params`)
- Calibration data (`/fs/mtd_caldata`)
- Mission/waypoint storage (`/fs/mtd_waypoints`)
- Dataman storage (`/fs/mtd_dataman`)

---

## Why QSPI Flash?

### Current Limitation (SD Card Only)
```
ERROR [dataman] open '/fs/mtd_caldata' failed (2)
```
- Parameters stored on SD card (`/fs/microsd/params`)
- Missions stored in RAM (lost on reboot)
- No factory calibration protection
- Slower boot time (SD card init ~1-2s)

### Benefits of QSPI Flash

| Feature | SD Card | QSPI Flash |
|---------|---------|------------|
| **Speed** | ~10-20 MB/s | ~50-100 MB/s |
| **Boot Time** | ~1-2s init | ~100ms init |
| **Reliability** | Medium (can corrupt) | High (solid-state) |
| **Persistence** | Removable | Always present |
| **Write Endurance** | ~10K cycles | ~100K cycles |
| **Capacity** | GBs | 8MB (sufficient for params) |

### PX4 Features Enabled by QSPI Flash

1. **Persistent Missions** - Waypoints survive reboots
2. **Factory Calibration** - Protected from user reset
3. **Faster Parameter Access** - No SD card overhead
4. **Geofence Persistence** - Safety boundaries retained
5. **Rally Points** - Emergency landing sites persist
6. **Professional Grade** - Same as Pixhawk boards

---

## Hardware Specifications

### SST26VF064B QSPI Flash (On-board)

| Parameter | Value |
|-----------|-------|
| **Part Number** | SST26VF064B |
| **Capacity** | 8MB (64 Mbit) |
| **Interface** | QSPI (Quad SPI) |
| **Max Clock** | 104 MHz |
| **Erase Cycles** | 100,000 per sector |
| **Page Size** | 256 bytes |
| **Sector Size** | 4KB |
| **Block Size** | 64KB |

### QSPI Pin Assignment (Fixed on SAMV71-XULT)

| Signal | Pin | Peripheral | Function |
|--------|-----|------------|----------|
| QSPI_CS | PA11 | Peripheral A | Chip Select |
| QSPI_IO0 | PA13 | Peripheral A | MOSI/Data0 |
| QSPI_IO1 | PA12 | Peripheral A | MISO/Data1 |
| QSPI_IO2 | PA17 | Peripheral A | WP/Data2 |
| QSPI_IO3 | PD31 | Peripheral A | HOLD/Data3 |
| QSPI_SCK | PA14 | Peripheral A | Clock |

**Important:** These pins are dedicated to QSPI and cannot be used for other functions.

---

## MTD Partition Layout

### Proposed 8MB Flash Layout

```
/dev/qspiflash0 (8MB = 8,388,608 bytes)
├── Partition 0: /fs/mtd_params     (128KB) - System parameters
├── Partition 1: /fs/mtd_caldata    (64KB)  - Factory calibration
├── Partition 2: /fs/mtd_waypoints  (2MB)   - Mission waypoints
├── Partition 3: /fs/mtd_dataman    (4MB)   - Dataman storage
└── Reserved                        (~1.8MB) - Future use
```

### Partition Details

| Partition | Size | Blocks | Purpose |
|-----------|------|--------|---------|
| mtd_params | 128KB | 32 (4KB each) | PX4 parameters, survives firmware update |
| mtd_caldata | 64KB | 16 (4KB each) | Factory IMU/mag calibration |
| mtd_waypoints | 2MB | 512 (4KB each) | Mission, geofence, rally points |
| mtd_dataman | 4MB | 1024 (4KB each) | Dataman general storage |

---

## Implementation Plan

### Phase 1: NuttX QSPI Configuration (1-2 hours)

#### Step 1.1: Enable QSPI in defconfig

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

Add:
```
# QSPI Flash Support
CONFIG_SAMV7_QSPI=y
CONFIG_SAMV7_QSPI_DMA=y

# MTD Support
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_PARTITION=y

# SST26 Flash Driver
CONFIG_MTD_SST26=y
CONFIG_SST26_SPIMODE=0
CONFIG_SST26_SPIFREQUENCY=50000000
```

#### Step 1.2: Add QSPI Pin Definitions

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board.h`

Add:
```c
/* QSPI Pin Configuration for SST26VF064B Flash */
#define GPIO_QSPI0_CS     (GPIO_QSPI0_CS_1)    /* PA11 */
#define GPIO_QSPI0_IO0    (GPIO_QSPI0_IO0_1)   /* PA13 - MOSI */
#define GPIO_QSPI0_IO1    (GPIO_QSPI0_IO1_1)   /* PA12 - MISO */
#define GPIO_QSPI0_IO2    (GPIO_QSPI0_IO2_1)   /* PA17 - WP */
#define GPIO_QSPI0_IO3    (GPIO_QSPI0_IO3_1)   /* PD31 - HOLD */
#define GPIO_QSPI0_SCK    (GPIO_QSPI0_SCK_1)   /* PA14 */

/* QSPI Flash Configuration */
#define SAMV7_QSPI_FLASH_SIZE   (8 * 1024 * 1024)  /* 8MB */
#define SAMV7_QSPI_SECTOR_SIZE  (4 * 1024)         /* 4KB */
```

---

### Phase 2: Board Initialization (2-3 hours)

#### Step 2.1: Create QSPI Initialization

**File:** `boards/microchip/samv71-xult-clickboards/src/qspi.c` (new file)

```c
/****************************************************************************
 * SAMV71-XULT QSPI Flash Initialization
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/spi/qspi.h>
#include <nuttx/mtd/mtd.h>

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>

#include "sam_qspi.h"

#ifdef CONFIG_SAMV7_QSPI

static struct qspi_dev_s *g_qspi = NULL;
static struct mtd_dev_s *g_mtd = NULL;

/****************************************************************************
 * Name: samv71_qspi_initialize
 *
 * Description:
 *   Initialize QSPI flash and register MTD device
 *
 ****************************************************************************/
int samv71_qspi_initialize(void)
{
    int ret;

    /* Initialize QSPI peripheral */
    g_qspi = sam_qspi_initialize(0);
    if (!g_qspi) {
        PX4_ERR("QSPI0 initialization failed");
        return -ENODEV;
    }

    PX4_INFO("QSPI0 peripheral initialized");

    /* Initialize SST26 flash on QSPI */
    g_mtd = sst26_initialize_qspi(g_qspi);
    if (!g_mtd) {
        PX4_ERR("SST26 flash initialization failed");
        return -ENODEV;
    }

    /* Get flash geometry */
    struct mtd_geometry_s geo;
    ret = g_mtd->ioctl(g_mtd, MTDIOC_GEOMETRY, (unsigned long)&geo);
    if (ret < 0) {
        PX4_ERR("Failed to get MTD geometry: %d", ret);
        return ret;
    }

    PX4_INFO("SST26VF064B: %lu KB (%lu sectors of %lu bytes)",
             (unsigned long)(geo.neraseblocks * geo.erasesize / 1024),
             (unsigned long)geo.neraseblocks,
             (unsigned long)geo.erasesize);

    /* Register as /dev/mtdqspi */
    ret = register_mtddriver("/dev/mtdqspi", g_mtd, 0755, NULL);
    if (ret < 0) {
        PX4_ERR("MTD driver registration failed: %d", ret);
        return ret;
    }

    PX4_INFO("QSPI flash registered as /dev/mtdqspi");
    return OK;
}

#endif /* CONFIG_SAMV7_QSPI */
```

#### Step 2.2: Call from Board Init

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

Add to `board_app_initialize()`:
```c
#ifdef CONFIG_SAMV7_QSPI
    extern int samv71_qspi_initialize(void);
    ret = samv71_qspi_initialize();
    if (ret < 0) {
        syslog(LOG_WARNING, "QSPI flash init failed: %d\n", ret);
        /* Non-fatal - continue boot with SD card fallback */
    }
#endif
```

---

### Phase 3: MTD Manifest Configuration (2-3 hours)

#### Step 3.1: Create MTD Configuration

**File:** `boards/microchip/samv71-xult-clickboards/src/mtd.cpp` (new file)

```cpp
/****************************************************************************
 * SAMV71-XULT MTD Configuration
 *
 * Defines flash partitions for parameter, calibration, and mission storage
 ****************************************************************************/

#include <nuttx/config.h>
#include <board_config.h>
#include <nuttx/spi/spi.h>
#include <px4_platform_common/px4_manifest.h>

#ifdef CONFIG_SAMV7_QSPI

/* QSPI Flash device definition */
static const px4_mft_device_t qspi_flash_device = {
    .bus_type = px4_mft_device_t::QSPI,
    .devid    = 0  /* QSPI0 */
};

/* MTD partition layout for 8MB SST26VF064B
 *
 * Block size: 4KB (sector size of SST26)
 * Total blocks: 2048 (8MB / 4KB)
 *
 * Layout:
 *   Partition 0: params     - 32 blocks  (128KB)
 *   Partition 1: caldata    - 16 blocks  (64KB)
 *   Partition 2: waypoints  - 512 blocks (2MB)
 *   Partition 3: dataman    - 1024 blocks (4MB)
 *   Reserved                - 464 blocks (~1.8MB)
 */
static const px4_mtd_entry_t qspi_flash_config = {
    .device = &qspi_flash_device,
    .npart = 4,
    .partd = {
        {
            .type = MTD_PARAMETERS,
            .path = "/fs/mtd_params",
            .nblocks = 32  /* 128KB for parameters */
        },
        {
            .type = MTD_CALDATA,
            .path = "/fs/mtd_caldata",
            .nblocks = 16  /* 64KB for factory calibration */
        },
        {
            .type = MTD_WAYPOINTS,
            .path = "/fs/mtd_waypoints",
            .nblocks = 512  /* 2MB for missions/geofence/rally */
        },
        {
            .type = MTD_DATAMAN,
            .path = "/fs/mtd_dataman",
            .nblocks = 1024  /* 4MB for dataman */
        }
    },
};

static const px4_mtd_manifest_t board_mtd_config = {
    .nconfigs = 1,
    .entries = {
        &qspi_flash_config,
    }
};

static const px4_mft_entry_s mtd_mft = {
    .type = MTD,
    .pmft = (void *)&board_mtd_config,
};

static const px4_mft_s mft = {
    .nmft = 1,
    .mfts = {
        &mtd_mft,
    }
};

const px4_mft_s *board_get_manifest(void)
{
    return &mft;
}

#else /* !CONFIG_SAMV7_QSPI */

/* No QSPI - return empty manifest (use SD card) */
static const px4_mft_s empty_mft = {
    .nmft = 0,
    .mfts = { }
};

const px4_mft_s *board_get_manifest(void)
{
    return &empty_mft;
}

#endif /* CONFIG_SAMV7_QSPI */
```

#### Step 3.2: Update CMakeLists.txt

**File:** `boards/microchip/samv71-xult-clickboards/src/CMakeLists.txt`

Add mtd.cpp and qspi.c:
```cmake
px4_add_board_library(
    SRCS
        init.c
        led.c
        i2c.cpp
        spi.cpp
        sam_hsmci.c
        usb.c
        timer_config.cpp
        sam_gpiosetevent.c
        qspi.c       # NEW
        mtd.cpp      # NEW
)
```

---

### Phase 4: Board Configuration Updates (1 hour)

#### Step 4.1: Update board_config.h

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`

Add:
```c
/*
 * QSPI Flash Configuration
 */
#ifdef CONFIG_SAMV7_QSPI
#define BOARD_HAS_QSPI_FLASH        1
#define FLASH_BASED_PARAMS          1
#define FLASH_BASED_DATAMAN         1

/* QSPI Flash partition paths */
#define PARAM_STORAGE_PATH          "/fs/mtd_params"
#define CALDATA_STORAGE_PATH        "/fs/mtd_caldata"
#define MISSION_STORAGE_PATH        "/fs/mtd_waypoints"
#define DATAMAN_STORAGE_PATH        "/fs/mtd_dataman"
#endif
```

---

### Phase 5: Testing & Verification (2-3 hours)

#### Test 1: Build Verification

```bash
# Clean build
make clean
make microchip_samv71-xult-clickboards_default

# Check for QSPI-related symbols
arm-none-eabi-nm build/microchip_samv71-xult-clickboards_default/*.elf | grep -i qspi
```

#### Test 2: Boot Log Verification

After flashing, check boot messages:
```
nsh> dmesg | grep -i qspi
```

**Expected output:**
```
[init] QSPI0 peripheral initialized
[init] SST26VF064B: 8192 KB (2048 sectors of 4096 bytes)
[init] QSPI flash registered as /dev/mtdqspi
```

#### Test 3: MTD Device Verification

```bash
nsh> ls /dev/mtd*
/dev/mtdqspi      # Full QSPI flash
/dev/mtdqspi0     # Partition 0: params
/dev/mtdqspi1     # Partition 1: caldata
/dev/mtdqspi2     # Partition 2: waypoints
/dev/mtdqspi3     # Partition 3: dataman
```

#### Test 4: Parameter Storage Test

```bash
# Set a test parameter
nsh> param set TEST_QSPI 12345
nsh> param save

# Verify it was saved to QSPI (not SD)
nsh> param show TEST_QSPI
TEST_QSPI: 12345

# Reboot and verify persistence
nsh> reboot

# After reboot
nsh> param show TEST_QSPI
TEST_QSPI: 12345  # Should persist!
```

#### Test 5: Mission Persistence Test

```bash
# Upload a simple mission via QGroundControl
# Reboot the board
nsh> reboot

# Check if mission persists
nsh> dataman status
# Should show mission data available
```

#### Test 6: Filesystem Mount Check

```bash
nsh> mount
# Should show:
#   /fs/mtd_params type littlefs
#   /fs/mtd_caldata type littlefs (or rawfs)
#   /fs/mtd_waypoints type littlefs
```

---

## Troubleshooting Guide

### Issue: "QSPI0 initialization failed"

**Cause:** QSPI peripheral not enabled or pin conflict

**Solution:**
1. Verify `CONFIG_SAMV7_QSPI=y` in defconfig
2. Check no other peripheral using PA11-PA14, PA17, PD31
3. Verify clock is enabled for QSPI peripheral

### Issue: "SST26 flash initialization failed"

**Cause:** Flash chip not responding

**Solution:**
1. Check QSPI clock frequency (try lower: 25MHz)
2. Verify SST26 driver enabled: `CONFIG_MTD_SST26=y`
3. Check flash chip is properly soldered (hardware issue)

### Issue: Parameters not persisting

**Cause:** MTD partition not mounted or param system using SD

**Solution:**
1. Check `mount` output for `/fs/mtd_params`
2. Verify `FLASH_BASED_PARAMS` defined in board_config.h
3. Check boot log for MTD mount errors

### Issue: "No space left on device"

**Cause:** Partition too small

**Solution:**
1. Increase partition size in mtd.cpp
2. Erase flash and reformat: `mtd_erase /dev/mtdqspi0`

---

## File Summary

### Files to Create

| File | Description |
|------|-------------|
| `src/qspi.c` | QSPI peripheral and flash initialization |
| `src/mtd.cpp` | MTD partition manifest configuration |

### Files to Modify

| File | Changes |
|------|---------|
| `nuttx-config/nsh/defconfig` | Add QSPI, MTD, SST26 config |
| `nuttx-config/include/board.h` | Add QSPI pin definitions |
| `src/board_config.h` | Add QSPI flash defines |
| `src/init.c` | Call QSPI initialization |
| `src/CMakeLists.txt` | Add qspi.c, mtd.cpp to build |

---

## Estimated Time

| Phase | Description | Time |
|-------|-------------|------|
| 1 | NuttX QSPI configuration | 1-2 hours |
| 2 | Board initialization code | 2-3 hours |
| 3 | MTD manifest configuration | 2-3 hours |
| 4 | Board config updates | 1 hour |
| 5 | Testing & verification | 2-3 hours |
| **Total** | | **8-12 hours** |

---

## Dependencies

### Hardware
- SST26VF064B QSPI flash (already on SAMV71-XULT board)
- QSPI pins available (PA11-PA14, PA17, PD31)

### Software
- NuttX SAMV7 QSPI driver (`sam_qspi.c`)
- NuttX SST26 MTD driver (`drivers/mtd/sst26.c`)
- PX4 MTD manifest framework

### No Pin Conflicts
Current PWMC implementation uses PA7, PA2, PC19, PB0 - no conflict with QSPI pins.

---

## References

1. **SST26VF064B Datasheet**: https://www.microchip.com/en-us/product/SST26VF064B
2. **SAMV71 Datasheet Chapter 44**: QSPI peripheral
3. **SAMV71-XULT User Guide**: Board schematic and flash connection
4. **NuttX QSPI Driver**: `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_qspi.c`
5. **NuttX SST26 Driver**: `platforms/nuttx/NuttX/nuttx/drivers/mtd/sst26.c`
6. **PX4 MTD Framework**: `platforms/common/include/px4_platform_common/px4_manifest.h`

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-02-03 | 1.0 | Initial implementation plan |

---

## Appendix A: Quick Reference Commands

### Build Commands
```bash
make microchip_samv71-xult-clickboards_default
```

### Flash Commands
```bash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
    -c "adapter speed 4000" \
    -c "program build/microchip_samv71-xult-clickboards_default/*.bin 0x00400000 verify reset exit"
```

### NSH Test Commands
```bash
# Check QSPI status
dmesg | grep -i qspi
ls /dev/mtd*
mount

# Test parameters
param set TEST_QSPI 999
param save
reboot
param show TEST_QSPI

# Check dataman
dataman status

# MTD diagnostics
mtd_test /dev/mtdqspi0
```

---

## Appendix B: Rollback Plan

If QSPI implementation causes issues:

1. **Disable in defconfig:**
   ```
   # CONFIG_SAMV7_QSPI is not set
   ```

2. **Rebuild:**
   ```bash
   make clean
   make microchip_samv71-xult-clickboards_default
   ```

3. **Reflash:**
   System will automatically fall back to SD card storage.

The SD card fallback is always available as a safety net.

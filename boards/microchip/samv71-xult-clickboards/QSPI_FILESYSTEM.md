# QSPI Flash Filesystem — SAMV71-XULT

**Date:** 2026-02-10
**Status:** Working (LittleFS at `/mnt/qspi`, persistence verified)
**Supersedes:** `QSPI_FLASH_IMPLEMENTATION_PLAN.md` (stale — references wrong chip and wrong APIs)

---

## Hardware

| Parameter | Value |
|-----------|-------|
| **Chip** | **S25FL116K** (Spansion/Infineon) |
| **JEDEC ID** | `01 40 15` (manufacturer=01, memory_type=40, capacity=15) |
| **Capacity** | 2 MB (16 Mbit) |
| **Interface** | QSPI in SPI compatibility mode (single data line) |
| **SPI Clock** | 1 MHz (conservative; chip supports up to 104 MHz) |
| **Sector Size** | 4 KB |
| **Sectors** | 512 |
| **Erase Cycles** | 100,000 per sector |
| **Page Size** | 256 bytes |

**Important:** The SAMV71-XULT board has an **S25FL116K**, not SST26VF064B as documented in
the Atmel/Microchip user guide and older NuttX references. The S25FL116K uses the same JEDEC
command set as the Winbond W25Q series (memory type 0x40), so the NuttX W25 MTD driver works
without modification.

### QSPI Pins (Fixed on SAMV71-XULT, all Peripheral A)

| Signal | Pin | Function |
|--------|-----|----------|
| CS | PA11 | Chip Select (HW-managed by QSPI peripheral) |
| IO0/MOSI | PA13 | Data out |
| IO1/MISO | PA12 | Data in |
| IO2/WP | PA17 | Write Protect |
| IO3/HOLD | PD31 | Hold |
| SCK | PA14 | Clock |

**Custom PCB note:** PA12 and PA14 are shared with PWM1_H0 and PWM1_H1 (Periph C).
On the custom PCB, PWM1 channels must use alternate pins (PA30/PA31 Periph A) to avoid
conflict with QSPI.

---

## Software Architecture

```
sam_qspi_spi_initialize(0)   -> struct spi_dev_s*     (QSPI in SPI compat mode)
w25_initialize(spi)           -> struct mtd_dev_s*     (W25/S25FL1xx MTD driver)
register_mtddriver("/dev/mtdqspi")                     (NuttX VFS)
mount("/dev/mtdqspi", "/mnt/qspi", "littlefs", 0, "autoformat")
```

### Key Files

| File | Purpose |
|------|---------|
| `boards/microchip/samv71-xult-clickboards/src/qspi.c` | Board-level QSPI init, JEDEC probe, MTD registration |
| `boards/microchip/samv71-xult-clickboards/src/init.c` | Calls `board_qspi_flash_init()`, mounts LittleFS |
| `boards/microchip/samv71-xult-clickboards/src/board_config.h` | `BOARD_HAS_QSPI_FLASH` define |
| `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_qspi_spi.c` | NuttX QSPI SPI-mode driver (4 bugs fixed) |
| `platforms/nuttx/NuttX/nuttx/drivers/mtd/w25.c` | NuttX W25/S25FL1xx MTD driver |

### defconfig Settings

```
CONFIG_SAMV7_QSPI=y
CONFIG_SAMV7_QSPI_SPI_MODE=y
CONFIG_MTD_W25=y
CONFIG_W25_SPIMODE=0
CONFIG_W25_SPIFREQUENCY=1000000
CONFIG_FS_LITTLEFS=y
```

---

## NuttX Driver Bugs Fixed

Four bugs were found and fixed in `sam_qspi_spi.c` (the QSPI SPI-compatibility mode wrapper).
These are upstream NuttX bugs — the driver had never been tested end-to-end on real hardware.

### Bug 1: Swapped `qspi_putreg()` Arguments at WPCR

**Location:** `sam_qspi_spi.c` line ~810 (in `qspi_spi_lock()`)

```c
// BROKEN — arguments are (spi, offset, value) but caller passed (spi, value, offset):
qspi_putreg(spi, SAM_QSPI_WPCR_OFFSET, QSPI_WPCR_WPKEY);

// FIXED:
qspi_putreg(spi, QSPI_WPCR_WPKEY, SAM_QSPI_WPCR_OFFSET);
```

**Impact:** Wrote the WPKEY value to the QSPI memory-mapped flash window (0x80000000+offset),
causing an AHB bus hang. The board would lock up during `SPI_LOCK()`.

### Bug 2: CSMODE Left at NRELOAD

**Location:** `sam_qspi_spi.c` in `qspi_spi_setmode()`

The driver initialized CSMODE to NRELOAD (auto-reload CS between transfers). Combined with the
serialized `exchange()` loop that sends one byte at a time via `qspi_spi_send()`, this caused
CS to pulse between every byte of a multi-byte transaction.

**Fix:** Changed CSMODE to LASTXFER. CS stays asserted across the entire SELECT..DESELECT session
and only deasserts when LASTXFER is explicitly triggered.

### Bug 3: Missing LASTXFER Logic in exchange/select

**Location:** `sam_qspi_spi.c` in `qspi_spi_exchange()` and `qspi_spi_select()`

With CSMODE_LASTXFER, the driver needs to set the LASTXFER bit in CR on the last byte of an
exchange, but only if `SPI_SELECT(false)` (deselect) hasn't been called yet. The W25 MTD driver
does multi-step transactions: SELECT, SEND command, EXCHANGE data, DESELECT — so LASTXFER
must NOT fire during intermediate exchanges.

**Fix:** Added `escape_lastxfer` flag to the driver private struct:
- `SPI_SELECT(true)` sets `escape_lastxfer = true`
- `exchange()` checks: if last byte AND `escape_lastxfer == false`, write CR_LASTXFER
- `SPI_SELECT(false)` handles CS deassertion (see Bug 4)

### Bug 4: CS Deassertion — Dummy Byte Corrupts Flash Commands

**Location:** `sam_qspi_spi.c` in `qspi_spi_select()` (deselect path)

This was the hardest bug, requiring multiple iterations across two sessions.

**Problem:** In CSMODE_LASTXFER mode, LASTXFER only takes effect *during* an active transfer.
After the last real byte completes, writing LASTXFER to CR is a no-op — CS stays asserted.

**Failed approach — dummy byte:** Sending 0xFF to create a transfer for LASTXFER corrupts
single-byte flash commands like WREN (0x06). The S25FL116K datasheet requires CS to rise
**immediately** after the 8th bit of WREN. Any trailing byte invalidates the Write Enable
Latch, causing all subsequent erase/write operations to silently fail. Reads still work because
extra bytes just clock out more data, which masked the bug.

**Failed approach — CSS guard:** A status register check `if (SR & CSS) == 0` was used to
skip deassertion when CS was "already deasserted". But the **CSS polarity is inverted** from
what the NuttX header comment suggests: CSS=0 means CS IS asserted, CSS=1 means CS IS
deasserted. The guard was always skipping the deassertion path.

**Working fix — QSPI disable/re-enable cycle:**

```c
/* Wait for any in-flight transfer to complete */
while ((SR & TXEMPTY) == 0 && --timeout > 0);

/* Disable QSPI — CS goes high after transfer completion */
CR = QSPIDIS;
while ((SR & QSPIENS) != 0 && --timeout > 0);

/* Re-enable QSPI (MR/SCR preserved, CS starts deasserted) */
CR = QSPIEN;
while ((SR & QSPIENS) == 0 && --timeout > 0);
```

Writing QSPIDIS to CR forces CS high after any in-flight transfer completes. MR and SCR
configuration registers are preserved across the disable/enable cycle (only SWRST resets them).

---

## Boot Sequence

On a successful boot, the console shows:

```
[boot] QSPI flash init: S25FL116K via SPI mode
[boot] QSPI JEDEC probe: 01 40 15
[boot] QSPI flash: 2048 KB (512 sectors of 4096 bytes)
[boot] QSPI flash registered at /dev/mtdqspi
[boot] LittleFS mounted at /mnt/qspi
```

### Failure Modes (Non-Fatal)

If QSPI init fails, the board continues booting with SD card storage only. Failure at any step
prints an error and returns control to `board_app_initialize()`.

| Failure | Console Message | Cause |
|---------|----------------|-------|
| QSPI peripheral init | `QSPI SPI init failed` | Clock or pin config error |
| JEDEC probe FF/FF/FF | `QSPI flash not responding` | CS, clock, or wiring issue |
| JEDEC probe 00/00/00 | `QSPI flash not responding` | Bus stuck low |
| W25 driver reject | `W25/S25FL init failed` | JEDEC mismatch (unknown chip) |
| MTD registration | `register_mtddriver failed` | VFS error |
| LittleFS mount | `LittleFS mount failed` | Corrupt filesystem (will autoformat next boot) |

---

## Verified Capabilities

All tested at NSH prompt on 2026-02-10:

| Test | Result |
|------|--------|
| File create (`echo > /mnt/qspi/test.txt`) | Pass |
| File read (`cat /mnt/qspi/test.txt`) | Pass |
| Large file write (`dd if=/dev/zero bs=512 count=64`) | Pass (32 KB) |
| Directory create (`mkdir /mnt/qspi/subdir`) | Pass |
| Nested files (`echo > /mnt/qspi/subdir/nested.txt`) | Pass |
| Persistence across reboot | Pass (all files survive) |
| LittleFS autoformat (first boot on erased flash) | Pass |
| LittleFS mount existing (subsequent boots) | Pass |

---

## Build Notes

**Critical:** The PX4 build system does not track NuttX submodule source file changes. If you
modify any file under `platforms/nuttx/NuttX/nuttx/`, you **must** run `make clean` before
rebuilding. Otherwise the NuttX Makefile will re-archive cached `.o` files and your changes
will not take effect.

```bash
# After modifying NuttX sources (e.g., sam_qspi_spi.c):
make clean && make microchip_samv71-xult-clickboards_default
```

An incremental build (just `make`) will show only a few steps and produce an identical binary.

---

## Future Work

### MTD Partitions for PX4 Parameter/Dataman Storage

The current implementation mounts the entire 2 MB flash as a single LittleFS volume at
`/mnt/qspi`. For production use, the flash should be partitioned into dedicated MTD regions:

```
/dev/mtdqspi (2 MB)
+-- Partition 0: /fs/mtd_params     (128 KB)  - System parameters
+-- Partition 1: /fs/mtd_caldata    ( 64 KB)  - Factory calibration
+-- Partition 2: /fs/mtd_waypoints  (512 KB)  - Mission/geofence/rally
+-- Partition 3: /fs/mtd_dataman    (  1 MB)  - Dataman general storage
+-- Reserved                        (~320 KB)  - Future use
```

This requires implementing the PX4 MTD manifest (`board_get_manifest()`) and updating
`board_config.h` with `FLASH_BASED_PARAMS` / `FLASH_BASED_DATAMAN`.

Flight logs remain on SD card (continuous high-throughput writes, GB capacity needed).

### SPI Clock Speed

Currently running at 1 MHz for conservative operation. The S25FL116K supports up to 104 MHz.
Once the driver is proven stable, increasing to 25-50 MHz will improve read/write throughput
significantly.

---

## Rollback

Set `CONFIG_SAMV7_QSPI_SPI_MODE` to `n` in defconfig, rebuild. All QSPI code compiles out
cleanly. The board boots with SD card storage only.

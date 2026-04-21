# SQI Flash Filesystem — Implementation Reference

**Board:** PIC32CZ CA90 Curiosity Ultra (EV16W43A)
**Flash chip:** SST26VF032BAT-104I/SM — 4 MB, JEDEC BF 26 42, on SQI1

---

## What this doc covers

The NuttX MTD filesystem stack that exposes the SQI flash chip as three PX4 storage
partitions: `/fs/mtd_params`, `/fs/mtd_caldata`, `/fs/mtd_waypoints`. The stack is the
same across boards — only the hardware driver at the bottom changes.

---

## Flash Chip: SST26VF032BAT

| Property       | Value                          |
|----------------|-------------------------------|
| Manufacturer   | Microchip (SST)               |
| Part           | SST26VF032BAT-104I/SM         |
| Capacity       | 32 Mbit (4 MB)                |
| JEDEC ID       | `BF 26 42`                    |
| Interface      | SQI (quad SPI), also works in SPI mode |
| Erase unit     | 4096 bytes (4 KB sector)      |
| Page write     | 256 bytes                     |
| Total sectors  | 1024                          |
| Block protect  | ULBPR (`0x98`) required before first write |

**NuttX driver:** `drivers/mtd/sst26.c` — handles SST26VF016/032/064 via same command set.
Accepts a `spi_dev_s *` — works with any SPI-compatible hardware adapter.

---

## NuttX MTD Filesystem Stack

```
┌─────────────────────────────────────────────────────────┐
│  PX4 flight stack                                       │
│  param_load() / param_save() / dataman_open()           │
└───────────────────────┬─────────────────────────────────┘
                        │  character device path
                        │  /fs/mtd_params, /fs/mtd_caldata, /fs/mtd_waypoints
┌───────────────────────▼─────────────────────────────────┐
│  BCH (Block-to-Character) device                        │
│  bchdev_register("/dev/mtdblock0", "/fs/mtd_params")    │
│  Converts block device to character device              │
└───────────────────────┬─────────────────────────────────┘
                        │  block device  /dev/mtdblock0/1/2
┌───────────────────────▼─────────────────────────────────┐
│  FTL (Flash Translation Layer)                          │
│  ftl_initialize(minor=0, part_mtd)                      │
│  Wear leveling, bad block management, block I/O         │
└───────────────────────┬─────────────────────────────────┘
                        │  struct mtd_dev_s * (partition)
┌───────────────────────▼─────────────────────────────────┐
│  MTD Partition                                          │
│  mtd_partition(parent_mtd, first_block, num_blocks)     │
│  Sub-range view of the parent MTD device                │
└───────────────────────┬─────────────────────────────────┘
                        │  struct mtd_dev_s * (full device)
┌───────────────────────▼─────────────────────────────────┐
│  SST26 MTD Driver                                       │
│  sst26_initialize_spi(spi, devid)                       │
│  Implements read/write/erase using SST26 SPI commands   │
│  ULBPR unlock on first write (required for SST26)       │
└───────────────────────┬─────────────────────────────────┘
                        │  struct spi_dev_s *
┌───────────────────────▼─────────────────────────────────┐
│  SPI hardware adapter                                   │
│  CA90: sam_sqibus_initialize(1)  ← SQI1 BD-DMA engine  │
│  Implements SPI_LOCK / SPI_SETFREQUENCY / SPI_EXCHANGE  │
└───────────────────────┬─────────────────────────────────┘
                        │  hardware registers
┌───────────────────────▼─────────────────────────────────┐
│  SQI1 peripheral (CA90)                                 │
│  Base: 0x4F009000, GCLK_ID=57, MCLK_ID_AHB=67          │
│  Integrated BD-DMA engine (BDCTRL/BDNXT at offset 0x00) │
│  DFP: component/sqi.h, instance/sqi1.h                  │
└─────────────────────────────────────────────────────────┘
```

---

## Partition Layout

All offsets are in erase-sector units (1 sector = 4096 bytes).

| # | Mount point          | Offset (sectors) | Size (sectors) | Size (bytes) | PX4 type       |
|---|----------------------|------------------|----------------|--------------|----------------|
| 0 | `/fs/mtd_params`     | 0                | 32             | 128 KB       | `MTD_PARAMETERS` |
| 1 | `/fs/mtd_caldata`    | 32               | 16             | 64 KB        | `MTD_CALDATA`  |
| 2 | `/fs/mtd_waypoints`  | 48               | 128            | 512 KB       | `MTD_WAYPOINTS` |
|   | *(unused)*           | 176              | 848            | ~3.3 MB      | —              |

Total used: 704 KB out of 4 MB. Constants defined in `board_config.h`:

```c
#define QSPI_PART_PARAMS_OFFSET      0
#define QSPI_PART_PARAMS_SECTORS     32
#define QSPI_PART_CALDATA_OFFSET     32
#define QSPI_PART_CALDATA_SECTORS    16
#define QSPI_PART_WAYPOINTS_OFFSET   48
#define QSPI_PART_WAYPOINTS_SECTORS  128
#define QSPI_NUM_PARTITIONS          3
```

---

## Reference Implementation (CA70)

The CA70 `boards/microchip/pic32czca70-curiosity/src/qspi.c` implements this stack.
The full sequence follows. The CA90 implementation differs only in the hardware adapter
(`sam_sqibus_initialize(1)` instead of `sam_qspi_spi_initialize(0)`).

### Step 1 — Hardware adapter

```c
/* CA70: QSPI peripheral in SPI-compatibility mode */
struct spi_dev_s *spi = sam_qspi_spi_initialize(0);

/* CA90: SQI1 peripheral using BD-DMA engine */
struct spi_dev_s *spi = sam_sqibus_initialize(1);
```

Both return a `spi_dev_s *`. Everything above this line is identical between boards.

### Step 2 — JEDEC probe

Probe at 1 MHz before handing to the SST26 driver. If this returns `FF FF FF` or
`00 00 00`, the problem is wiring, clock, or CS — not the MTD driver.

```c
SPI_LOCK(spi, true);
SPI_SETFREQUENCY(spi, 1000000);
SPI_SETMODE(spi, SPIDEV_MODE0);
SPI_SETBITS(spi, 8);
SPI_SELECT(spi, SPIDEV_FLASH(0), true);
SPI_SEND(spi, 0x9f);          /* RDID command */
jedec[0] = SPI_SEND(spi, 0xff);
jedec[1] = SPI_SEND(spi, 0xff);
jedec[2] = SPI_SEND(spi, 0xff);
SPI_SELECT(spi, SPIDEV_FLASH(0), false);
SPI_LOCK(spi, false);
/* Expected: BF 26 42 for SST26VF032BAT */
```

### Step 3 — SST26 MTD driver

```c
struct mtd_dev_s *mtd = sst26_initialize_spi(spi, 0);
/* mtd == NULL → JEDEC mismatch or SPI error */
```

### Step 4 — Register full MTD device

```c
register_mtddriver("/dev/mtdqspi", mtd, 0755, NULL);
```

### Step 5 — Query geometry (for block-to-sector conversion)

```c
mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)&geo);
int blkpererase = geo.erasesize / geo.blocksize;
/* SST26VF032B: erasesize=4096, blocksize=256 → blkpererase=16 */
```

### Step 6 — Create partitions

For each partition (params, caldata, waypoints):

```c
off_t first_block = (off_t)offset_esect * blkpererase;
off_t num_blocks  = (off_t)size_esect   * blkpererase;

struct mtd_dev_s *part = mtd_partition(mtd, first_block, num_blocks);
ftl_initialize(minor, part);                  /* → /dev/mtdblockN */

char blkdev[20];
snprintf(blkdev, sizeof(blkdev), "/dev/mtdblock%d", minor);
bchdev_register(blkdev, mount_path, false);   /* → /fs/mtd_params etc. */
```

### Step 7 — Register with PX4 MTD

Required so `mft query -k MTD -s MTD_PARAMETERS` in rcS returns the correct device:

```c
static mtd_instance_s instance;
static int            block_counts[QSPI_NUM_PARTITIONS];
static int            types[QSPI_NUM_PARTITIONS];
static const char    *names[QSPI_NUM_PARTITIONS];

instance.mtd_dev                = mtd;
instance.partition_block_counts = block_counts;
instance.partition_types        = types;
instance.partition_names        = names;
instance.n_partitions_current   = QSPI_NUM_PARTITIONS;

for (int i = 0; i < QSPI_NUM_PARTITIONS; i++) {
    block_counts[i] = parts[i].size_esect * blkpererase;
    types[i]        = parts[i].mtd_type;   /* MTD_PARAMETERS, MTD_CALDATA, MTD_WAYPOINTS */
    names[i]        = parts[i].path;
}

px4_mtd_register_instance(&instance);
```

### Step 8 — Call order in `board_app_initialize()`

```c
/* SQI first — params available immediately on boot */
board_qspi_flash_init();
board_qspi_create_partitions(g_qspi_mtd);

/* SD card second */
board_sdcard_init();
```

---

## CA90 Implementation Differences

| Aspect                  | CA70 (reference)                   | CA90 (to implement)               |
|-------------------------|------------------------------------|-----------------------------------|
| Flash peripheral        | QSPI (SAMV7 IP)                    | SQI1 (CA90 IP, BD-DMA integrated) |
| Hardware init function  | `sam_qspi_spi_initialize(0)`       | `sam_sqibus_initialize(1)`        |
| Hardware driver file    | `sam_qspi_spi.c` (NuttX SAMV7)     | `sam_sqi.c` (write from scratch)  |
| DMA dependency          | XDMAC (external, SAMV7 system DMA) | **None** — BD-DMA is built into SQI1 |
| Flash chip              | SST26VF032B (same JEDEC BF 26 42)  | SST26VF032BAT (same)              |
| SST26 MTD driver        | `drivers/mtd/sst26.c`              | Same — unchanged                  |
| Partition layout        | params/caldata/waypoints           | Same — unchanged                  |
| MTD stack above SST26   | mtd_partition → ftl → bchdev       | Same — unchanged                  |
| PX4 MTD registration    | `px4_mtd_register_instance()`      | Same — unchanged                  |
| ULBPR unlock            | Required (SST26 block protection)  | Same — required                   |
| Board file              | `src/qspi.c`                       | `src/qspi.c` (same structure)     |
| Board config constants  | `QSPI_PART_*` in `board_config.h`  | Same constants, same values        |

**Key point:** Everything from `sst26_initialize_spi()` upward is reused without change.
Only `sam_sqi.c` (the SQI1 hardware driver) and the `sam_sqibus_initialize(1)` call
in `qspi.c` are CA90-specific.

---

## SQI1 Hardware: What sam_sqi.c Must Implement

The SQI1 peripheral exposes SPI-compatible transfers through a BD (Buffer Descriptor) chain.
The `spi_dev_s` wrapper translates standard SPI calls into BD operations.

### Key registers (DFP `component/sqi.h`, `instance/sqi1.h`)

| Register  | Offset | Purpose                                          |
|-----------|--------|--------------------------------------------------|
| `BDCTRL`  | 0x00   | BD control: BUFLEN, DIR, MODE, SPI_DEV_SEL, LIFM, LAST_BD |
| `BDNXT`   | 0x04   | Next BD pointer (for chained transfers)          |
| `SQICFG`  | 0x08   | TX/RX FIFO enable (TXBUFEN, RXBUFEN)             |
| `SPICTL`  | 0x0C   | SPI mode, CS select (SPIMODE, CSEN)              |
| `CLKCON`  | 0x10   | Clock divider (CLKDIV)                           |
| `INTSIG`  | 0x14   | Interrupt signal register                        |
| `INTEN`   | 0x18   | Interrupt enable                                 |

**SQI1 hardware details:**
- Base address: `0x4F009000` (DFP `instance/sqi1.h`)
- GCLK channel: 57 → route GCLK1 (150 MHz) at init
- MCLK AHB ID: 67 → enable before first register access
- No APB clock (SQI uses AHB only)
- BD descriptors must be in the nocache MPU region (`0x200F0000`, already reserved in linker)
- CS0 on PG3 (hardware-managed by SQI, no GPIO toggle needed in software)

### SPI wrapper interface

`sam_sqi.c` must implement these `spi_dev_s` operations so the SST26 MTD driver works:

```c
static const struct spi_ops_s g_sqi_ops = {
    .lock          = sqi_lock,
    .select        = sqi_select,       /* CS handled by hardware, stub OK */
    .setfrequency  = sqi_setfrequency, /* set CLKCON.CLKDIV */
    .setmode       = sqi_setmode,      /* SPI mode 0 for SST26 */
    .setbits       = sqi_setbits,      /* 8-bit only needed */
    .exchange      = sqi_exchange,     /* build BD, kick transfer, wait done */
    .send          = sqi_send,         /* single-byte exchange */
    .sndblock      = sqi_sndblock,     /* TX-only bulk */
    .recvblock     = sqi_recvblock,    /* RX-only bulk */
};
```

---

## ULBPR — Block Protection Unlock

The SST26VF032BAT ships with all blocks write-protected. The NuttX `sst26.c` driver
sends WREN (0x06) then ULBPR (0x98) on first use to unlock. This is transparent to
the board code — no explicit call needed. However, if the chip responds to JEDEC but
writes fail silently, confirm the SQI driver correctly asserts CS before and after the
two-command sequence (ULBPR requires its own CS cycle, not chained with other commands).

---

## Smoke Test Sequence

```
nsh> ls /fs/mtd_params
nsh> param set SYS_AUTOSTART 4001
nsh> param save
nsh> reboot
(after reboot)
nsh> param show SYS_AUTOSTART
  SYS_AUTOSTART: 4001 (saved)
nsh> ls /fs/mtd_caldata
nsh> ls /fs/mtd_waypoints
```

If `param show` returns the default value after reboot, the write is not persisting —
check ULBPR unlock, BD descriptor cache coherency, and GCLK/MCLK enable sequence.

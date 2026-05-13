/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file qspi.c
 *
 * PIC32CZ CA90 SQI1 flash filesystem initializer.
 *
 * Initializes the SST26VF032BAT SQI flash and mounts three MTD partitions:
 *   /fs/mtd_params     — 128 KB  parameter storage
 *   /fs/mtd_caldata    — 64 KB   calibration data
 *   /fs/mtd_waypoints  — 512 KB  dataman waypoints
 *
 * Call order (from board_app_initialize in init.c):
 *   1. board_qspi_flash_init()           — hardware bringup + JEDEC probe
 *   2. board_qspi_create_partitions()    — MTD stack + PX4 registration
 *
 * Reference: boards/microchip/pic32czca70-curiosity/src/qspi.c (structure)
 *            docs/sqi_filesystem.md (full stack walkthrough)
 *
 * Hardware: SQI1 base 0x4F009000, CS0=PG3, GCLK2=100 MHz
 *           SST26VF032BAT JEDEC: BF 26 42
 */

#define MODULE_NAME "qspi"

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <nuttx/config.h>

#ifdef CONFIG_PIC32CZCA90_SQI1

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <nuttx/spi/spi.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/drivers/drivers.h>
#include <sys/mount.h>

#include <px4_platform_common/px4_mtd.h>
#include <px4_platform_common/px4_manifest.h>
#include <px4_platform_common/mtd_manifest.h>

#include <nuttx/cache.h>

#include "board_config.h"

/* sam_sqibus_initialize, sam_sqi_xip_enable, sam_sqi_flash_cmd_write */
#include "sam_sqi.h"
#include "hardware/sam_sqi.h"
#include "arm_internal.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct qspi_part_s
{
  const char *path;        /* e.g. "/fs/mtd_params" */
  int         offset_esect; /* partition start in erase sectors */
  int         size_esect;   /* partition size in erase sectors */
  int         mtd_type;     /* MTD_PARAMETERS etc. */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct qspi_part_s g_qspi_parts[QSPI_NUM_PARTITIONS] =
{
  { "/fs/mtd_params",    QSPI_PART_PARAMS_OFFSET,    QSPI_PART_PARAMS_SECTORS,    QSPI_PART_PARAMS_TYPE    },
  { "/fs/mtd_caldata",   QSPI_PART_CALDATA_OFFSET,   QSPI_PART_CALDATA_SECTORS,   QSPI_PART_CALDATA_TYPE   },
  { "/fs/mtd_waypoints", QSPI_PART_WAYPOINTS_OFFSET, QSPI_PART_WAYPOINTS_SECTORS, QSPI_PART_WAYPOINTS_TYPE },
};

static struct mtd_dev_s *g_qspi_mtd = NULL;

/* PX4 MTD registration state */
static mtd_instance_s         g_mtd_instance;
static int                    g_block_counts[QSPI_NUM_PARTITIONS];
static int                    g_part_types[QSPI_NUM_PARTITIONS];
static const char            *g_part_names[QSPI_NUM_PARTITIONS];

/****************************************************************************
 * XIP reads — mode stays XIP at rest. sam_sqi_flash_cmd_write/rdsr handle
 * their own DMA↔XIP transitions.  Read functions just memcpy directly.
 ****************************************************************************/

/****************************************************************************
 * xip_word_copy — word-aligned copy from XIP window.
 *
 * Errata 2.12.2: "Byte and half-word transfers are not supported in XIP mode."
 * All reads from the SQI XIP window (0x90000000) MUST be 32-bit word-aligned.
 * memcpy uses byte copies and returns shifted/corrupt data on this silicon.
 ****************************************************************************/

static void xip_word_copy(uint8_t *dst, uintptr_t xip_addr, size_t nbytes)
{
  volatile uint32_t *src = (volatile uint32_t *)(xip_addr & ~3u);
  size_t align_offset = xip_addr & 3u;
  size_t i = 0;

  /* Handle unaligned start by reading the containing word */

  if (align_offset)
    {
      uint32_t w = *src++;
      uint8_t *wp = (uint8_t *)&w;
      while (align_offset < 4 && i < nbytes)
        {
          dst[i++] = wp[align_offset++];
        }
    }

  /* Bulk word-aligned copy */

  while (i + 4 <= nbytes)
    {
      uint32_t w = *src++;
      dst[i]     = (uint8_t)(w);
      dst[i + 1] = (uint8_t)(w >> 8);
      dst[i + 2] = (uint8_t)(w >> 16);
      dst[i + 3] = (uint8_t)(w >> 24);
      i += 4;
    }

  /* Handle trailing bytes */

  if (i < nbytes)
    {
      uint32_t w = *src;
      uint8_t *wp = (uint8_t *)&w;
      size_t j = 0;
      while (i < nbytes)
        {
          dst[i++] = wp[j++];
        }
    }
}

static ssize_t qspi_xip_read(FAR struct mtd_dev_s *dev, off_t offset,
                              size_t nbytes, FAR uint8_t *buffer)
{
  xip_word_copy(buffer, SAM_SQI1_XIP_BASE + (uintptr_t)offset, nbytes);
  return (ssize_t)nbytes;
}

static ssize_t qspi_xip_bread(FAR struct mtd_dev_s *dev, off_t startblock,
                               size_t nblocks, FAR uint8_t *buffer)
{
  off_t offset = startblock << 8;
  size_t nbytes = nblocks << 8;
  xip_word_copy(buffer, SAM_SQI1_XIP_BASE + (uintptr_t)offset, nbytes);
  return (ssize_t)nblocks;
}

/****************************************************************************
 * JEDEC probe helper
 *
 * Probe at 1 MHz before handing to the SST26 driver.
 * Returns 0 on success, -1 on unexpected JEDEC ID.
 ****************************************************************************/

static int qspi_jedec_probe(struct spi_dev_s *spi)
{
  uint8_t jedec[3];

  const uint8_t rdid = 0x9Fu;
  int ret;

  SPI_LOCK(spi, true);
  SPI_SETFREQUENCY(spi, 1000000);
  SPI_SETMODE(spi, SPIDEV_MODE0);
  SPI_SETBITS(spi, 8);

  /* Single linked TX→RX BD chain: CS held from command through response.
   * Two separate SPI_SEND/SPI_EXCHANGE calls would deassert CS between
   * them (BDCON=0 after each BD) — producing FF FF FF. */

  ret = sam_sqi_flash_cmd_read(spi, &rdid, 1, jedec, 3);
  SPI_LOCK(spi, false);

  if (ret < 0)
    {
      PX4_ERR("sam_sqi_flash_cmd_read failed: %d", ret);
      return -1;
    }

  PX4_INFO("SQI1 JEDEC: %02X %02X %02X", jedec[0], jedec[1], jedec[2]);

  if (jedec[0] != 0xBF || jedec[1] != 0x26 || jedec[2] != 0x42)
    {
      PX4_ERR("SQI1 JEDEC mismatch — expected BF 26 42 (SST26VF032BAT)");
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Custom MTD operations — direct BD-DMA for writes, XIP for reads.
 * Bypasses NuttX sst26.c entirely (SPI ops are incompatible with SQI BD-DMA).
 ****************************************************************************/

#define SST26_PAGE_SIZE     256u
#define SST26_PAGE_SHIFT    8u
#define SST26_SECTOR_SIZE   4096u
#define SST26_SECTOR_SHIFT  12u
#define SST26_TOTAL_SIZE    (4u * 1024u * 1024u)  /* 4 MB */
#define SST26_NSECTORS      (SST26_TOTAL_SIZE / SST26_SECTOR_SIZE)
#define SST26_NPAGES        (SST26_TOTAL_SIZE / SST26_PAGE_SIZE)

#define SST26_CMD_WREN      0x06u
#define SST26_CMD_WRDI      0x04u
#define SST26_CMD_RDSR      0x05u
#define SST26_CMD_SE        0x20u   /* 4 KB Sector Erase */
#define SST26_CMD_PP        0x02u   /* Page Program */
#define SST26_CMD_ULBPR     0x98u   /* Global Block Unlock */
#define SST26_SR_WIP        0x01u   /* Write In Progress bit */

static struct mtd_dev_s g_custom_mtd;

static int qspi_wait_write_complete(void)
{
  int status;
  uint32_t timeout = 1000000u;  /* ~100 ms at loop speed */

  do
    {
      status = sam_sqi_flash_rdsr();
      if (status < 0)
        {
          return -EIO;
        }

      if (--timeout == 0)
        {
          return -ETIMEDOUT;
        }
    }
  while (status & SST26_SR_WIP);

  return OK;
}

static int qspi_mtd_erase(FAR struct mtd_dev_s *dev, off_t startblock,
                           size_t nblocks)
{
  static uint8_t cmd[256];
  size_t i;


  /* Note: WBPR pre-erase removed — it poisoned BD state for subsequent SE */

  for (i = 0; i < nblocks; i++)
    {
      uint32_t addr = (uint32_t)(startblock + i) * SST26_SECTOR_SIZE;
      /* Use linked TX→TX BD chain: WREN(1B,LIFM) → SE(4B,LIFM).
       * Single DMA operation — no SWRST between WREN and SE. */

      /* Pad SE to 256 bytes — BD processor drops short (4-byte) TX BDs
       * from param save context but transmits long (260-byte) PP correctly.
       * Flash ignores trailing 0xFF after the 4-byte SE command. */

      memset(cmd, 0xFF, sizeof(cmd));
      cmd[0] = SST26_CMD_SE;
      cmd[1] = (addr >> 16) & 0xFF;
      cmd[2] = (addr >> 8) & 0xFF;
      cmd[3] = addr & 0xFF;
      sam_sqi_flash_wren_cmd(cmd, 256);

      int sr = sam_sqi_flash_rdsr();
      PX4_INFO("ERASE: addr=0x%06x RDSR=0x%02x (WIP=%d)",
               (unsigned)addr, sr, sr & 1);

      int ret = qspi_wait_write_complete();
      if (ret < 0)
        {
          sam_sqi_enter_xip();
          return ret;
        }

      /* Invalidate D-Cache for erased sector so XIP reads get fresh data */

      {
        uintptr_t a;
        for (a = (SAM_SQI1_XIP_BASE + addr) & ~31u;
             a < SAM_SQI1_XIP_BASE + addr + SST26_SECTOR_SIZE;
             a += 32)
          putreg32(a, 0xE000EF5Cu);  /* DCIMVAC */
        __asm__ volatile ("dsb sy" ::: "memory");
      }
    }

  sam_sqi_enter_xip();
  return (int)nblocks;
}

static ssize_t qspi_mtd_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
                                size_t nblocks, FAR const uint8_t *buffer)
{
  static uint8_t cmd[4 + SST26_PAGE_SIZE];  /* static: saves 260 bytes of LPWORK stack */
  size_t i;


  for (i = 0; i < nblocks; i++)
    {
      uint32_t addr = (uint32_t)(startblock + i) * SST26_PAGE_SIZE;
      uint8_t wren = SST26_CMD_WREN;

      /* Same pattern as sqi_test: WREN → RDSR(verify WEL) → PP */

      sam_sqi_flash_cmd_write(&wren, 1);

      int wel = sam_sqi_flash_rdsr();
      if (wel < 0 || !(wel & 0x02))
        {
          PX4_ERR("BWRITE: WEL not set after WREN (RDSR=0x%02x)", wel);
          sam_sqi_enter_xip();
          return -EIO;
        }

      cmd[0] = SST26_CMD_PP;
      cmd[1] = (addr >> 16) & 0xFF;
      cmd[2] = (addr >> 8) & 0xFF;
      cmd[3] = addr & 0xFF;
      memcpy(&cmd[4], &buffer[i * SST26_PAGE_SIZE], SST26_PAGE_SIZE);
      sam_sqi_flash_cmd_write(cmd, 4 + SST26_PAGE_SIZE);

      /* Wait for completion */

      int ret = qspi_wait_write_complete();
      if (ret < 0)
        {
          sam_sqi_enter_xip();
          return ret;
        }

      /* Invalidate D-Cache for written page so XIP reads get fresh data */

      {
        uintptr_t a;
        for (a = (SAM_SQI1_XIP_BASE + addr) & ~31u;
             a < SAM_SQI1_XIP_BASE + addr + SST26_PAGE_SIZE;
             a += 32)
          putreg32(a, 0xE000EF5Cu);  /* DCIMVAC */
        __asm__ volatile ("dsb sy" ::: "memory");
      }

    }

  sam_sqi_enter_xip();
  return (ssize_t)nblocks;
}

static int qspi_mtd_ioctl(FAR struct mtd_dev_s *dev, int cmd,
                           unsigned long arg)
{
  if (cmd == MTDIOC_GEOMETRY)
    {
      FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)arg;
      geo->blocksize    = SST26_PAGE_SIZE;
      geo->erasesize    = SST26_SECTOR_SIZE;
      geo->neraseblocks = SST26_NSECTORS;
      return OK;
    }

  if (cmd == MTDIOC_BULKERASE)
    {
      return qspi_mtd_erase(dev, 0, SST26_NSECTORS);
    }

  return -ENOTTY;
}

/****************************************************************************
 * board_qspi_flash_init
 *
 * Initializes SQI1 hardware, probes the flash, unlocks block protection,
 * enables XIP, and registers the custom MTD device.
 *
 * Returns 0 on success, negative errno on failure.
 ****************************************************************************/

int board_qspi_flash_init(void)
{
  struct spi_dev_s *spi;
  uint8_t cmd[1];

  /* Initialize SQI1 hardware */

  spi = sam_sqibus_initialize(1);
  if (spi == NULL)
    {
      PX4_ERR("sam_sqibus_initialize(1) failed");
      return -EIO;
    }

  /* JEDEC probe */

  if (qspi_jedec_probe(spi) < 0)
    {
      return -ENODEV;
    }

  /* Unlock all block protection: WREN + WBPR (0x42 + 18 zero bytes) */

  {
    uint8_t wbpr_cmd[19];
    memset(wbpr_cmd, 0, sizeof(wbpr_cmd));
    wbpr_cmd[0] = 0x42u;
    sam_sqi_flash_wren_cmd(wbpr_cmd, 19);
  }

  /* Verify unlock succeeded — if WEL still set, retry with separate calls */

  {
    int sr = sam_sqi_flash_rdsr();
    if (sr & 0x02)
      {
        cmd[0] = SST26_CMD_WREN;
        sam_sqi_flash_cmd_write(cmd, 1);
        cmd[0] = SST26_CMD_ULBPR;
        sam_sqi_flash_cmd_write(cmd, 1);
      }
  }

  /* Enable XIP for reads */

  sam_sqi_xip_enable();

  /* Dummy read to initialize XIP state machine — first read after enable
   * returns stale/zero data on this silicon. Discard the result. */

  {
    volatile uint32_t dummy = *(volatile uint32_t *)SAM_SQI1_XIP_BASE;
    (void)dummy;
  }

  /* Register custom MTD device */

  memset(&g_custom_mtd, 0, sizeof(g_custom_mtd));
  g_custom_mtd.erase  = qspi_mtd_erase;
  g_custom_mtd.bread  = qspi_xip_bread;
  g_custom_mtd.bwrite = qspi_mtd_bwrite;
  g_custom_mtd.read   = qspi_xip_read;
  g_custom_mtd.ioctl  = qspi_mtd_ioctl;
  g_custom_mtd.name   = "sst26";

  g_qspi_mtd = &g_custom_mtd;

  register_mtddriver("/dev/mtdqspi", &g_custom_mtd, 0755, NULL);

  PX4_INFO("SQI1 SST26VF032BAT ready (4 MB, XIP @ 0x%08x)",
           (unsigned)SAM_SQI1_XIP_BASE);
  return OK;
}

/****************************************************************************
 * board_qspi_create_partitions
 ****************************************************************************/

int board_qspi_create_partitions(struct mtd_dev_s *mtd)
{
  struct mtd_geometry_s geo;
  int blkpererase;
  int ret;
  int i;

  if (mtd == NULL)
    {
      mtd = g_qspi_mtd;
    }

  if (mtd == NULL)
    {
      PX4_ERR("board_qspi_create_partitions: no MTD device");
      return -EINVAL;
    }

  ret = mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)&geo);
  if (ret < 0)
    {
      PX4_ERR("MTDIOC_GEOMETRY failed: %d", ret);
      return ret;
    }

  blkpererase = geo.erasesize / geo.blocksize;

  for (i = 0; i < QSPI_NUM_PARTITIONS; i++)
    {
      const struct qspi_part_s *p = &g_qspi_parts[i];
      struct mtd_dev_s         *part;
      char                      blkdev[20];
      off_t                     first_block;
      off_t                     num_blocks;

      first_block = (off_t)p->offset_esect * blkpererase;
      num_blocks  = (off_t)p->size_esect   * blkpererase;

      part = mtd_partition(mtd, first_block, num_blocks);
      if (part == NULL)
        {
          PX4_ERR("mtd_partition[%d] failed", i);
          return -ENOMEM;
        }

      ret = ftl_initialize(i, part);
      if (ret < 0)
        {
          PX4_ERR("ftl_initialize[%d] failed: %d", i, ret);
          return ret;
        }

      snprintf(blkdev, sizeof(blkdev), "/dev/mtdblock%d", i);
      ret = bchdev_register(blkdev, p->path, false);
      if (ret < 0)
        {
          PX4_ERR("bchdev_register %s failed: %d", p->path, ret);
          return ret;
        }

      g_block_counts[i] = (int)(p->size_esect * blkpererase);
      g_part_types[i]   = p->mtd_type;
      g_part_names[i]   = p->path;

      PX4_INFO("MTD partition %d: %s (%d KB)",
               i, p->path, p->size_esect * 4);
    }

  memset(&g_mtd_instance, 0, sizeof(g_mtd_instance));
  g_mtd_instance.mtd_dev                = mtd;
  g_mtd_instance.partition_block_counts = g_block_counts;
  g_mtd_instance.partition_types        = g_part_types;
  g_mtd_instance.partition_names        = g_part_names;
  g_mtd_instance.n_partitions_current   = QSPI_NUM_PARTITIONS;

  px4_mtd_register_instance(&g_mtd_instance);
  return OK;
}

#endif /* CONFIG_PIC32CZCA90_SQI1 */

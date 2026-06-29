/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * boards/microchip/czca90curiosity/src/i2c_stats.c
 *
 * NSH command 'i2c_stats' — prints I2C5 ISR and DMA counters.
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include "sam_i2c_master.h"

__EXPORT int i2c_stats_main(int argc, char *argv[]);

int i2c_stats_main(int argc, char *argv[])
{
  sam_i2c_print_stats();
  return 0;
}

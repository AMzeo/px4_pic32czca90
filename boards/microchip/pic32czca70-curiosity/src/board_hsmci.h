/****************************************************************************
 * boards/microchip/samv71-xult-clickboards/src/board_hsmci.h
 *
 * Board-specific HSMCI header for SAMV71-XULT clickboards variant
 *
 ****************************************************************************/

#ifndef __BOARDS_MICROCHIP_SAMV71_XULT_CLICKBOARDS_SRC_BOARD_HSMCI_H
#define __BOARDS_MICROCHIP_SAMV71_XULT_CLICKBOARDS_SRC_BOARD_HSMCI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "sam_gpio.h"

/****************************************************************************
 * Public Functions Definitions
 ****************************************************************************/

/****************************************************************************
 * Name: sam_hsmci_initialize
 *
 * Description:
 *   Perform architecture specific initialization
 *
 ****************************************************************************/

int sam_hsmci_initialize(int slotno, int minor, gpio_pinset_t cdcfg,
                         int cdirq);

/****************************************************************************
 * Name: sam_cardinserted
 *
 * Description:
 *   Check if a card is inserted into the selected HSMCI slot
 *
 ****************************************************************************/

bool sam_cardinserted(int slotno);

/****************************************************************************
 * Name: sam_writeprotected
 *
 * Description:
 *   Check if the card in the MMCSD slot is write protected
 *
 ****************************************************************************/

bool sam_writeprotected(int slotno);

#endif /* __BOARDS_MICROCHIP_SAMV71_XULT_CLICKBOARDS_SRC_BOARD_HSMCI_H */

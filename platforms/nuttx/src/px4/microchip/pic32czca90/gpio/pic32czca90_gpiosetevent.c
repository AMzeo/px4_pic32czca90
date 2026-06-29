/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/****************************************************************************
 * PIC32CZ CA90 GPIO interrupt support via EIC (External Interrupt Controller).
 *
 * On CA90, each EXTINT channel has its own dedicated NVIC vector —
 * no software demux needed (unlike SAMV7's per-port PIO IRQ).
 *
 * Pin → EXTINT mapping: eirq = pin_number % 16 (function A on all ports).
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "arm_internal.h"
#include "sam_port.h"
#include "hardware/sam_pinmap.h"
#include "hardware/sam_eic.h"

#ifdef CONFIG_PIC32CZCA90_EIC
#include "sam_eic.h"
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct eic_handler_s
{
  xcpt_t   func;
  void    *arg;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct eic_handler_s g_eic_handlers[EIC_NEXTINT];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int eic_interrupt(int irq, void *context, void *arg)
{
#ifdef CONFIG_PIC32CZCA90_EIC
  int eirq = irq - SAM_IRQ_EXTINT0;

  if (eirq >= 0 && eirq < EIC_NEXTINT)
    {
      sam_eic_irq_ack(irq);

      if (g_eic_handlers[eirq].func != NULL)
        {
          return g_eic_handlers[eirq].func(irq, context,
                                           g_eic_handlers[eirq].arg);
        }
    }
#endif

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pic32czca90_gpiosetevent
 *
 * Description:
 *   Configure a GPIO pin as an EIC interrupt source.
 *
 *   The pin's EXTINT channel is derived from its pin number (% 16).
 *   Function A mux is applied to route the pin to EIC.
 *
 * Input Parameters:
 *   pinset      - PX4 GPIO pin encoding (port_pinset_t)
 *   risingedge  - Enable rising edge detection
 *   fallingedge - Enable falling edge detection
 *   event       - (unused on CA90, kept for API compat)
 *   func        - ISR callback, or NULL to disable
 *   arg         - Callback argument
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 ****************************************************************************/

int pic32czca90_gpiosetevent(uint32_t pinset, bool risingedge,
                             bool fallingedge, bool event,
                             xcpt_t func, void *arg)
{
#ifndef CONFIG_PIC32CZCA90_EIC
  return -ENOSYS;
#else
  uint8_t pin;
  uint8_t eirq;
  uint8_t sense;
  int irq;
  irqstate_t flags;

  (void)event;

  pin  = (pinset >> PORT_PIN_SHIFT) & 0x1f;
  eirq = pin % EIC_NEXTINT;
  irq  = SAM_IRQ_EXTINT0 + eirq;

  flags = enter_critical_section();

  if (func == NULL)
    {
      /* Disable: detach NVIC, disable EIC channel */

      up_disable_irq(irq);
      irq_detach(irq);
      sam_eic_disable(eirq);

      g_eic_handlers[eirq].func = NULL;
      g_eic_handlers[eirq].arg  = NULL;
    }
  else
    {
      /* Configure pin mux to EIC function A (value 0) with input enabled */

      uint32_t eic_pinset = (pinset & (0x7u << 28)) |  /* keep port */
                            PORT_FUNC(0) |              /* function A = EIC */
                            ((uint32_t)pin << PORT_PIN_SHIFT) |
                            PORT_FLAG_PMUXEN |
                            PORT_FLAG_INEN;

      sam_portconfig(eic_pinset);

      /* Determine edge sense */

      if (risingedge && fallingedge)
        {
          sense = EIC_SENSE_BOTH;
        }
      else if (risingedge)
        {
          sense = EIC_SENSE_RISE;
        }
      else if (fallingedge)
        {
          sense = EIC_SENSE_FALL;
        }
      else
        {
          leave_critical_section(flags);
          return -EINVAL;
        }

      /* Store handler before enabling */

      g_eic_handlers[eirq].func = func;
      g_eic_handlers[eirq].arg  = arg;

      /* Configure EIC channel (async mode, no filter) */

      sam_eic_configure(eirq, sense, false);

      /* Attach and enable NVIC */

      irq_attach(irq, eic_interrupt, NULL);
      up_enable_irq(irq);
    }

  leave_critical_section(flags);
  return OK;
#endif
}

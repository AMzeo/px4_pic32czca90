/****************************************************************************
 * USB diagnostic command — reads USBHS0 registers to verify PHY state.
 * Usage: nsh> usb_diag
 *        nsh> usb_diag trace    (dump EP0 event trace)
 *        nsh> usb_diag session  (set SESSION bit)
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern volatile uint32_t *sam_usb_dbg_counters(void);
extern uint8_t *sam_usb_last_setup(void);

/* EP0 trace ring exported from sam_usb.c */

struct ep0_trace_entry
{
  uint8_t evt;
  uint8_t csr0l;
  uint8_t state;
  uint8_t data;
};

#define EP0_TRACE_SIZE 32
extern struct ep0_trace_entry *sam_usb_ep0_trace(void);
extern uint8_t sam_usb_ep0_trace_idx(void);
extern void sam_usb_ep3_sizes(volatile uint8_t **sizes, volatile uint8_t **stx,
                              volatile uint8_t **idx);

#define USBHS0_BASE       0x4f010000u

#define REG32(off)  (*(volatile uint32_t *)(USBHS0_BASE + (off)))
#define REG8(off)   (*(volatile uint8_t *)(USBHS0_BASE + (off)))
#define REG16(off)  (*(volatile uint16_t *)(USBHS0_BASE + (off)))

static const char *evt_name(uint8_t evt)
{
  switch (evt)
    {
      case 0x01: return "ENTER";
      case 0x02: return "STALL";
      case 0x03: return "SETUPEND";
      case 0x04: return "SETUP";
      case 0x05: return "TX_START";
      case 0x06: return "TX_DONE";
      case 0x07: return "TX_CONT";
      case 0x08: return "ADDR";
      case 0x09: return "CLASS";
      case 0x0A: return "SUBMIT";
      case 0x0B: return "NOREQ";
      case 0x0C: return "RESET";
      case 0x0D: return "H2D";
      case 0x0E: return "H2D_RX";
      case 0x0F: return "H2D_NORQ";
      default:   return "?";
    }
}

static const char *state_name(uint8_t st)
{
  switch (st)
    {
      case 0: return "IDLE";
      case 1: return "SETUP";
      case 2: return "TX";
      case 3: return "TX_LAST";
      case 4: return "RX";
      case 5: return "STAT_IN";
      case 6: return "STAT_OUT";
      case 7: return "STALL";
      default: return "?";
    }
}

int usb_diag_main(int argc, char *argv[])
{
  /* If argument "session" is passed, set SESSION bit in DEVCTL */
  if (argc > 1 && strcmp(argv[1], "session") == 0)
    {
      uint8_t devctl = REG8(0x1060);
      devctl |= 0x01;
      REG8(0x1060) = devctl;
      printf("SESSION bit set in DEVCTL\n");
      for (volatile int i = 0; i < 1000000; i++);
    }

  /* If argument "trace" is passed, dump EP0 trace ring */
  if (argc > 1 && strcmp(argv[1], "trace") == 0)
    {
      struct ep0_trace_entry *trace = sam_usb_ep0_trace();
      uint8_t idx = sam_usb_ep0_trace_idx();
      uint8_t count = idx < EP0_TRACE_SIZE ? idx : EP0_TRACE_SIZE;
      uint8_t start = idx < EP0_TRACE_SIZE ? 0 :
                      idx % EP0_TRACE_SIZE;

      printf("=== EP0 Trace (%d entries, idx=%d) ===\n", count, idx);
      printf(" #  EVT       CSR0L ST       DATA\n");

      for (int i = 0; i < count; i++)
        {
          uint8_t ri = (start + i) % EP0_TRACE_SIZE;
          struct ep0_trace_entry *e = &trace[ri];
          printf("%2d  %-9s 0x%02x  %-8s 0x%02x\n",
                 i, evt_name(e->evt), e->csr0l,
                 state_name(e->state), e->data);
        }

      return 0;
    }

  /* If argument "tty" is passed, test open+tcgetattr on /dev/ttyACM0 */
  if (argc > 1 && strcmp(argv[1], "tty") == 0)
    {
      #include <fcntl.h>
      #include <termios.h>
      #include <errno.h>
      int fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
      printf("open(/dev/ttyACM0) = %d (errno=%d)\n", fd, errno);
      if (fd >= 0)
        {
          struct termios t;
          int ret = tcgetattr(fd, &t);
          printf("tcgetattr() = %d (errno=%d)\n", ret, errno);
          if (ret == 0)
            {
              printf("  c_iflag=0x%x c_oflag=0x%x c_lflag=0x%x c_cflag=0x%x\n",
                     t.c_iflag, t.c_oflag, t.c_lflag, t.c_cflag);
            }
          close(fd);
        }
      return 0;
    }

  /* If argument "ep3sizes" is passed, show last 16 EP3 packet sizes */
  if (argc > 1 && strcmp(argv[1], "ep3sizes") == 0)
    {
      volatile uint8_t *sizes;
      volatile uint8_t *stx;
      volatile uint8_t *widx;
      sam_usb_ep3_sizes(&sizes, &stx, &widx);
      uint8_t idx = *widx;
      uint8_t count = idx < 16 ? idx : 16;
      uint8_t start = idx < 16 ? 0 : idx & 0x0f;

      printf("=== Last %d EP3 packets (newest last) ===\n", count);
      printf("  #   size  stx   valid?\n");
      for (int i = 0; i < count; i++)
        {
          uint8_t ri = (start + i) & 0x0f;
          uint8_t s  = sizes[ri];
          uint8_t b  = stx[ri];
          printf("  [%2d] %3d   0x%02x  %s\n",
                 i, (int)s, (int)b,
                 (b == 0xfd) ? "MAVLink2" :
                 (b == 0xfe) ? "MAVLink1" : "INVALID");
        }
      printf("(35+0xfd = PARAM_SET MAVLink2; 21+0xfd = heartbeat)\n");
      return 0;
    }

  /* If argument "ep" is passed, dump EP1-3 FIFO config */
  if (argc > 1 && strcmp(argv[1], "ep") == 0)
    {
      printf("=== EPn FIFO Config ===\n");
      printf(" EP  DIR  FIFOSZ  FIFOADD  MAXP  CSRL  CSRH\n");
      for (int ep = 1; ep <= 3; ep++)
        {
          REG8(0x100e) = ep;  /* INDEX = ep */
          printf(" %d   TX   0x%02x    0x%04x   %4d  0x%02x  0x%02x\n",
                 ep,
                 REG8(0x1062), REG16(0x1064),
                 REG16(0x1010),
                 REG8(0x1012), REG8(0x1013));
          printf(" %d   RX   0x%02x    0x%04x   %4d  0x%02x  0x%02x\n",
                 ep,
                 REG8(0x1063), REG16(0x1066),
                 REG16(0x1014),
                 REG8(0x1016), REG8(0x1017));
        }

      return 0;
    }

  printf("=== USBHS0 Register Dump ===\n");
  printf("CTRLA     (0x00) = 0x%08lx\n", (unsigned long)REG32(0x0000));
  printf("INTENSET  (0x10) = 0x%08lx\n", (unsigned long)REG32(0x0010));
  printf("INTFLAG   (0x14) = 0x%08lx\n", (unsigned long)REG32(0x0014));
  printf("STATUS    (0x18) = 0x%08lx\n", (unsigned long)REG32(0x0018));
  printf("SYNCBUSY  (0x1C) = 0x%08lx\n", (unsigned long)REG32(0x001c));
  printf("\n--- MUSB Core (base+0x1000) ---\n");
  printf("FADDR     (0x1000) = 0x%02x\n", REG8(0x1000));
  printf("POWER     (0x1001) = 0x%02x\n", REG8(0x1001));
  printf("INTRTXE   (0x1006) = 0x%04x\n", REG16(0x1006));
  printf("INTRRXE   (0x1008) = 0x%04x\n", REG16(0x1008));
  printf("INTRUSBE  (0x100B) = 0x%02x\n", REG8(0x100b));
  printf("DEVCTL    (0x1060) = 0x%02x\n", REG8(0x1060));
  printf("EPINFO    (0x1078) = 0x%02x\n", REG8(0x1078));
  printf("RAMINFO   (0x1079) = 0x%02x\n", REG8(0x1079));
  printf("\n--- SUPC/OSCCTRL ---\n");
  printf("OSCCTRL STATUS   = 0x%08lx\n", (unsigned long)(*(volatile uint32_t *)0x44040010u));
  printf("OSCCTRL XOSCCTRLA= 0x%08lx\n", (unsigned long)(*(volatile uint32_t *)0x44040014u));
  printf("SUPC VREGCTRL    = 0x%08lx\n", (unsigned long)(*(volatile uint32_t *)0x4402001cu));
  printf("SUPC STATUS      = 0x%08lx\n", (unsigned long)(*(volatile uint32_t *)0x4402000cu));

  /* Read debug counters from driver */
  volatile uint32_t *dbg = sam_usb_dbg_counters();
  printf("\n--- EP0 FIFO Config (INDEX=0) ---\n");
  REG8(0x100e) = 0;  /* INDEX = 0 */
  printf("  TXFIFOSZ  (0x1062) = 0x%02x\n", REG8(0x1062));
  printf("  RXFIFOSZ  (0x1063) = 0x%02x\n", REG8(0x1063));
  printf("  TXFIFOADD (0x1064) = 0x%04x\n", REG16(0x1064));
  printf("  RXFIFOADD (0x1066) = 0x%04x\n", REG16(0x1066));
  printf("  CSR0L     (0x1012) = 0x%02x\n", REG8(0x1012));
  printf("  CSR0H     (0x1013) = 0x%02x\n", REG8(0x1013));
  printf("  COUNT0    (0x1018) = 0x%02x\n", REG8(0x1018));

  printf("\n--- Last SETUP packet ---\n");
  uint8_t *sp = sam_usb_last_setup();
  printf("  raw: %02x %02x %02x %02x %02x %02x %02x %02x\n",
         sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
  printf("  bmReqType=%02x bReq=%02x wVal=%04x wIdx=%04x wLen=%04x\n",
         sp[0], sp[1], sp[2]|(sp[3]<<8), sp[4]|(sp[5]<<8), sp[6]|(sp[7]<<8));

  printf("\n--- DMA Channel Status ---\n");
  for (int ch = 1; ch <= 4; ch++)
    {
      uint32_t cntl = REG32(0x1204 + (ch-1)*0x10);
      uint32_t addr = REG32(0x1208 + (ch-1)*0x10);
      uint32_t count = REG32(0x120c + (ch-1)*0x10);
      if (cntl || addr || count)
        {
          printf("  DMA%d: CNTL=0x%08lx ADDR=0x%08lx COUNT=%lu\n",
                 ch, (unsigned long)cntl, (unsigned long)addr,
                 (unsigned long)count);
        }
    }
  printf("  DMAINTR (0x1200) = 0x%02x\n", REG8(0x1200));
  printf("  INTFLAG (wrapper) = 0x%08lx\n", (unsigned long)REG32(0x0014));
  printf("  INTENSET (wrapper) = 0x%08lx\n", (unsigned long)REG32(0x0010));

  printf("\n--- ISR Debug Counters ---\n");
  printf("  isr=%lu reset=%lu ep0=%lu setup=%lu suspend=%lu ep0tx_noreq=%lu\n",
         (unsigned long)dbg[0], (unsigned long)dbg[1],
         (unsigned long)dbg[2], (unsigned long)dbg[3],
         (unsigned long)dbg[4], (unsigned long)dbg[5]);
  printf("  epn_rx=%lu rx_noreq=%lu rx_bytes=%lu epn_tx=%lu tx_done=%lu\n",
         (unsigned long)dbg[6], (unsigned long)dbg[7],
         (unsigned long)dbg[8], (unsigned long)dbg[20],
         (unsigned long)dbg[21]);
  printf("  ep3_rx=%lu ep3_bytes=%lu\n",
         (unsigned long)dbg[9], (unsigned long)dbg[10]);

  printf("\n--- Interpretation ---\n");
  uint32_t status = REG32(0x0018);
  printf("  PHYRDY=%d PHYON=%d VREGRDY=%d\n",
         !!(status & 1), !!(status & 2), !!(status & 4));
  uint8_t power = REG8(0x1001);
  printf("  SOFTCONN=%d HSENABLE=%d HSMODE=%d RESET=%d\n",
         !!(power & 0x40), !!(power & 0x20), !!(power & 0x10), !!(power & 0x08));
  uint8_t devctl = REG8(0x1060);
  printf("  VBUS[4:3]=%d BDEVICE=%d HOSTMODE=%d SESSION=%d\n",
         (devctl >> 3) & 3, !!(devctl & 0x80), !!(devctl & 0x04), !!(devctl & 0x01));
  uint32_t oscstatus = *(volatile uint32_t *)0x44040010u;
  printf("  XOSCRDY=%d PLL0LOCK=%d\n",
         !!(oscstatus & 1), !!(oscstatus & (1u << 24)));

  return 0;
}

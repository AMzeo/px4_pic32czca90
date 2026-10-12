/****************************************************************************
 * boards/microchip/czca90curiosity/src/sbus_test.c
 *
 * NSH command: sbus_test
 *
 * Reads SERCOM4 CTRLA hardware register to confirm whether RXINV bit is
 * set, then dumps raw bytes from /dev/ttyS2 and counts 0x0F occurrences
 * (SBUS start byte).
 *
 * How to use:
 *   nsh> rc_input stop         # release ttyS2 first
 *   nsh> sbus_test             # run with SBUS signal connected
 *
 * Expected output:
 *   With RXINV working:     0x0F appears every ~25 bytes (valid SBUS frames)
 *   With RXINV not working: 0x0F never appears; bytes are garbled inversions
 *
 * RXINV silicon bug evidence:
 *   If CTRLA bit10=1 (register confirms bit is set) but 0x0F count=0
 *   (hardware ignores the bit) -> silicon bug confirmed.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <px4_platform_common/log.h>
#include "hardware/pic32czca90_memorymap.h"

#define SERCOM4_CTRLA_ADDR   (SAM_SERCOM4_BASE + 0x00U)
#define RXINV_BIT            (1U << 10)
#define SBUS_START_BYTE      0x0FU
#define COLLECT_BYTES        75    /* ~3 full SBUS frames */
#define TIMEOUT_MS           3000

__EXPORT int sbus_test_main(int argc, char *argv[]);

int sbus_test_main(int argc, char *argv[])
{
	/* Step 1: open ttyS2 raw -- this causes the driver to re-enable SERCOM4 */
	int fd = open("/dev/ttyS2", O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		PX4_ERR("open /dev/ttyS2 failed (%d) -- run 'rc_input stop' first", errno);
		return 1;
	}

	struct termios tio;
	tcgetattr(fd, &tio);
	cfmakeraw(&tio);
	tcsetattr(fd, TCSANOW, &tio);

	/* Step 2: read CTRLA after driver has re-initialized SERCOM4 */
	uint32_t ctrla = *(volatile uint32_t *)SERCOM4_CTRLA_ADDR;
	PX4_INFO("SERCOM4 CTRLA = 0x%08" PRIx32, ctrla);
	PX4_INFO("  bit10 RXINV = %u  (1 = bit IS set in register)", (unsigned)((ctrla >> 10) & 1u));

	/* drain stale bytes */
	{
		uint8_t tmp[64];
		while (read(fd, tmp, sizeof(tmp)) > 0);
	}

	/* Step 3: collect bytes with timeout */
	uint8_t buf[COLLECT_BYTES];
	int total   = 0;
	int waited  = 0;

	while (total < COLLECT_BYTES && waited < TIMEOUT_MS) {
		int n = read(fd, buf + total, COLLECT_BYTES - total);

		if (n > 0) {
			total += n;

		} else {
			usleep(10000);   /* 10 ms */
			waited += 10;
		}
	}

	close(fd);

	if (total == 0) {
		PX4_ERR("no bytes received in %d ms (SBUS signal present?)", TIMEOUT_MS);
		return 1;
	}

	/* Step 4: hex dump */
	PX4_INFO("--- %d raw bytes from ttyS2 ---", total);

	for (int i = 0; i < total; i += 16) {
		char line[64];
		int  p = 0;

		for (int j = i; j < total && j < i + 16; j++) {
			p += snprintf(line + p, sizeof(line) - p, "%02X ", buf[j]);
		}

		PX4_INFO("  %s", line);
	}

	/* Step 5: count SBUS start bytes */
	int starts = 0;

	for (int i = 0; i < total; i++) {
		if (buf[i] == SBUS_START_BYTE) { starts++; }
	}

	PX4_INFO("--- 0x0F (SBUS start byte) appearances: %d ---", starts);

	if ((ctrla & RXINV_BIT) && starts == 0) {
		PX4_WARN("RXINV=1 in register but 0x0F never seen");
		PX4_WARN("-> hardware ignores RXINV bit in async mode: silicon bug confirmed");

	} else if ((ctrla & RXINV_BIT) && starts > 0) {
		PX4_INFO("RXINV=1 and 0x0F present: RXINV appears functional");

	} else {
		PX4_INFO("RXINV=0 (no inversion configured)");
	}

	return 0;
}

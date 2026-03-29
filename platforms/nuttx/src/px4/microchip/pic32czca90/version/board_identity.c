/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ****************************************************************************/

/**
 * @file board_identity.c
 * PIC32CZ CA90 board identity implementation
 */

#include <px4_platform_common/px4_config.h>
#include <stdio.h>
#include <string.h>

#define CPU_UUID_BYTE_FORMAT_ORDER          {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8}
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00ff0000) >> 8) | (((x) & 0x0000ff00) << 8) | ((x) << 24))

#ifndef PX4_SOC_ARCH_ID
#define PX4_SOC_ARCH_ID 0x0090  /* PIC32CZ CA90 */
#endif

static const uint16_t soc_arch_id = PX4_SOC_ARCH_ID;

/* PIC32CZ CA90 serial number location (DSU Device Identification) */
#define CA90_SERIAL_BASE    0x41002018  /* DSU DID register */
#define CA90_SERIAL_WORDS   3

typedef const uint8_t uuid_uint8_reorder_t[PX4_CPU_UUID_BYTE_LENGTH];

void board_get_uuid(uuid_byte_t uuid_bytes)
{
	uuid_uint8_reorder_t reorder = CPU_UUID_BYTE_FORMAT_ORDER;
	union {
		uuid_byte_t b;
		uuid_uint32_t w;
	} id;

	board_get_uuid32(id.w);

	for (int i = 0; i < PX4_CPU_UUID_BYTE_LENGTH; i++) {
		uuid_bytes[i] = id.b[reorder[i]];
	}
}

__EXPORT void board_get_uuid32(uuid_uint32_t uuid_words)
{
	volatile uint32_t *serial = (volatile uint32_t *)CA90_SERIAL_BASE;

	for (unsigned i = 0; i < PX4_CPU_UUID_WORD32_LENGTH; i++) {
		if (i < CA90_SERIAL_WORDS) {
			uuid_words[i] = serial[i];
		} else {
			uuid_words[i] = serial[i % CA90_SERIAL_WORDS] ^ (i << 24);
		}
	}
}

int board_get_uuid32_formated(char *format_buffer, int size,
			      const char *format,
			      const char *seperator)
{
	uuid_uint32_t uuid;
	board_get_uuid32(uuid);
	int offset = 0;
	int sep_size = seperator ? strlen(seperator) : 0;

	for (unsigned i = 0; (offset < size - 1) && (i < PX4_CPU_UUID_WORD32_LENGTH); i++) {
		offset += snprintf(&format_buffer[offset], size - offset, format, uuid[i]);

		if (sep_size && (offset < size - sep_size - 1) && (i < PX4_CPU_UUID_WORD32_LENGTH - 1)) {
			strncat(&format_buffer[offset], seperator, size - offset);
			offset += sep_size;
		}
	}

	return 0;
}

int board_get_mfguid(mfguid_t mfgid)
{
	volatile uint32_t *serial = (volatile uint32_t *)CA90_SERIAL_BASE;
	uint8_t *rv = &mfgid[0];

	for (unsigned i = 0; i < PX4_CPU_UUID_WORD32_LENGTH; i++) {
		uint32_t uuid_val;

		if (i < CA90_SERIAL_WORDS) {
			uuid_val = serial[i];
		} else {
			uuid_val = serial[i % CA90_SERIAL_WORDS] ^ (i << 24);
		}

		uint32_t uuid_bytes = SWAP_UINT32(uuid_val);
		memcpy(rv, &uuid_bytes, sizeof(uint32_t));
		rv += sizeof(uint32_t);
	}

	return PX4_CPU_MFGUID_BYTE_LENGTH;
}

int board_get_mfguid_formated(char *format_buffer, int size)
{
	mfguid_t mfguid;

	board_get_mfguid(mfguid);
	int offset = 0;

	for (unsigned i = 0; offset < size && i < PX4_CPU_MFGUID_BYTE_LENGTH; i++) {
		offset += snprintf(&format_buffer[offset], size - offset, "%02x", mfguid[i]);
	}

	return offset;
}

int board_get_px4_guid(px4_guid_t px4_guid)
{
	uint8_t *pb = (uint8_t *)&px4_guid[0];
	*pb++ = (soc_arch_id >> 8) & 0xff;
	*pb++ = (soc_arch_id & 0xff);

	for (unsigned i = 0; i < PX4_GUID_BYTE_LENGTH - (sizeof(soc_arch_id) + PX4_CPU_UUID_BYTE_LENGTH); i++) {
		*pb++ = 0;
	}

	volatile uint32_t *serial = (volatile uint32_t *)CA90_SERIAL_BASE;

	for (unsigned i = 0; i < PX4_CPU_UUID_WORD32_LENGTH; i++) {
		uint32_t uuid_val;

		if (i < CA90_SERIAL_WORDS) {
			uuid_val = serial[i];
		} else {
			uuid_val = serial[i % CA90_SERIAL_WORDS] ^ (i << 24);
		}

		uint32_t uuid_bytes = SWAP_UINT32(uuid_val);
		memcpy(pb, &uuid_bytes, sizeof(uint32_t));
		pb += sizeof(uint32_t);
	}

	return PX4_GUID_BYTE_LENGTH;
}

int board_get_px4_guid_formated(char *format_buffer, int size)
{
	px4_guid_t px4_guid;
	board_get_px4_guid(px4_guid);
	int offset = 0;

	size = size & 1 ? size : size - 1;

	for (unsigned i = PX4_GUID_BYTE_LENGTH - size / 2; offset < size && i < PX4_GUID_BYTE_LENGTH; i++) {
		offset += snprintf(&format_buffer[offset], size - offset, "%02x", px4_guid[i]);
	}

	return offset;
}

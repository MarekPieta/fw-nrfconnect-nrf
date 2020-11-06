/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _HID_REPORT_VOICE_H_
#define _HID_REPORT_VOICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_SIZE_VOICE		                20 /* bytes */
#define VOICE_REPORT_COUNT_MAX				REPORT_SIZE_VOICE


#define REPORT_MAP_VOICE(report_id)					\
	0x85, report_id,						\
	0x0A, 0x03, 0xff, /* Usage (0xFF03) */				\
	0x25, 0xff,	  /* Logical Maximum (255) */			\
	0x15, 0x00,       /* Logical Minimum (0) */			\
	0x75, 0x08,       /* Report Size (8) */				\
	0x95, REPORT_SIZE_VOICE,       /* Report Count */		\
	0x81, 0x02        /* Input (Data, Variable, Absolute) */	\

#ifdef __cplusplus
}
#endif

#endif /* _HID_REPORT_VOICE_H_ */

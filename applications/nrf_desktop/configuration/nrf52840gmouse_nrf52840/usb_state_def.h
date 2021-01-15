/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "hid_report_desc.h"

/* This configuration file is included only once from usb_state module and holds
 * information about HID report subscriptions of USB HID instances.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} usb_state_def_include_once;

static const uint32_t usb_hid_report_bm[] = {
	BIT(REPORT_ID_MOUSE),
	BIT(REPORT_ID_KEYBOARD_KEYS),
};

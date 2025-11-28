/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* This configuration file is included only once from usb_state module and holds
 * information about HID report subscriptions of USB HID instances.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} usb_state_def_include_once;

static const uint32_t usb_hid_report_bm[] = {
	BIT(REPORT_ID_SYSTEM_CTRL) | BIT(REPORT_ID_CONSUMER_CTRL),
	BIT(REPORT_ID_KEYBOARD_KEYS),
};

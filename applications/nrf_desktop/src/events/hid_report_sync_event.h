/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _HID_REPORT_SYNC_EVENT_H_
#define _HID_REPORT_SYNC_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>
#include <nrf_profiler.h>

#include <caf/events/module_state_event.h>


/**
 * @brief HID report synchronization events
 * @defgroup nrf_desktop_hid_report_sync_event HID report synchronization events
 *
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief HID report synchronization event.
 *
 * The event is used to improve synchronization of sensor sampling.
 */
struct hid_report_sync_event {
	/** Event header. */
	struct app_event_header header;

	/** Bitmask with indexes of the selected aplication modules. */
	struct module_flags module_flags;

	/** ID of the HID input report using the provided data. */
	uint8_t report_id;

	/** Information if the HID report subscription is enabled or disabled. */
	bool enable;
};
APP_EVENT_TYPE_DECLARE(hid_report_sync_event);

/** @brief HID report synchronization heartbeat event.
 *
 * The event is used to synchronously trigger sampling sensors for a HID input report.
 */
struct hid_report_sync_heartbeat_event {
	/** Event header. */
	struct app_event_header header;

	/** ID of the HID input report using the provided data. */
	uint8_t report_id;
};
APP_EVENT_TYPE_DECLARE(hid_report_sync_heartbeat_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _HID_REPORT_SYNC_EVENT_H_ */

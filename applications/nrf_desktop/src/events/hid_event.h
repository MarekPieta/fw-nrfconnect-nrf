/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _HID_EVENT_H_
#define _HID_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>
#include "nrf_profiler.h"
#include "hid_report_desc.h"


/**
 * @brief HID Events
 * @defgroup nrf_desktop_hid_event_hid_event HID Events
 *
 * File defines a set of events used to transmit the HID report data between application modules.
 *
 * * HID subscribers (HID transports) are the modules that interact directly with HID host.
 *   The modules are used to exchange HID report with the HID host.
 * * HID state is a module that 
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/** @brief HID report event.
 *
 * The event is used to transmit HID reports.
 *
 * For a HID input report, the event is submitted by a HID report provider (on HID state request).
 * Then the event is received by a HID transport and HID report is passed to connected HID host.
 *
 * For a HID output report, the event is submitted a by HID transport. Then the event is processed
 * by HID state that handles the HID output report. In that case, the subscriber field is set to
 * NULL and source is set to an identifier of the HID transport that submitted the event.
 *
 */
struct hid_report_event {
	struct app_event_header header; /**< Event header. */

	const void *source; /**< ID of the report source. */
	const void *subscriber; /**< ID of the report subscriber. */
	struct event_dyndata dyndata; /**< Report data. The first byte is a report id. */
};

APP_EVENT_TYPE_DYNDATA_DECLARE(hid_report_event);


/** @brief HID report subscriber event.
 *
 * The event is submitted by a HID subscriber (HID transport) to subscribe for HID input reports.
 * The HID state module handles the event and notifies HID report providers to provide HID input reports.
 */
struct hid_report_subscriber_event {
	struct app_event_header header; /**< Event header. */

	const void *subscriber; /**< ID of the report subscriber. */
	struct {
		uint8_t priority; /**< Subscriber priority. The bigger value means the
				    * higher priority. The subscriber priority must be unique.
				    * Two or more subscriber must not use the same priority value.
				    */
		uint8_t pipeline_size; /**< Pipeline size. */
		uint8_t report_max; /**< Maximum number of reports with different ID, which can be
				      * processed.
				      */
	} params; /**< Subscriber parameters. Only needed when a subscriber is connecting.
		    * Ignored when disconnecting.
		    */
	bool connected; /**< True if subscriber is connected to the system. */
};

APP_EVENT_TYPE_DECLARE(hid_report_subscriber_event);


/** @brief Report sent event. */
struct hid_report_sent_event {
	struct app_event_header header; /**< Event header. */

	const void *subscriber; /**< Id of the report subscriber. */
	uint8_t report_id; /**< Report id. */
	bool error; /**< If true error occured on send. */
};

APP_EVENT_TYPE_DECLARE(hid_report_sent_event);


/** @brief Report subscription event. */
struct hid_report_subscription_event {
	struct app_event_header header; /**< Event header. */

	const void *subscriber; /**< Id of the report subscriber. */
	uint8_t report_id; /**< Report id. */
	bool enabled; /**< True if notification are enabled. */
};

APP_EVENT_TYPE_DECLARE(hid_report_subscription_event);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _HID_EVENT_H_ */

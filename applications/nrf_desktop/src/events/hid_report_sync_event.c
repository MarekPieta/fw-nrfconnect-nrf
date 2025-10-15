/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "hid_report_sync_event.h"


static void log_hid_report_sync_event(const struct app_event_header *aeh)
{
	const struct hid_report_sync_event *event = cast_hid_report_sync_event(aeh);

	APP_EVENT_MANAGER_LOG(aeh, "Report ID:0x%" PRIx8 ", %sable",
			      event->report_id, enable ? "en" : "dis");
}

static void profile_hid_report_sync_event(struct log_event_buf *buf,
					  const struct app_event_header *aeh)
{
	const struct hid_report_sync_event *event = cast_hid_report_sync_event(aeh);

	nrf_profiler_log_encode_uint8(buf, event->report_id);
	nrf_profiler_log_encode_uint8(buf, event->enable ? 1 : 0);
}

APP_EVENT_INFO_DEFINE(hid_report_sync_event,
		      ENCODE(NRF_PROFILER_ARG_U8, NRF_PROFILER_ARG_U8),
		      ENCODE("report_id", "enable"),
		      profile_hid_report_sync_event);

APP_EVENT_TYPE_DEFINE(hid_report_sync_event,
		      log_hid_report_sync_event,
		      &hid_report_sync_event_info,
		      APP_EVENT_FLAGS_CREATE(
			IF_ENABLED(CONFIG_DESKTOP_INIT_LOG_HID_REPORT_SYNC_EVENT,
				(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE))));

static void log_hid_report_sync_heartbeat_event(const struct app_event_header *aeh)
{
	const struct hid_report_sync_heartbeat_event *event =
		cast_hid_report_sync_heartbeat_event(aeh);

	APP_EVENT_MANAGER_LOG(aeh, "Report ID:0x%" PRIx8, event->report_id);
}

static void profile_hid_report_sync_heartbeat_event(struct log_event_buf *buf,
						    const struct app_event_header *aeh)
{
	const struct hid_report_sync_heartbeat_event *event =
		cast_hid_report_sync_heartbeat_event(aeh);

	nrf_profiler_log_encode_uint8(buf, event->report_id);
}

APP_EVENT_INFO_DEFINE(hid_report_sync_heartbeat_event,
		      ENCODE(NRF_PROFILER_ARG_U8),
		      ENCODE("report_id"),
		      profile_hid_report_sync_heartbeat_event);

APP_EVENT_TYPE_DEFINE(hid_report_sync_heartbeat_event,
		      log_hid_report_sync_heartbeat_event,
		      &hid_report_sync_heartbeat_event_info,
		      APP_EVENT_FLAGS_CREATE(
			IF_ENABLED(CONFIG_DESKTOP_INIT_LOG_HID_REPORT_SYNC_EVENT,
				(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE))));

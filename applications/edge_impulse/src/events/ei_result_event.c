/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include "ei_result_event.h"


static int log_ei_result_event(const struct event_header *eh, char *buf, size_t buf_len)
{
	const struct ei_result_event *event = cast_ei_result_event(eh);

	return snprintf(buf, buf_len, "%s, val: %0.2f, anomaly: %0.2f",
			event->label, event->value, event->anomaly);
}

static void profile_ei_result_event(struct log_event_buf *buf, const struct event_header *eh)
{
}

EVENT_INFO_DEFINE(ei_result_event,
		  ENCODE(),
		  ENCODE(),
		  profile_ei_result_event);

EVENT_TYPE_DEFINE(ei_result_event,
		  IS_ENABLED(CONFIG_EI_APP_INIT_LOG_EI_RESULT_EVENT),
		  log_ei_result_event,
		  &ei_result_event_info);

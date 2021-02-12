/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _SENSOR_EVENT_H_
#define _SENSOR_EVENT_H_

/**
 * @brief Sensor Event
 * @defgroup sensor_event Sensor Event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sensor_event {
	struct event_header header;
	const char *descr;

	struct event_dyndata dyndata;
};

EVENT_TYPE_DYNDATA_DECLARE(sensor_event);

static inline size_t sensor_event_get_data_cnt(const struct sensor_event *event)
{
	return (event->dyndata.size / sizeof(float));
}

static inline const float *sensor_event_get_data_ptr(const struct sensor_event *event)
{
	return (const float *)event->dyndata.data;
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _SENSOR_EVENT_H_ */

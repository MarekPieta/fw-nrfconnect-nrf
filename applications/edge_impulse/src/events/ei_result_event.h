/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _EI_RESULT_EVENT_H_
#define _EI_RESULT_EVENT_H_

/**
 * @brief Edge Impulse Result Event
 * @defgroup ei_result_event Edge Impulse Result Event
 * @{
 */

#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ei_result_event {
	struct event_header header;

	const char *label;
	float value;
	float anomaly;
};

EVENT_TYPE_DECLARE(ei_result_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _EI_RESULT_EVENT_H_ */

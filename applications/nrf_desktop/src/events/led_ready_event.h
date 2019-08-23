/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _LED_READY_EVENT_H_
#define _LED_READY_EVENT_H_

/**
 * @brief LED Ready Event
 * @defgroup led_ready_event LED Ready Event
 * @{
 */

#include "event_manager.h"


#ifdef __cplusplus
extern "C" {
#endif


/** @brief LED ready event used to notify that leds are ready for next step . */
struct led_ready_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(led_ready_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _LED_READY_EVENT_H_ */

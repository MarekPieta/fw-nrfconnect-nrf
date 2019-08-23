/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "led_ready_event.h"

EVENT_TYPE_DEFINE(led_ready_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_LED_READY_EVENT),
		  NULL,
		  NULL);

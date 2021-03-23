/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#include <caf/events/button_event.h>
#include "ml_state_event.h"

#define MODULE ml_state
#include <caf/events/module_state_event.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_ML_APP_ML_STATE_LOG_LEVEL);

#define CONTROL_BUTTON_ID	CONFIG_ML_APP_ML_STATE_CONTROL_BUTTON_ID

#define APP_ML_RUNNER		IS_ENABLED(CONFIG_ML_APP_ML_RUNNER)
#define APP_ML_FORWARDER	IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER)
#define APP_MULTI_STATE		(APP_ML_RUNNER && APP_ML_FORWARDER)

BUILD_ASSERT(APP_ML_RUNNER || APP_ML_FORWARDER);

static enum ml_state state = APP_ML_RUNNER ? ML_STATE_MODEL_RUNNING : ML_STATE_DATA_FORWARDING;


static void broadcast_state(void)
{
	struct ml_state_event *event = new_ml_state_event();

	event->state = state;
	EVENT_SUBMIT(event);
}

static bool handle_button_event(const struct button_event *event)
{
	if ((event->key_id == CONTROL_BUTTON_ID) && event->pressed) {
		switch (state) {
		case ML_STATE_DATA_FORWARDING:
			state = ML_STATE_MODEL_RUNNING;
			break;

		case ML_STATE_MODEL_RUNNING:
			state = ML_STATE_DATA_FORWARDING;
			break;

		default:
			/* Should not happen. */
			__ASSERT_NO_MSG(false);
			break;
		}

		broadcast_state();
	}

	return false;
}

static bool event_handler(const struct event_header *eh)
{
	if (APP_MULTI_STATE && is_button_event(eh)) {
		return handle_button_event(cast_button_event(eh));
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event = cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			module_set_state(MODULE_STATE_READY);
			initialized = true;
			broadcast_state();
		}

		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
#if APP_MULTI_STATE
EVENT_SUBSCRIBE(MODULE, button_event);
#endif /* APP_MULTI_STATE */

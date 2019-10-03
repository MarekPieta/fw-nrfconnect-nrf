/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <misc/util.h>
#include <misc/byteorder.h>

#include "button_event.h"
#include "config_event.h"
#include "power_event.h"

#define MODULE cpi_state
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_CPI_STATE_LOG_LEVEL);

#define CPI_STEP	(CONFIG_DESKTOP_CPI_STATE_HIGH_VALUE - 		\
			 CONFIG_DESKTOP_CPI_STATE_LOW_VALUE) /  	\
			 CONFIG_DESKTOP_CPI_STATE_NUMBER_OF_STEPS

#define KEY_ID_INCREMENT	CONFIG_DESKTOP_CPI_STATE_TOGGLE_UP_BUTTON_ID
#define KEY_ID_DECREMENT	CONFIG_DESKTOP_CPI_STATE_TOGGLE_DOWN_BUTTON_ID

enum state {
	STATE_DISABLED,
	STATE_ACTIVE,
	STATE_OFF,
};

static enum state state;
static u32_t current_cpi = CONFIG_DESKTOP_MOTION_SENSOR_CPI;


static void send_cpi_config_event(u32_t cpi)
{
	BUILD_ASSERT(sizeof(cpi) == sizeof(current_cpi));

	struct config_event *event = new_config_event(sizeof(cpi));

	event->id = SETUP_EVENT_ID(SETUP_MODULE_SENSOR, SENSOR_OPT_CPI);
	sys_put_le32(cpi, event->dyndata.data);
	event->store_needed = true;

	EVENT_SUBMIT(event);
}

static void step_cpi(bool up)
{
	BUILD_ASSERT(CONFIG_DESKTOP_CPI_STATE_HIGH_VALUE >
		     CONFIG_DESKTOP_CPI_STATE_LOW_VALUE);
	BUILD_ASSERT((CONFIG_DESKTOP_CPI_STATE_HIGH_VALUE -
		      CONFIG_DESKTOP_CPI_STATE_LOW_VALUE) %
		     CONFIG_DESKTOP_CPI_STATE_NUMBER_OF_STEPS == 0);

	u32_t new_cpi;

	if (state != STATE_ACTIVE) {
		return;
	}

	if (up) {
		new_cpi = MIN(current_cpi + CPI_STEP,
			      CONFIG_DESKTOP_CPI_STATE_HIGH_VALUE);
	} else {
		if (current_cpi > CPI_STEP) {
			new_cpi = MAX(current_cpi - CPI_STEP,
				      CONFIG_DESKTOP_CPI_STATE_LOW_VALUE);
		} else {
			new_cpi = CONFIG_DESKTOP_CPI_STATE_LOW_VALUE;
		}
	}

	if (new_cpi != current_cpi) {
		current_cpi = new_cpi;
		send_cpi_config_event(current_cpi);
		LOG_INF("CPI set to: %u", current_cpi);
	}
}

static void power_down(void)
{
	state = STATE_OFF;
	module_set_state(MODULE_STATE_OFF);
}

static void wake_up(void)
{
	state = STATE_ACTIVE;
	module_set_state(MODULE_STATE_READY);
}

static bool is_my_config_id(u8_t config_id)
{
        return (GROUP_FIELD_GET(config_id) == EVENT_GROUP_SETUP) &&
               (MOD_FIELD_GET(config_id) == SETUP_MODULE_SENSOR) &&
	       (OPT_FIELD_GET(config_id) == SENSOR_OPT_CPI);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_button_event(eh)) {
		const struct button_event *event =
			cast_button_event(eh);

		if (unlikely(event->key_id == KEY_ID_INCREMENT) &&
		    event->pressed) {
			step_cpi(true);
		} else if (unlikely(event->key_id == KEY_ID_DECREMENT) &&
			   event->pressed) {
			step_cpi(false);
		}

		return false;
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			__ASSERT_NO_MSG(state == STATE_DISABLED);
			state = STATE_ACTIVE;
		}
		return false;
	}

	if (is_power_down_event(eh)) {
		if (state != STATE_OFF) {
			power_down();
		}
		return false;
	}

	if (is_wake_up_event(eh)) {
		if (state != STATE_ACTIVE) {
			wake_up();
		}
		return false;
	}

	if (is_config_event(eh)) {
		const struct config_event *event = cast_config_event(eh);

		if (is_my_config_id(event->id)) {
			__ASSERT_NO_MSG(event->dyndata.size ==
					sizeof(current_cpi));
			current_cpi = sys_get_le32(event->dyndata.data);

		}

		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, button_event);
EVENT_SUBSCRIBE(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
EVENT_SUBSCRIBE_EARLY(MODULE, config_event);

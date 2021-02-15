/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <ei_wrapper.h>

#include "sensor_event.h"
#include "ei_result_event.h"

#define MODULE ei_ml_runner
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_EI_APP_ML_RUNNER_LOG_LEVEL);

#define SHIFT_WINDOWS		0
#define SHIFT_FRAMES		5


static void result_ready_cb(int err)
{
	if (err) {
		LOG_ERR("Result ready callback returned error (err: %d)", err);
		module_set_state(MODULE_STATE_ERROR);
		return;
	}

	struct ei_result_event *evt = new_ei_result_event();

	err = ei_wrapper_get_classification_results(&evt->label, &evt->value, &evt->anomaly);

	if (err) {
		LOG_ERR("Cannot get classification results (err: %d)", err);
		k_free(evt);
		module_set_state(MODULE_STATE_ERROR);
	} else {
		EVENT_SUBMIT(evt);
	}

	err = ei_wrapper_start_prediction(SHIFT_WINDOWS, SHIFT_FRAMES);
	if (err) {
		LOG_ERR("Cannot restart prediction (err: %d)", err);
		module_set_state(MODULE_STATE_ERROR);
	}
}

static bool handle_sensor_event(const struct sensor_event *event)
{
	int err = ei_wrapper_add_data(sensor_event_get_data_ptr(event),
				      sensor_event_get_data_cnt(event));

	if (err) {
		LOG_ERR("Cannot add data for EI wrapper (err %d)", err);
		module_set_state(MODULE_STATE_ERROR);
	}

	return false;
}

static int init_fn(void)
{
	int err = ei_wrapper_init(result_ready_cb);

	if (err) {
		LOG_ERR("Edge Impulse wrapper failed to initialize (err: %d)", err);
	};

	err = ei_wrapper_start_prediction(0, 0);

	if (err) {
		LOG_ERR("Cannot start prediction (err: %d)", err);
	}

	return err;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sensor_event(eh)) {
		return handle_sensor_event(cast_sensor_event(eh));
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event = cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			if (!init_fn()) {
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}

		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, sensor_event);

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <drivers/sensor_sim.h>

#define MODULE sensor_sim_ctrl
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_EI_APP_SENSOR_SIM_CTRL_LOG_LEVEL);

#define WAVE_AMPLITUDE		0.5
#define WAVE_NOISE		0.1
#define WAVE_OFFSET		0
#define WAVE_PERIOD_MS		2000
#define WAVE_SWAP_PERIOD	K_MSEC(3 * WAVE_PERIOD_MS)

static struct k_delayed_work change_wave;


static void change_wave_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	BUILD_ASSERT(WAVE_GEN_TYPE_SINE == 0);

	static struct wave_gen_param wgp = {
		.type = WAVE_GEN_TYPE_SINE,
		.period_ms = WAVE_PERIOD_MS,
		.offset = WAVE_OFFSET,
		.amplitude = WAVE_AMPLITUDE,
		.noise = WAVE_NOISE,
	};

	int err = sensor_sim_set_wave_param(SENSOR_CHAN_ACCEL_XYZ, &wgp);

	if (err) {
		LOG_ERR("Cannot set simulated accel params (err %d)", err);
	} else {
		wgp.type++;
		if (wgp.type == WAVE_GEN_TYPE_COUNT) {
			wgp.type = WAVE_GEN_TYPE_SINE;
		}

		k_delayed_work_submit(&change_wave, WAVE_SWAP_PERIOD);
	}
}

static int init_fn(void)
{
	k_delayed_work_init(&change_wave, change_wave_fn);
	k_delayed_work_submit(&change_wave, WAVE_SWAP_PERIOD);

	return 0;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_module_state_event(eh)) {
		struct module_state_event *event = cast_module_state_event(eh);

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

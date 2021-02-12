/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <drivers/sensor.h>

#include "sensor_event.h"

#define MODULE sensor_sampler
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_EI_APP_SENSOR_SAMPLER_LOG_LEVEL);

#define SENSOR_LABEL		CONFIG_SENSOR_SIM_DEV_NAME
#define SAMPLE_PERIOD		K_MSEC(20)
#define ACCELEROMETER_CHANNELS	3

const static enum sensor_channel sensor_channels[] = {
	SENSOR_CHAN_ACCEL_X,
	SENSOR_CHAN_ACCEL_Y,
	SENSOR_CHAN_ACCEL_Z
};

static const struct device *dev;
static struct k_delayed_work sample_sensor;

static int send_sensor_data(void)
{
	struct sensor_value data[ARRAY_SIZE(sensor_channels)];
	int err = 0;

	for (size_t i = 0; (!err) && (i < ARRAY_SIZE(sensor_channels)); i++) {
		err = sensor_sample_fetch_chan(dev, sensor_channels[i]);
		if (!err) {
			err = sensor_channel_get(dev, sensor_channels[i], &data[i]);
		}
	}

	if (err) {
		LOG_ERR("Sensor sampling error (err %d)", err);
		return err;
	}

	struct sensor_event *event =
		new_sensor_event(sizeof(float) * ACCELEROMETER_CHANNELS);

	event->descr = "accel_sim";
	float *data_ptr = (float *)&event->dyndata.data[0];

	for (size_t i = 0; i < ARRAY_SIZE(data); i++) {
		*data_ptr = sensor_value_to_double(&data[i]);
		data_ptr++;
	}

	EVENT_SUBMIT(event);

	return err;
}

void sample_sensor_fn(struct k_work* w)
{
	ARG_UNUSED(w);

	if (send_sensor_data()) {
		module_set_state(MODULE_STATE_ERROR);
	} else {
		k_delayed_work_submit(&sample_sensor, SAMPLE_PERIOD);
	}
}

static int init_fn(void)
{
	dev = device_get_binding(SENSOR_LABEL);
	
	if (!dev) {
		return -ENXIO;
	}

	k_delayed_work_init(&sample_sensor, sample_sensor_fn);
	k_delayed_work_submit(&sample_sensor, SAMPLE_PERIOD);

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

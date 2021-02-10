/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <drivers/gpio.h>
#include <init.h>
#include <drivers/sensor.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging/log.h>

#include "sensor_sim.h"
#include "signal_gen.h"

#include <math.h>
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

LOG_MODULE_REGISTER(sensor_sim, CONFIG_SENSOR_SIM_LOG_LEVEL);

static double accel_samples[3];

static double temp_sample;
static double humidity_sample;
static double pressure_sample;

typedef void (*generator_function)(enum sensor_channel chan, double *out_val);

/*
 * @brief Helper function to convert from double to sensor_value struct
 *
 * @param val Sensor value to convert.
 * @param sense_val Pointer to sensor_value to store the converted data.
 */
static void double_to_sensor_value(double val,
				struct sensor_value *sense_val)
{
	sense_val->val1 = (int)val;
	sense_val->val2 = (val - (int)val) * 1000000;
}

#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
/*
 * @brief Callback for GPIO when using button as trigger.
 *
 * @param dev Pointer to device structure.
 * @param cb Pointer to GPIO callback structure.
 * @param pins Pin mask for callback.
 */
static void sensor_sim_gpio_callback(const struct device *dev,
				struct gpio_callback *cb,
				uint32_t pins)
{
	ARG_UNUSED(pins);
	struct sensor_sim_data *drv_data =
		CONTAINER_OF(cb, struct sensor_sim_data, gpio_cb);

	gpio_pin_interrupt_configure(dev, drv_data->gpio_pin, GPIO_INT_DISABLE);
	k_sem_give(&drv_data->gpio_sem);
}
#endif /* CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON */

#if defined(CONFIG_SENSOR_SIM_TRIGGER)
/*
 * @brief Function that runs in the sensor simulator thread when using trigger.
 *
 * @param dev_ptr Pointer to sensor simulator device.
 */
static void sensor_sim_thread(int dev_ptr)
{
	struct device *dev = INT_TO_POINTER(dev_ptr);
	struct sensor_sim_data *drv_data = dev->data;

	while (true) {
		if (IS_ENABLED(CONFIG_SENSOR_SIM_TRIGGER_USE_TIMEOUT)) {
			k_sleep(K_MSEC(CONFIG_SENSOR_SIM_TRIGGER_TIMEOUT_MSEC));
		} else if (IS_ENABLED(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)) {
			k_sem_take(&drv_data->gpio_sem, K_FOREVER);
		} else {
			/* Should not happen. */
			__ASSERT_NO_MSG(false);
		}

		if (drv_data->drdy_handler != NULL) {
			drv_data->drdy_handler(dev, &drv_data->drdy_trigger);
		}

#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
		gpio_pin_interrupt_configure(drv_data->gpio, drv_data->gpio_pin,
					     GPIO_INT_EDGE_FALLING);
#endif
	}
}

/*
 * @brief Initializing thread when simulator uses trigger
 *
 * @param dev Pointer to device instance.
 */
static int sensor_sim_init_thread(const struct device *dev)
{
	struct sensor_sim_data *drv_data = dev->data;

#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
	drv_data->gpio = device_get_binding(drv_data->gpio_port);
	if (drv_data->gpio == NULL) {
		LOG_ERR("Failed to get pointer to %s device",
			drv_data->gpio_port);
		return -EINVAL;
	}

	gpio_pin_configure(drv_data->gpio, drv_data->gpio_pin,
			   GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_DEBOUNCE);

	gpio_init_callback(&drv_data->gpio_cb,
			   sensor_sim_gpio_callback,
			   BIT(drv_data->gpio_pin));

	if (gpio_add_callback(drv_data->gpio, &drv_data->gpio_cb) < 0) {
		LOG_ERR("Failed to set GPIO callback");
		return -EIO;
	}

	k_sem_init(&drv_data->gpio_sem, 0, UINT_MAX);

#endif   /* CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON */

	k_thread_create(&drv_data->thread, drv_data->thread_stack,
			CONFIG_SENSOR_SIM_THREAD_STACK_SIZE,
			// TODO TORA: upmerge confirmation from Jan Tore needed.
			(k_thread_entry_t)sensor_sim_thread, (void *)dev,
			NULL, NULL,
			K_PRIO_COOP(CONFIG_SENSOR_SIM_THREAD_PRIORITY),
			0, K_NO_WAIT);

	return 0;
}

static int sensor_sim_trigger_set(const struct device *dev,
			   const struct sensor_trigger *trig,
			   sensor_trigger_handler_t handler)
{
	int ret = 0;
	struct sensor_sim_data *drv_data = dev->data;

#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
	gpio_pin_interrupt_configure(drv_data->gpio, drv_data->gpio_pin,
				     GPIO_INT_DISABLE);
#endif

	switch (trig->type) {
	case SENSOR_TRIG_DATA_READY:
		drv_data->drdy_handler = handler;
		drv_data->drdy_trigger = *trig;
		break;
	default:
		LOG_ERR("Unsupported sensor trigger");
		ret = -ENOTSUP;
		break;
	}

#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
	gpio_pin_interrupt_configure(drv_data->gpio, drv_data->gpio_pin,
				     GPIO_INT_EDGE_FALLING);
#endif
	return ret;
}
#endif /* CONFIG_SENSOR_SIM_TRIGGER */

/*
 * @brief Initializes sensor simulator
 *
 * @param dev Pointer to device instance.
 *
 * @return 0 when successful or negative error code
 */
static int sensor_sim_init(const struct device *dev)
{
#if defined(CONFIG_SENSOR_SIM_TRIGGER)
#if defined(CONFIG_SENSOR_SIM_TRIGGER_USE_BUTTON)
	struct sensor_sim_data *drv_data = dev->data;

	drv_data->gpio_port = DT_GPIO_LABEL(DT_ALIAS(sw0), gpios);
	drv_data->gpio_pin = DT_GPIO_PIN(DT_ALIAS(sw0), gpios);
#endif
	if (sensor_sim_init_thread(dev) < 0) {
		LOG_ERR("Failed to initialize trigger interrupt");
		return -EIO;
	}
#endif
	srand(k_cycle_get_32());

	return 0;
}

/**
 * @brief Generates a pseudo-random number between -1 and 1.
 */
static double generate_pseudo_random(void)
{
	return (double)rand() / ((double)RAND_MAX / 2.0) - 1.0;
}

static void generate_toggle(enum sensor_channel chan, double *out_val)
{
	static const double amplitude = 20.0;
	static double val_sign = 1.0;

	double res_val = amplitude * val_sign;

	*out_val = res_val;
	if (chan == SENSOR_CHAN_ACCEL_XYZ) {
		*(out_val + 1) = res_val;
		*(out_val + 2) = res_val;
	}

	val_sign *= -1.0;
}

static void generate_sine(enum sensor_channel chan, double *out_val)
{
	/* Predefined period and amplitude for generated sine function. */
	static const uint32_t period_ms = 10000;
	static const double amplitude = 20.0;

	double time = k_uptime_get_32() % period_ms;
	double angle = 2 * M_PI * (time / period_ms);

	double res_val = offset + amplitude * sin(angle);

	*out_val = res_val;
	if (chan == SENSOR_CHAN_ACCEL_XYZ) {
		*(out_val + 1) = res_val;
		*(out_val + 2) = res_val;
	}
}

static void generate_signal_gen(enum sensor_channel chan, double *out_val)
{
	float readout[3];

	signal_gen_get_data(readout, 3);

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
		/* The function must generate samples for all the
		 * requested channels.
		 */
		*out_val = readout[0];
		break;
	case SENSOR_CHAN_ACCEL_Y:
		*out_val = readout[1];
		break;
	case SENSOR_CHAN_ACCEL_Z:
		*out_val = readout[2];
		break;
	case SENSOR_CHAN_ACCEL_XYZ:
		*out_val = readout[0];
		out_val++;
		*out_val = readout[1];
		out_val++;
		*out_val = readout[2];
		break;
	default:
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
	}
}

/*
 * @brief Generates accelerometer data.
 *
 * @param chan Channel to generate data for.
 */
static int generate_accel_data(enum sensor_channel chan)
{
	int retval = 0;
	generator_function gen_fn;

	if (IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_SINE)) {
		gen_fn = generate_sine;
	} else if (IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_STATIC)) {
		gen_fn = generate_toggle;
	} else if (IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_SIGNAL_GEN)){
		gen_fn = generate_signal_gen;
	} else {
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
	}

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_XYZ:
		/* The function must generate samples for all the
		 * requested channels.
		 */
		gen_fn(chan, &accel_samples[0]);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		gen_fn(chan, &accel_samples[1]);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		gen_fn(chan, &accel_samples[2]);
		break;
	default:
		retval = -ENOTSUP;
	}

	return retval;
}

/**
 * @brief Generates temperature data.
 */
static void generate_temp_data(void)
{
	temp_sample = CONFIG_SENSOR_SIM_BASE_TEMPERATURE +
		      generate_pseudo_random();
}

/**
 * @brief Generates humidity data.
 */
static void generate_humidity_data(void)
{
	humidity_sample = CONFIG_SENSOR_SIM_BASE_HUMIDITY +
			  generate_pseudo_random();
}

/**
 * @brief Generates pressure data.
 */
static void generate_pressure_data(void)
{
	pressure_sample = CONFIG_SENSOR_SIM_BASE_PRESSURE +
			  generate_pseudo_random();
}

/*
 * @brief Generates simulated sensor data for a channel.
 *
 * @param chan Channel to generate data for.
 */
static int sensor_sim_generate_data(enum sensor_channel chan)
{
	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
	case SENSOR_CHAN_ACCEL_XYZ:
		generate_accel_data(chan);
		break;

	case SENSOR_CHAN_AMBIENT_TEMP:
		generate_temp_data();
		break;
	case SENSOR_CHAN_HUMIDITY:
		generate_humidity_data();
		break;
	case SENSOR_CHAN_PRESS:
		generate_pressure_data();
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int sensor_sim_attr_set(const struct device *dev,
		enum sensor_channel chan,
		enum sensor_attribute attr,
		const struct sensor_value *val)
{
	return 0;
}

static int sensor_sim_sample_fetch(const struct device *dev,
				enum sensor_channel chan)
{
	return sensor_sim_generate_data(chan);
}

static int sensor_sim_channel_get(const struct device *dev,
				  enum sensor_channel chan,
				  struct sensor_value *sample)
{
	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
		double_to_sensor_value(accel_samples[0], sample);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		double_to_sensor_value(accel_samples[1], sample);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		double_to_sensor_value(accel_samples[2], sample);
		break;
	case SENSOR_CHAN_ACCEL_XYZ:
		double_to_sensor_value(accel_samples[0], sample);
		double_to_sensor_value(accel_samples[1], ++sample);
		double_to_sensor_value(accel_samples[2], ++sample);
		break;
	case SENSOR_CHAN_AMBIENT_TEMP:
		double_to_sensor_value(temp_sample, sample);
		break;
	case SENSOR_CHAN_HUMIDITY:
		double_to_sensor_value(humidity_sample, sample);
		break;
	case SENSOR_CHAN_PRESS:
		double_to_sensor_value(pressure_sample, sample);
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static struct sensor_sim_data sensor_sim_data;

static const struct sensor_driver_api sensor_sim_api_funcs = {
	.attr_set = sensor_sim_attr_set,
	.sample_fetch = sensor_sim_sample_fetch,
	.channel_get = sensor_sim_channel_get,
#if defined(CONFIG_SENSOR_SIM_TRIGGER)
	.trigger_set = sensor_sim_trigger_set
#endif
};

DEVICE_DEFINE(sensor_sim, CONFIG_SENSOR_SIM_DEV_NAME,
	      sensor_sim_init, device_pm_control_nop,
	      &sensor_sim_data, NULL,
	      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
	      &sensor_sim_api_funcs);

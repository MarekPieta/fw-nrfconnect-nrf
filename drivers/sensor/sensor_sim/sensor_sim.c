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

#include "sensor_sim_priv.h"
#include <drivers/sensor_sim.h>

#include <math.h>
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif
#define ACCEL_CHAN_COUNT	3

LOG_MODULE_REGISTER(sensor_sim, CONFIG_SENSOR_SIM_LOG_LEVEL);

struct wave_param {
	enum wave_type type;
	uint32_t period_ms;
	double amplitude;
	double noise;
	struct k_mutex mutex;
};

static struct wave_param accel_params = {
	.type = WAVE_TYPE_SINE,
	.period_ms = 10000,
	.amplitude = 20.0,
};

static double accel_samples[ACCEL_CHAN_COUNT];

static double temp_sample;
static double humidity_sample;
static double pressure_sample;

/**
 * @typedef generator_function
 * @brief Function used to generate sensor value for given channel.
 *
 * @param chan[in]	Selected sensor channel.
 * @param out_val[out]	Pointer to the variable that is used to store result.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
typedef int (*generator_function)(enum sensor_channel chan, double *out_val);

int sensor_sim_set_accel_params(enum wave_type type, double amplitude, uint32_t period_ms,
				double noise)
{
	if (!IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_WAVE)) {
		return -ENOTSUP;
	}

	int err = k_mutex_lock(&accel_params.mutex, K_FOREVER);

	__ASSERT_NO_MSG(!err);

	accel_params.type = type;
	accel_params.amplitude = amplitude;
	accel_params.period_ms = period_ms;
	accel_params.noise = noise;

	err = k_mutex_unlock(&accel_params.mutex);
	__ASSERT_NO_MSG(!err);

	return err;
}

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
	int err = k_mutex_init(&accel_params.mutex);

	__ASSERT_NO_MSG(!err);

	return 0;
}

/**
 * @brief Generates a pseudo-random number between -1 and 1.
 */
static double generate_pseudo_random(void)
{
	return (double)rand() / ((double)RAND_MAX / 2.0) - 1.0;
}

/*
 * @brief Generate value for acceleration signal toggling between two values on fetch.
 *
 * @param chan[in]	Selected sensor channel.
 * @param out_val[out]	Pointer to the variable that is used to store result.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
static int generate_toggle(enum sensor_channel chan, double *out_val)
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

	return 0;
}

/*
 * @brief Calculate sine wave value.
 *
 * @param time[in]	Time for generated value (lower than the sine wave period).
 * @param period[in]	Sine wave period.
 *
 * @return Sine wave value for given time.
 */
static double sine_val(uint32_t time, uint32_t period)
{
	static const double amplitude = 1.0;
	double angle = 2 * M_PI * time / period;

	return amplitude * sin(angle);
}

/*
 * @brief Calculate triangle wave value.
 *
 * @param time[in]	Time for generated value (lower than the triangle wave period).
 * @param period[in]	Triangle wave period.
 *
 * @return Triangle wave value for given time.
 */
static double triangle_val(uint32_t time, uint32_t period)
{
	static const double amplitude = 1.0;

	double res;
	uint32_t line_time = period / 2;
	double change;

	if (time < line_time) {
		change = 2 * amplitude * time / line_time;
		res = -amplitude + change;
	} else {
		time -= line_time;
		change = 2 * amplitude * time / line_time;
		res = amplitude - change;
	}

	return res;
}

/*
 * @brief Calculate square wave value.
 *
 * @param time[in]	Time for generated value (lower than the square wave period).
 * @param period[in]	Square wave period.
 *
 * @return Square wave value for given time.
 */
static double square_val(uint32_t time, uint32_t period)
{
	static const double amplitude = 1.0;

	return ((time < (period / 2)) ? (-amplitude) : (amplitude));
}

/*
 * @brief Generate value of acceleration wave signal.
 *
 * @param chan[in]	Selected sensor channel.
 * @param out_val[out]	Pointer to the variable that is used to store result.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
static int generate_wave(enum sensor_channel chan, double *out_val)
{
	static const double base_accel_samples[ACCEL_CHAN_COUNT] = {0.0, 0.0, 0.0};

	uint32_t time = k_uptime_get_32() % accel_params.period_ms;
	double res_val = 0;

	int err = k_mutex_lock(&accel_params.mutex, K_FOREVER);

	__ASSERT_NO_MSG(!err);

	switch (accel_params.type) {
	case WAVE_TYPE_SINE:
		res_val = sine_val(time, accel_params.period_ms);
		break;

	case WAVE_TYPE_TRIANGLE:
		res_val = triangle_val(time, accel_params.period_ms);
		break;

	case WAVE_TYPE_SQUARE:
		res_val = square_val(time, accel_params.period_ms);
		break;

	case WAVE_TYPE_NONE:
		res_val = 0.0;
		break;

	default:
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
	}

	res_val *= accel_params.amplitude;

	err = k_mutex_unlock(&accel_params.mutex);
	__ASSERT_NO_MSG(!err);

	BUILD_ASSERT((SENSOR_CHAN_ACCEL_X + 1) == SENSOR_CHAN_ACCEL_Y);
	BUILD_ASSERT((SENSOR_CHAN_ACCEL_Y + 1) == SENSOR_CHAN_ACCEL_Z);

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
		*out_val = res_val + base_accel_samples[chan - SENSOR_CHAN_ACCEL_X] +
			   (accel_params.noise * generate_pseudo_random());
		break;

	case SENSOR_CHAN_ACCEL_XYZ:
		for (size_t i = 0; i < ACCEL_CHAN_COUNT; i++) {
			*(out_val + i) = res_val + base_accel_samples[i] +
					 (accel_params.noise * generate_pseudo_random());

		}
		break;

	default:
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
	}

	return err;
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

	if (IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_WAVE)) {
		gen_fn = generate_wave;
	} else if (IS_ENABLED(CONFIG_SENSOR_SIM_ACCEL_STATIC)) {
		gen_fn = generate_toggle;
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
		retval = gen_fn(chan, &accel_samples[0]);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		retval = gen_fn(chan, &accel_samples[1]);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		retval = gen_fn(chan, &accel_samples[2]);
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

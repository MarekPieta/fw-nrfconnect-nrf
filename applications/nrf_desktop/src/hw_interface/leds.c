/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <assert.h>
#include <pwm.h>

#include "power_event.h"
#include "led_event.h"
#include "led_ready_event.h"

#include "leds_def.h"

#define MODULE leds
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_LOG_LEVEL);

struct led {
	struct device *pwm_dev;

	size_t id;
	struct led_color color;
	const struct led_effect *effect;
	const struct led_effect *next_effect;
	bool stream;
	u16_t effect_step;
	u16_t effect_substep;

	struct k_delayed_work work;
};

static const struct led_effect *leds_state_bck[CONFIG_DESKTOP_LED_COUNT];
static struct led leds[CONFIG_DESKTOP_LED_COUNT];
static bool streaming;

static void pwm_out(struct led *led, struct led_color *color)
{
	for (size_t i = 0; i < ARRAY_SIZE(color->c); i++) {
		pwm_pin_set_usec(led->pwm_dev, led_pins[led->id][i],
				 CONFIG_DESKTOP_LED_BRIGHTNESS_MAX,
				 color->c[i]);
	}
}

static void pwm_off(struct led *led)
{
	struct led_color nocolor = {0};

	pwm_out(led, &nocolor);
}

static void ask_for_next_step(void)
{
	LOG_INF("Asking for new step");
	struct led_ready_event *ready_event = new_led_ready_event();

	EVENT_SUBMIT(ready_event);
}

static void restore_led_state_effect(struct led *led)
{
	LOG_INF("Restore previous led state effect");
	led->effect = leds_state_bck[led->id];
}

static void handle_led_stream(struct led *led)
{
	if (!led->next_effect) {
		LOG_INF("Streaming end");
		streaming = false;
		led->stream = false;

		restore_led_state_effect(led);
	} else {
		led->effect = led->next_effect;
		led->next_effect = NULL;

		ask_for_next_step();
	}
	led->effect_step = 0;
}

static void handle_first_stream_event(struct led *led)
{
	LOG_INF("First stream event - inserting additional effect");

	streaming = true;
	led->effect = led->next_effect;
}

static void handle_incoming_effect(const struct led_event *event)
{
	LOG_INF("Handle incoming effect");
	if (leds[event->led_id].next_effect != NULL) {
		LOG_WRN("Effect in use, override effect");
	}
	LOG_INF("Inserting incoming effect to next_effect");
	leds[event->led_id].next_effect = event->led_effect;
}

static void work_handler(struct k_work *work)
{
	struct led *led = CONTAINER_OF(work, struct led, work);

	const struct led_effect_step *effect_step =
		&led->effect->steps[led->effect_step];

	int substeps_left = effect_step->substep_count - led->effect_substep;
	for (size_t i = 0; i < ARRAY_SIZE(led->color.c); i++) {
		int diff = (effect_step->color.c[i] - led->color.c[i]) /
			substeps_left;
		led->color.c[i] += diff;
	}
	pwm_out(led, &led->color);

	led->effect_substep++;
	if (led->effect_substep == effect_step->substep_count) {
		led->effect_substep = 0;
		led->effect_step++;

		if (led->effect_step == led->effect->step_count) {
			if (led->effect->loop_forever) {
				led->effect_step = 0;
			}
			if (led->stream) {
				handle_led_stream(led);
			}
		} else {
			__ASSERT_NO_MSG(led->effect->steps[led->effect_step].substep_count > 0);
		}
	}

	if (led->effect_step < led->effect->step_count) {
		s32_t next_delay =
			led->effect->steps[led->effect_step].substep_time;

		k_delayed_work_submit(&led->work, next_delay);
	}
}

static void led_update(struct led *led)
{
	k_delayed_work_cancel(&led->work);

	led->effect_step = 0;
	led->effect_substep = 0;

	if (!led->effect) {
		LOG_WRN("No effect set");
		return;
	}

	__ASSERT_NO_MSG(led->effect->steps);

	if (led->effect->step_count > 0) {
		s32_t next_delay =
			led->effect->steps[led->effect_step].substep_time;

		k_delayed_work_submit(&led->work, next_delay);
	} else {
		LOG_WRN("LED effect with no effect");
	}
}

static int leds_init(void)
{
	const char *dev_name[] = {
#if CONFIG_PWM_0
		DT_NORDIC_NRF_PWM_PWM_0_LABEL,
#endif
#if CONFIG_PWM_1
		DT_NORDIC_NRF_PWM_PWM_1_LABEL,
#endif
#if CONFIG_PWM_2
		DT_NORDIC_NRF_PWM_PWM_2_LABEL,
#endif
#if CONFIG_PWM_3
		DT_NORDIC_NRF_PWM_PWM_3_LABEL,
#endif
	};

	int err = 0;

	BUILD_ASSERT_MSG(ARRAY_SIZE(leds) <= ARRAY_SIZE(dev_name),
			 "not enough PWMs");

	for (size_t i = 0; (i < ARRAY_SIZE(leds)) && !err; i++) {
		leds[i].pwm_dev = device_get_binding(dev_name[i]);
		leds[i].id = i;

		if (!leds[i].pwm_dev) {
			LOG_ERR("Cannot bind %s", dev_name[i]);
			err = -ENXIO;
		} else {
			k_delayed_work_init(&leds[i].work, work_handler);
			led_update(&leds[i]);
		}
	}

	return err;
}

static void leds_start(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
		int err = device_set_power_state(leds[i].pwm_dev,
						 DEVICE_PM_ACTIVE_STATE,
						 NULL, NULL);
		if (err) {
			LOG_ERR("PWM enable failed");
		}
#endif
		led_update(&leds[i]);
	}
}

static void leds_stop(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		k_delayed_work_cancel(&leds[i].work);

		pwm_off(&leds[i]);

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
		int err = device_set_power_state(leds[i].pwm_dev,
						 DEVICE_PM_SUSPEND_STATE,
						 NULL, NULL);
		if (err) {
			LOG_ERR("PWM disable failed");
		}
#endif
	}
}

static bool event_handler(const struct event_header *eh)
{
	static bool initialized;

	if (is_led_event(eh)) {
		const struct led_event *event = cast_led_event(eh);

		__ASSERT_NO_MSG(event->led_id < CONFIG_DESKTOP_LED_COUNT);

		if (streaming && !event->stream) {
			leds_state_bck[event->led_id] = event->led_effect;
			return true;
		}

		struct led *led = &leds[event->led_id];

		led->stream = event->stream;

		if (led->stream) {
			LOG_INF("Stream event");
			handle_incoming_effect(event);
			if (streaming) {
				LOG_INF("Next step");
				return true;
			}
			leds_state_bck[event->led_id] = led->effect;
			handle_first_stream_event(led);
		}

		led->effect = event->led_effect;

		if (initialized) {
			led_update(led);
		}

		return false;
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			int err = leds_init();
			if (!err) {
				initialized = true;
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}
		return false;
	}

	if (is_wake_up_event(eh)) {
		if (!initialized) {
			leds_start();
			initialized = true;
			module_set_state(MODULE_STATE_READY);
		}

		return false;
	}

	if (is_power_down_event(eh)) {
		const struct power_down_event *event =
			cast_power_down_event(eh);

		/* Leds should keep working on system error. */
		if (event->error) {
			return false;
		}

		if (initialized) {
			leds_stop();
			initialized = false;
			module_set_state(MODULE_STATE_OFF);
		}

		return initialized;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, led_event);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);

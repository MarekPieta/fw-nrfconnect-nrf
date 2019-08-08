/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <assert.h>
#include <misc/util.h>
#include <pwm.h>

#include "power_event.h"
#include "led_event.h"

#include "leds_def.h"

#define MODULE leds
#define LED_STREEM_LOOP_SIZE 100
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_LOG_LEVEL);


struct led {
	struct device *pwm_dev;

	size_t id;
	struct led_color color;
	struct led_effect *effect;
	u16_t effect_step;
	u16_t effect_substep;

	struct k_delayed_work work;
};

static struct led leds[CONFIG_DESKTOP_LED_COUNT];


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

        if (led->effect->stream){
            led->effect->actual_step=led->effect_step;
            if (led->effect->step_count == LED_STREEM_LOOP_SIZE && led->effect->actual_step <= led->effect->next_free_step +1){
                led->effect->step_count = led->effect->next_free_step+1;
                led->effect->loop_forever = false;
            }
            LOG_INF("////// LEDS led->effect_step: %d", led->effect_step);
            LOG_INF("////// LEDS led->effect->actual_step: %d", led->effect->actual_step);
            LOG_INF("////// LEDS led->effect->step_count: %d", led->effect->step_count);
            LOG_INF("////// LEDS led->effect->next_free_step: %d", led->effect->next_free_step);
            int free_steps_left;
            if (led->effect->actual_step <= led->effect->next_free_step + 1) {
                free_steps_left = led->effect->actual_step + LED_STREEM_LOOP_SIZE - led->effect->next_free_step -1;
            } else {
                free_steps_left = led->effect->actual_step - led->effect->next_free_step;
            }
            LOG_INF("////// LEDS free steps: %d", free_steps_left);
        }

		if (led->effect_step == led->effect->step_count) {
			if (led->effect->loop_forever) {
				led->effect_step = 0;
			} else {
                if (led->effect->stream) {
                    led->effect->stream = false;
                }
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

	static_assert(ARRAY_SIZE(leds) <= ARRAY_SIZE(dev_name),
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
		struct led_event *event = cast_led_event(eh);

		__ASSERT_NO_MSG(event->led_id < CONFIG_DESKTOP_LED_COUNT);

		struct led *led = &leds[event->led_id];

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

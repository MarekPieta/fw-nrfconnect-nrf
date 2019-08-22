/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#define MODULE led_stream
#include "module_state_event.h"

#include <logging/log.h>

#include "config_event.h"
#include "led_event.h"
#include "led_ready_event.h"

#define STREAM_LED_ID 0
#define QUEUE_SIZE 5

LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_STREAM_LOG_LEVEL);

static struct led_effect led_stream_effect = {
	.step_count = 1,
};
static struct led_effect_step steps_queue[QUEUE_SIZE];
static s8_t last_added_position = -1;
static s8_t send_position;
static u8_t free_places = QUEUE_SIZE;
static bool first_sent;
static bool leds_initialized;

static void queue_data(const struct event_dyndata *dyndata)
{
	LOG_INF("Insert_data_to_effect");
	BUILD_ASSERT(CONFIG_DESKTOP_LED_COUNT == 3);
	size_t pos = 0;

	memcpy(steps_queue[last_added_position].color.c, &dyndata->data[pos],
		CONFIG_DESKTOP_LED_COUNT);
	pos += CONFIG_DESKTOP_LED_COUNT;
	steps_queue[last_added_position].substep_count = dyndata->data[pos];
	pos += sizeof(steps_queue[last_added_position].substep_count);
	steps_queue[last_added_position].substep_time = dyndata->data[pos];
}

static void send_effect(struct led_effect *effect, size_t led_id)
{
	LOG_INF("Send_effect to leds");
	struct led_event *led_event = new_led_event();

	led_event->stream = true;
	led_event->led_id = led_id;
	led_event->led_effect = effect;

	__ASSERT_NO_MSG(led_event->led_effect->steps);

	EVENT_SUBMIT(led_event);
}

static void store_data(const struct event_dyndata *data)
{
	if (free_places > 0) {
		if (last_added_position == QUEUE_SIZE - 1) {
			last_added_position = -1;
		}
		last_added_position++;
		LOG_INF("Insert data on position %u, free places %u",
			last_added_position, free_places);
		queue_data(data);
		free_places--;
	} else {
		LOG_INF("Queue is full - dropping incoming step");
	}
}

static void send_data_from_queue(void)
{
	if (free_places < QUEUE_SIZE) {
		LOG_INF("Sending data %u, %u, %u from pos %u, free places %u",
			steps_queue[send_position].color.c[0],
			steps_queue[send_position].color.c[1],
			steps_queue[send_position].color.c[2], send_position,
			free_places);

		led_stream_effect.steps = &steps_queue[send_position];
		send_effect(&led_stream_effect, STREAM_LED_ID);

		send_position++;
		if (send_position == QUEUE_SIZE) {
			send_position = 0;
		}
		free_places++;
	} else {
		LOG_INF("No steps ready in queue");
		first_sent = false;
	}
}

static void handle_incoming_step(const struct config_event *event)
{
	if (leds_initialized) {
		store_data(&event->dyndata);
		if (!first_sent) {
			LOG_INF("Sending first led effect");
			send_data_from_queue();
			first_sent = true;
		}
	} else {
		LOG_WRN("Leds module not ready");
	}
}

static void handle_led_ready_event(const struct led_ready_event *event)
{
	LOG_INF("Led ready event handler");
	send_data_from_queue();
}

static void handle_config_event(const struct config_event *event)
{
	if (!event->store_needed) {
		/* Accept only events coming from transport. */
		return;
	}

	if (GROUP_FIELD_GET(event->id) != EVENT_GROUP_LED_STREAM) {
		/* Only LED STREAM events. */
		return;
	}

	switch (TYPE_FIELD_GET(event->id)) {
	case STREAM_DATA:
		handle_incoming_step(event);
		break;

	default:
		/* Ignore unknown event. */
		LOG_WRN("Unknown LED STREAM config event");
		break;
	}
}

static void handle_config_fetch_request_event(
	const struct config_fetch_request_event *event)
{
	LOG_INF("Handle config fetch request event");

	if (GROUP_FIELD_GET(event->id) != EVENT_GROUP_LED_STREAM) {
		/* Only LED STREAM events. */
		return;
	}

	LOG_INF("Free places left: %u", free_places);

	size_t data_size = sizeof(free_places);
	struct config_fetch_event *fetch_event =
		new_config_fetch_event(data_size);

	fetch_event->id = event->id;
	fetch_event->recipient = event->recipient;
	fetch_event->channel_id = event->channel_id;

	fetch_event->dyndata.data[0] = free_places;

	EVENT_SUBMIT(fetch_event);
}

static bool event_handler(const struct event_header *eh)
{
	if (IS_ENABLED(CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE)) {
		if (is_config_event(eh)) {
			handle_config_event(cast_config_event(eh));
			return false;
		}
		if (is_config_fetch_request_event(eh)) {
			handle_config_fetch_request_event(
				cast_config_fetch_request_event(eh));
			return false;
		}
	}

	if (is_led_ready_event(eh)) {
		handle_led_ready_event(cast_led_ready_event(eh));
		return false;
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);
		if (check_state(event, MODULE_ID(leds), MODULE_STATE_READY)) {
			if (!leds_initialized) {
				leds_initialized = true;
				module_set_state(MODULE_STATE_READY);
			}
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, led_ready_event);
EVENT_SUBSCRIBE(MODULE, module_state_event);
#if CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE
EVENT_SUBSCRIBE(MODULE, config_event);
EVENT_SUBSCRIBE(MODULE, config_fetch_request_event);
#endif

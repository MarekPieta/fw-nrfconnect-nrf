/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <misc/byteorder.h>

#define MODULE led_stream
#include "module_state_event.h"

#include "led_event.h"
#include "config_event.h"
#include "ble_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_STREAM_LOG_LEVEL);

#define INCOMING_LED_COLOR_COUNT 3
#define STREAM_DATA_SIZE 8
#define FETCH_CONFIG_SIZE 2
#define LED_ID_POS 7


#if defined(CONFIG_BT_PERIPHERAL)
#define DEFAULT_LATENCY CONFIG_BT_PERIPHERAL_PREF_SLAVE_LATENCY
#else
#define DEFAULT_LATENCY 0
#endif

static struct bt_conn *active_conn;

struct led {
	size_t id;
	const struct led_effect *state_effect;
	struct led_effect led_stream_effect;
	struct led_effect_step steps_queue[CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE];
	u8_t rx_idx;
	u8_t tx_idx;
	bool streaming;
};

static struct led leds[CONFIG_DESKTOP_LED_COUNT];
static bool initialized;


static void set_ble_latency(bool low_latency)
{
	if (!active_conn) {
		LOG_INF("No active_connection");
		return;
	}

	struct bt_conn_info info;

	int err = bt_conn_get_info(active_conn, &info);
	if (err) {
		LOG_WRN("Cannot get conn info (%d)", err);
		return;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    (info.role == BT_CONN_ROLE_SLAVE)) {
		const struct bt_le_conn_param param = {
			.interval_min = info.le.interval,
			.interval_max = info.le.interval,
			.latency = (low_latency) ? (0) : (DEFAULT_LATENCY),
			.timeout = info.le.timeout
		};

		err = bt_conn_le_param_update(active_conn, &param);
		if (err) {
			LOG_WRN("Cannot update parameters (%d)", err);
			return;
		}
	}

	LOG_INF("BLE latency %screased", low_latency ? "de" : "in");
}

static size_t next_index(size_t index)
{
	return (index + 1) % CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE;
}

static bool queue_data(const struct event_dyndata *dyndata, struct led *led)
{

	size_t min_len = INCOMING_LED_COLOR_COUNT * sizeof(led->steps_queue[led->rx_idx].color.c[0])
			 + sizeof(led->steps_queue[led->rx_idx].substep_count)
			 + sizeof(led->steps_queue[led->rx_idx].substep_time)
			 + sizeof(u8_t);

	__ASSERT_NO_MSG(min_len == STREAM_DATA_SIZE);

	if (dyndata->size != min_len) {
		LOG_WRN("Invalid stream data size (%"
			PRIu8 ")", dyndata->size);
		return false;
	}

	LOG_INF("Insert data to effect");

	size_t pos = 0;

	memcpy(led->steps_queue[led->rx_idx].color.c, &dyndata->data[pos],
	       CONFIG_DESKTOP_LED_COLOR_COUNT);
	pos += INCOMING_LED_COLOR_COUNT;
	led->steps_queue[led->rx_idx].substep_count = sys_get_le16(&dyndata->data[pos]);
	pos += sizeof(led->steps_queue[led->rx_idx].substep_count);
	led->steps_queue[led->rx_idx].substep_time = sys_get_le16(&dyndata->data[pos]);

	led->rx_idx = next_index(led->rx_idx);

	return true;
}

static void send_effect(const struct led_effect *effect, struct led *led)
{
	LOG_INF("Send effect to leds");

	struct led_event *led_event = new_led_event();

	led_event->led_id = led->id;
	led_event->led_effect = effect;

	__ASSERT_NO_MSG(led_event->led_effect->steps);

	EVENT_SUBMIT(led_event);
}

static size_t count_free_places(struct led *led)
{
	size_t len = (CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE + led->rx_idx - led->tx_idx)
		     % CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE;
	return CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE - len - 1;
}

static bool store_data(const struct event_dyndata *data, struct led *led)
{
	size_t free_places = count_free_places(led);

	if (free_places > 0) {
		LOG_INF("Insert data on position %" PRIu8
			", free places %zu", led->rx_idx, free_places);

		if (!queue_data(data, led)) {
			return false;
		}
	} else {
		LOG_WRN("Queue is full - dropping incoming step");
	}
	return true;
}

static bool leds_streaming(void)
{
	bool streaming = false;
	for (size_t i = 0; i < CONFIG_DESKTOP_LED_COUNT; i++) {
		streaming |= leds[i].streaming;
	}
	return streaming;
}

static void send_data_from_queue(struct led *led)
{
	size_t free_places = count_free_places(led);

	if (free_places < CONFIG_DESKTOP_LED_STREAM_QUEUE_SIZE - 1) {
		LOG_INF("Sending data");

		led->led_stream_effect.steps = &led->steps_queue[led->tx_idx];
		led->led_stream_effect.step_count = 1;

		send_effect(&led->led_stream_effect, led);

		led->tx_idx = next_index(led->tx_idx);
	} else {
		LOG_INF("No steps ready in queue, sending previous state effect");

		if (!leds_streaming()){

			set_ble_latency(false);
		}

		led->streaming = false;

		send_effect(led->state_effect, led);
	}
}

static void handle_incoming_step(const struct config_event *event)
{
	if (!initialized) {
		LOG_WRN("Not initialized");
		return;
	}

	size_t led_id = event->dyndata.data[LED_ID_POS];
	struct led *led = &leds[led_id];

	led->id = led_id;

	if (!store_data(&event->dyndata, led)) {
		return;
	}

	if (!led->streaming) {
		LOG_INF("Sending first led effect for led %zu", led_id);

		led->streaming = true;

		send_data_from_queue(led);
	}
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
		set_ble_latency(true);
		break;

	default:
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

	size_t led_id = MOD_FIELD_GET(event->id);

	if (led_id < 0 && led_id >= CONFIG_DESKTOP_LED_COUNT) {
		LOG_WRN("Wrong LED id");
		return;
	}

	struct led *led = &leds[led_id];

	u8_t free_places = count_free_places(led);

	LOG_INF("Free places left: %" PRIu8 " for led: %zu", free_places, led_id);

	size_t size = sizeof(free_places) + sizeof(initialized);

	__ASSERT_NO_MSG(size == FETCH_CONFIG_SIZE);

	struct config_fetch_event *fetch_event =
		new_config_fetch_event(size);

	fetch_event->id = event->id;
	fetch_event->recipient = event->recipient;
	fetch_event->channel_id = event->channel_id;

	size_t pos = 0;

	fetch_event->dyndata.data[pos] = free_places;
	pos += sizeof(free_places);
	fetch_event->dyndata.data[pos] = initialized;

	EVENT_SUBMIT(fetch_event);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_config_event(eh)) {
		handle_config_event(cast_config_event(eh));
		return false;
	}

	if (is_config_fetch_request_event(eh)) {
		handle_config_fetch_request_event(cast_config_fetch_request_event(eh));
		return false;
	}

	if (is_led_event(eh)) {
		const struct led_event *event = cast_led_event(eh);

		struct led *led = &leds[event->led_id];

		if (event->led_effect != &led->led_stream_effect) {
			__ASSERT_NO_MSG(event->led_effect);

			led->state_effect = event->led_effect;

			if (led->streaming) {
				return true;
			}
		}
		return false;
	}

	if (is_led_ready_event(eh)) {
		const struct led_ready_event *event = cast_led_ready_event(eh);

		struct led *led = &leds[event->led_id];

		if (event->led_effect == &led->led_stream_effect) {
			send_data_from_queue(led);
		}

		return true;
	}

	if (is_ble_peer_event(eh)) {
		struct ble_peer_event *event = cast_ble_peer_event(eh);

		switch (event->state) {
		case PEER_STATE_CONNECTED:
			active_conn = event->id;
			break;

		case PEER_STATE_DISCONNECTED:
			active_conn = NULL;
			break;

		case PEER_STATE_SECURED:
		case PEER_STATE_CONN_FAILED:
			/* No action */
			break;

		default:
			__ASSERT_NO_MSG(false);
			break;
		}

		return false;
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);
		if (check_state(event, MODULE_ID(leds), MODULE_STATE_READY)) {
			if (!initialized) {
				initialized = true;
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
EVENT_SUBSCRIBE_EARLY(MODULE, led_event);
EVENT_SUBSCRIBE(MODULE, led_ready_event);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, config_event);
EVENT_SUBSCRIBE(MODULE, config_fetch_request_event);
EVENT_SUBSCRIBE(MODULE, ble_peer_event);

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#define MODULE led_stream
#include "module_state_event.h"

#include "button_event.h"
#include "config_event.h"
#include "led_event.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_STATE_LOG_LEVEL);

#include "led_stream_def.h"
#include "led_effect.h"

static struct led_effect led_loop_effect = LED_EFFECT_LED_LOOP();

static void insert_data_to_effect(const struct event_dyndata *dyndata, int next_free_place) {
    size_t pos = 0;
    led_loop_effect.steps[next_free_place].color.c[0] = dyndata->data[pos];
    pos += sizeof(u8_t);
    led_loop_effect.steps[next_free_place].color.c[1] = dyndata->data[pos];
    pos += sizeof(u8_t);
    led_loop_effect.steps[next_free_place].color.c[2] = dyndata->data[pos];
    pos += sizeof(u8_t);
    led_loop_effect.steps[next_free_place].substep_count = dyndata->data[pos];
    pos += sizeof(u8_t);
    led_loop_effect.steps[next_free_place].substep_time = dyndata->data[pos];
}

static u8_t calculate_free_steps() {
    u8_t free_steps_left;
    if (led_loop_effect.actual_step <= led_loop_effect.next_free_step + 1) {
        free_steps_left = led_loop_effect.actual_step + LED_STREEM_LOOP_SIZE - led_loop_effect.next_free_step -2;
    } else {
        free_steps_left = led_loop_effect.actual_step - led_loop_effect.next_free_step - 2;
    }

    LOG_INF("////// LED STREAM free steps: %d", free_steps_left);

    return free_steps_left;
}

static void add_incoming_steps(const struct config_event *event) {
    LOG_INF("////// LED STREAM add_incoming_steps");
    static bool initialized = false;

    LOG_INF("////// LED STREAM led_loop_effect.step_count: %d", led_loop_effect.step_count);
    LOG_INF("////// LED STREAM led_loop_effect.actual_step: %d", led_loop_effect.actual_step);
    LOG_INF("////// LED STREAM led_loop_effect.next_free_step: %d", led_loop_effect.next_free_step);

    if (!led_loop_effect.stream){
        LOG_INF("////// LED STREAM deinitialize - stream has ended");
        led_loop_effect.step_count = 0;
        led_loop_effect.next_free_step= 0;
        initialized = false;
    }

    LOG_INF("////// LED STREAM ////// calculate_free_steps(): %d", calculate_free_steps());
    if(calculate_free_steps() <= 0) {
        LOG_INF("////// LED STREAM no free space left, drop incoming step !!!!!!!!!!!");
        return;
    }

    if (led_loop_effect.step_count < LED_STREEM_LOOP_SIZE) {
        led_loop_effect.next_free_step = led_loop_effect.step_count;
        led_loop_effect.step_count++;
    }else{
        LOG_INF("////// LED STREAM whole loop is full - step_count = LED_STREEM_LOOP_SIZE");
        led_loop_effect.loop_forever = true;
        if (led_loop_effect.next_free_step == LED_STREEM_LOOP_SIZE-1){
            led_loop_effect.next_free_step = 0;
        }else{
            led_loop_effect.next_free_step++;
        }
    }

    LOG_INF("////// LED STREAM insert on position: %d", led_loop_effect.next_free_step);
    insert_data_to_effect(&event->dyndata, led_loop_effect.next_free_step);

    if (!initialized){
        struct led_event *led_event = new_led_event();
        led_event->led_id = LED_ID_STREAM;
        led_loop_effect.stream = true;
        led_event->led_effect = &led_loop_effect;
        EVENT_SUBMIT(led_event);
        initialized = true;
    }
}

static void handle_config_event(const struct config_event *event){

    LOG_INF("////// LED STREAM handle_config_event");

    if (!event->store_needed) {
        /* Accept only events coming from transport. */
        return;
    }

    if (GROUP_FIELD_GET(event->id) != EVENT_GROUP_LED_STREAM) {
        /* Only LED STREAM events. */
        return;
    }

    LOG_INF("////// LED STREAM config is supported");
    LOG_INF("////// LED STREAM data size: %u", event->dyndata.size);

    add_incoming_steps(event);
}
static void handle_config_fetch_request_event(const struct config_fetch_request_event *event)
{
    LOG_INF("////// LED STREAM handle_config_fetch_request_event");

    if (GROUP_FIELD_GET(event->id) != EVENT_GROUP_LED_STREAM) {
        /* Only LED STREAM events. */
        return;
    }

    LOG_INF("////// LED STREAM config is supported");

    u8_t free_steps = calculate_free_steps();

    LOG_INF("Free steps left: %" PRIu8, free_steps);

    size_t data_size = sizeof(free_steps);
    struct config_fetch_event *fetch_event =
            new_config_fetch_event(data_size);

    fetch_event->id = event->id;
    fetch_event->recipient = event->recipient;
    fetch_event->channel_id = event->channel_id;

    size_t pos = 0;

    fetch_event->dyndata.data[pos] = free_steps;
    pos += sizeof(free_steps);

    EVENT_SUBMIT(fetch_event);
}

static bool event_handler(const struct event_header *eh)
{
    LOG_INF("////// LED STREAM event handler");

    if (is_button_event(eh)) {
        LOG_INF("////// LED STREAM led_loop_effect.step_count: %d", led_loop_effect.step_count);
        LOG_INF("////// LED STREAM led_loop_effect.actual_step: %d", led_loop_effect.actual_step);
        LOG_INF("////// LED STREAM led_loop_effect.next_free_step: %d", led_loop_effect.next_free_step);
        return false;
    }

    if (IS_ENABLED(CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE)) {
        if (is_config_event(eh)) {
            handle_config_event(cast_config_event(eh));
            return false;
        }
        if (is_config_fetch_request_event(eh)) {
            handle_config_fetch_request_event(cast_config_fetch_request_event(eh));
            return false;
        }
    }

    if (is_module_state_event(eh)) {
        return false;
    }

    /* If event is unhandled, unsubscribe. */
    __ASSERT_NO_MSG(false);

    return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, button_event);
#if CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE
EVENT_SUBSCRIBE(MODULE, config_event);
EVENT_SUBSCRIBE(MODULE, config_fetch_request_event);
#endif

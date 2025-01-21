/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <limits.h>
#include <sys/types.h>

#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include <caf/events/button_event.h>
#include "motion_event.h"
#include "wheel_event.h"
#include "hid_event.h"
#include "hid_report_provider_event.h"

#include "hid_keymap.h"
#include "hid_report_desc.h"

#define MODULE hid_provider_mouse
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_HID_REPORT_PROVIDER_MOUSE_LOG_LEVEL);

/* Make sure that mouse buttons would fit in button bitmask. */
BUILD_ASSERT(MOUSE_REPORT_BUTTON_COUNT_MAX <= BITS_PER_BYTE);

struct report_data {
	uint8_t button_bm; /* Bitmask of pressed mouse buttons. */
	int16_t axes[MOUSE_REPORT_AXIS_COUNT]; /**< Array of axes (motion X, motion Y, wheel). */
	bool update_needed;
};

static const struct hid_state_fops *hid_state_fops;
static struct report_data report_data;
static bool report_mouse_connected;
static bool report_boot_mouse_connected;


static void clear_report_data(struct report_data *rd)
{
	LOG_INF("Clear report data");

	rd->button_bm = 0;
	memset(&rd->axes, 0x00, sizeof(rd->axes));
	rd->update_needed = false;
}

static bool is_send_needed(uint8_t report_id)
{
	__ASSERT_NO_MSG((report_id == REPORT_ID_MOUSE) ||
			(report_id == REPORT_ID_BOOT_MOUSE));

	return report_data.update_needed;
}

static bool send_empty_report_mouse(uint8_t report_id, bool force, const void *source,
				    const void *subscriber)
{
	__ASSERT_NO_MSG(report_id == REPORT_ID_MOUSE);

	if (!force && !is_send_needed(report_id)) {
		return false;
	}

	struct hid_report_event *event = new_hid_report_event(sizeof(report_id)
							      + REPORT_SIZE_MOUSE);

	event->source = source;
	event->subscriber = subscriber;

	event->dyndata.data[0] = report_id;
	memset(&event->dyndata.data[1], 0x00, REPORT_SIZE_MOUSE);

	APP_EVENT_SUBMIT(event);

	return true;
}

static bool send_report_mouse(uint8_t report_id, bool force, const void *source,
			      const void *subscriber)
{
	__ASSERT_NO_MSG(report_id == REPORT_ID_MOUSE);
	__ASSERT_NO_MSG(report_mouse_connected);

	if (!force && !is_send_needed(report_id)) {
		return false;
	}

	struct report_data *rd = &report_data;

	/* X/Y axis */
	int16_t dx = CLAMP(rd->axes[MOUSE_REPORT_AXIS_X],
			   MOUSE_REPORT_XY_MIN, MOUSE_REPORT_XY_MAX);
	int16_t dy = CLAMP(-rd->axes[MOUSE_REPORT_AXIS_Y],
			   MOUSE_REPORT_XY_MIN, MOUSE_REPORT_XY_MAX);

	/* Wheel */
	int16_t wheel = CLAMP(rd->axes[MOUSE_REPORT_AXIS_WHEEL] / 2,
			      MOUSE_REPORT_WHEEL_MIN, MOUSE_REPORT_WHEEL_MAX);

	/* Button bitmask. */
	uint8_t button_bm = rd->button_bm;

	/* Update stored report data. */
	if (dx) {
		rd->axes[MOUSE_REPORT_AXIS_X] -= dx;
	}
	if (dy) {
		rd->axes[MOUSE_REPORT_AXIS_Y] += dy;
	}
	if (wheel) {
		rd->axes[MOUSE_REPORT_AXIS_WHEEL] -= wheel * 2;
	}

	/* Encode report. */
	BUILD_ASSERT(REPORT_SIZE_MOUSE == 5, "Invalid report size");

	struct hid_report_event *event = new_hid_report_event(sizeof(report_id)
							      + REPORT_SIZE_MOUSE);

	event->source = source;
	event->subscriber = subscriber;

	/* Convert to little-endian. */
	uint8_t x_buff[sizeof(dx)];
	uint8_t y_buff[sizeof(dy)];
	sys_put_le16(dx, x_buff);
	sys_put_le16(dy, y_buff);

	event->dyndata.data[0] = report_id;
	event->dyndata.data[1] = button_bm;
	event->dyndata.data[2] = wheel;
	event->dyndata.data[3] = x_buff[0];
	event->dyndata.data[4] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
	event->dyndata.data[5] = (y_buff[1] << 4) | (y_buff[0] >> 4);

	APP_EVENT_SUBMIT(event);

	if ((rd->axes[MOUSE_REPORT_AXIS_X] != 0) || (rd->axes[MOUSE_REPORT_AXIS_Y] != 0) ||
	    (rd->axes[MOUSE_REPORT_AXIS_WHEEL] < -1) || (rd->axes[MOUSE_REPORT_AXIS_WHEEL] > 1)) {
		/* If there is some axis data to send, request report update. */
		rd->update_needed = true;
	} else {
		rd->update_needed = false;
	}

	return true;
}

static bool send_empty_report_boot_mouse(uint8_t report_id, bool force, const void *source,
					 const void *subscriber)
{
	__ASSERT_NO_MSG(report_id == REPORT_ID_BOOT_MOUSE);

	if (!IS_ENABLED(CONFIG_DESKTOP_HID_BOOT_INTERFACE_MOUSE)) {
		/* Not supported. */
		__ASSERT_NO_MSG(false);
		return false;
	}

	if (!force && !is_send_needed(report_id)) {
		return false;
	}

	size_t report_size = REPORT_SIZE_MOUSE_BOOT;
	struct hid_report_event *event = new_hid_report_event(sizeof(report_id) + report_size);

	event->source = source;
	event->subscriber = subscriber;

	event->dyndata.data[0] = report_id;
	memset(&event->dyndata.data[1], 0x00, report_size);

	APP_EVENT_SUBMIT(event);

	return true;
}

static bool send_report_boot_mouse(uint8_t report_id, bool force, const void *source,
				   const void *subscriber)
{
	__ASSERT_NO_MSG(report_id == REPORT_ID_BOOT_MOUSE);
	__ASSERT_NO_MSG(report_boot_mouse_connected);

	if (!IS_ENABLED(CONFIG_DESKTOP_HID_BOOT_INTERFACE_MOUSE)) {
		/* Not supported. */
		__ASSERT_NO_MSG(false);
		return false;
	}

	if (!force && !is_send_needed(report_id)) {
		return false;
	}

	struct report_data *rd = &report_data;

	/* X/Y axis */
	int8_t dx = CLAMP(rd->axes[MOUSE_REPORT_AXIS_X], INT8_MIN, INT8_MAX);
	int8_t dy = CLAMP(-rd->axes[MOUSE_REPORT_AXIS_Y], INT8_MIN, INT8_MAX);

	/* Button bitmask. */
	uint8_t button_bm = rd->button_bm;

	if (dx) {
		rd->axes[MOUSE_REPORT_AXIS_X] -= dx;
	}
	if (dy) {
		rd->axes[MOUSE_REPORT_AXIS_Y] += dy;
	}
	rd->axes[MOUSE_REPORT_AXIS_WHEEL] = 0;

	size_t report_size = sizeof(report_id) + sizeof(button_bm) + sizeof(dx) + sizeof(dy);
	struct hid_report_event *event = new_hid_report_event(report_size);

	event->source = source;
	event->subscriber = subscriber;

	event->dyndata.data[0] = report_id;
	event->dyndata.data[1] = button_bm;
	event->dyndata.data[2] = dx;
	event->dyndata.data[3] = dy;

	APP_EVENT_SUBMIT(event);

	if ((rd->axes[MOUSE_REPORT_AXIS_X] != 0) || (rd->axes[MOUSE_REPORT_AXIS_Y] != 0)) {
		/* If there is some axis data to send, request report update. */
		rd->update_needed = true;
	} else {
		rd->update_needed = false;
	}

	return true;
}

static void mouse_report_connection_state(uint8_t report_id, bool connected)
{
	LOG_INF("Report 0x%" PRIx8 " %s", report_id, (connected) ? "connected" : "disconnected");

	switch (report_id) {
	case REPORT_ID_MOUSE:
		report_mouse_connected = connected;
		break;
	case REPORT_ID_BOOT_MOUSE:
		report_boot_mouse_connected = connected;
		break;
	default:
		/* Not supported. */
		__ASSERT_NO_MSG(false);
		break;
	}

	/* Both HID report and HID boot protocols cannot be active simultaneously. */
	__ASSERT_NO_MSG(!(report_mouse_connected && report_boot_mouse_connected));

	if (!connected) {
		/* Clear whole report data. */
		clear_report_data(&report_data);
	} else {
		/* Clear axes. */
		memset(&report_data.axes, 0x00, sizeof(report_data.axes));
	}
}

static void trigger_report_transmission(void)
{
	/* Mark that update is needed. */
	report_data.update_needed = true;

	if (report_mouse_connected) {
		hid_state_fops->trigger_report_send(REPORT_ID_MOUSE);
	} else if (report_boot_mouse_connected) {
		hid_state_fops->trigger_report_send(REPORT_ID_BOOT_MOUSE);
	} else {
		LOG_DBG("Subscription not enabled");
	}
}

static void update_key(uint16_t usage_id, bool pressed)
{
	bool connected = report_mouse_connected || report_boot_mouse_connected;

	if (!connected) {
		/* Ignore keypresses while not connected. */
		return;
	}

	__ASSERT_NO_MSG((usage_id >= 1) && (usage_id <= 8));
	uint8_t bit_pos = usage_id - 1;

	/* Module does not support multiple HW buttons mapped to the same HID usage ID. */
	__ASSERT_NO_MSG(!(pressed && IS_BIT_SET(report_data.button_bm, bit_pos)));
	WRITE_BIT(report_data.button_bm, bit_pos, pressed);

	trigger_report_transmission();
}

static void init(void)
{
	static const struct hid_report_provider_fops report_fops_mouse = {
		.send_report = send_report_mouse,
		.send_empty_report = send_empty_report_mouse,
		.connection_state = mouse_report_connection_state,
	};

	struct hid_report_provider_event *rp;

	rp = new_hid_report_provider_event();
	rp->report_id = REPORT_ID_MOUSE;
	rp->p_fops = &report_fops_mouse;
	rp->hs_fops = NULL;
	APP_EVENT_SUBMIT(rp);

	if (IS_ENABLED(CONFIG_DESKTOP_HID_BOOT_INTERFACE_MOUSE)) {
		static const struct hid_report_provider_fops report_fops_boot_mouse = {
			.send_report = send_report_boot_mouse,
			.send_empty_report = send_empty_report_boot_mouse,
			.connection_state = mouse_report_connection_state,
		};

		rp = new_hid_report_provider_event();
		rp->report_id = REPORT_ID_BOOT_MOUSE;
		rp->p_fops = &report_fops_boot_mouse;
		rp->hs_fops = NULL;
		APP_EVENT_SUBMIT(rp);
	}
}

static bool handle_motion_event(const struct motion_event *event)
{
	report_data.axes[MOUSE_REPORT_AXIS_X] += event->dx;
	report_data.axes[MOUSE_REPORT_AXIS_Y] += event->dy;

	trigger_report_transmission();

	return false;
}

static bool handle_wheel_event(const struct wheel_event *event)
{
	report_data.axes[MOUSE_REPORT_AXIS_WHEEL] += event->wheel;

	trigger_report_transmission();

	return false;
}

static bool handle_button_event(const struct button_event *event)
{
	/* Get usage ID and target report from HID Keymap */
	const struct hid_keymap *map = hid_keymap_get(event->key_id);

	/* Boot mouse report uses mouse report ID in the key map. */
	if (map && (map->report_id == REPORT_ID_MOUSE)) {
		update_key(map->usage_id, event->pressed);
	}

	return false;
}

static bool handle_module_state_event(const struct module_state_event *event)
{
	if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
		static bool initialized;

		__ASSERT_NO_MSG(!initialized);
		initialized = true;

		LOG_INF("Init mouse report provider.");
		init();
	}

	return false;
}

static bool handle_hid_report_provider_event(const struct hid_report_provider_event *event)
{
	if (event->report_id == REPORT_ID_MOUSE) {
		__ASSERT_NO_MSG(event->p_fops->connection_state == mouse_report_connection_state);
		__ASSERT_NO_MSG(!hid_state_fops);
		hid_state_fops = event->hs_fops;
	} else if (event->report_id == REPORT_ID_BOOT_MOUSE) {
		__ASSERT_NO_MSG(event->p_fops->connection_state == mouse_report_connection_state);
		__ASSERT_NO_MSG(hid_state_fops == event->hs_fops);
	}

	return false;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (!IS_ENABLED(CONFIG_DESKTOP_MOTION_NONE) &&
	    is_motion_event(aeh)) {
		return handle_motion_event(cast_motion_event(aeh));
	}

	if (IS_ENABLED(CONFIG_DESKTOP_WHEEL_ENABLE) &&
	    is_wheel_event(aeh)) {
		return handle_wheel_event(cast_wheel_event(aeh));
	}

	if (IS_ENABLED(CONFIG_CAF_BUTTON_EVENTS) &&
	    is_button_event(aeh)) {
		return handle_button_event(cast_button_event(aeh));
	}

	if (is_module_state_event(aeh)) {
		return handle_module_state_event(cast_module_state_event(aeh));
	}

	if (is_hid_report_provider_event(aeh)) {
		return handle_hid_report_provider_event(cast_hid_report_provider_event(aeh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
#ifndef CONFIG_DESKTOP_MOTION_NONE
APP_EVENT_SUBSCRIBE(MODULE, motion_event);
#endif /* !CONFIG_DESKTOP_MOTION_NONE */
#ifdef CONFIG_DESKTOP_WHEEL_ENABLE
APP_EVENT_SUBSCRIBE(MODULE, wheel_event);
#endif /* CONFIG_DESKTOP_WHEEL_ENABLE */
#ifdef CONFIG_CAF_BUTTON_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, button_event);
#endif /* CONFIG_CAF_BUTTON_EVENTS */
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, hid_report_provider_event);

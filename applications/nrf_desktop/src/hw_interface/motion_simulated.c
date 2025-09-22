/* Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

#include <app_event_manager.h>
#include "motion_event.h"
#include <caf/events/power_event.h>
#include "hid_event.h"
#include "config_event.h"

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_rtt.h>

#define MODULE motion
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_MOTION_LOG_LEVEL);

#define SCALE CONFIG_DESKTOP_MOTION_SIMULATED_SCALE_FACTOR

#define MODULE_VARIANT	"sim"

enum {
	STATE_IDLE,
	STATE_FETCHING,
	STATE_SUSPENDED,
};

struct vertex {
	int x;
	int y;
};

/* Coords sequence for generated trajectory */
const static struct vertex coords[] = {
	{-50 * SCALE,	0},
	{-35 * SCALE,	-35 * SCALE},
	{0,		-50 * SCALE},
	{35 * SCALE,	-35 * SCALE},
	{50 * SCALE,	0},
	{35 * SCALE,	35 * SCALE},
	{0,		50 * SCALE},
	{-35 * SCALE,	35 * SCALE},
};

/* Time for transition between two points from trajectory */
const static int edge_time = CONFIG_DESKTOP_MOTION_SIMULATED_EDGE_TIME;

static int x_cur;
static int y_cur;
static atomic_t state;
static atomic_t connected;

static uint32_t motion_sim_busy_wait_us = CONFIG_DESKTOP_MOTION_SIMULATED_BUSY_WAIT_US;

enum sensor_opt {
        SENSOR_OPT_VARIANT,
        SENSOR_OPT_ACTIVE,
        SENSOR_OPT_BUSY_WAIT_US,

        SENSOR_OPT_COUNT
};

static const char * const opt_descr[] = {
        [SENSOR_OPT_VARIANT] = OPT_DESCR_MODULE_VARIANT,
        [SENSOR_OPT_ACTIVE] = "active",
        [SENSOR_OPT_BUSY_WAIT_US] = "busy_wait_us",
};

static void set_default_state(void)
{
	if (IS_ENABLED(CONFIG_SHELL) || IS_ENABLED(CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE)) {
		atomic_set(&state, STATE_IDLE);
	} else {
		atomic_set(&state, STATE_FETCHING);
	}
}

static void motion_event_send(int16_t dx, int16_t dy)
{
	struct motion_event *event = new_motion_event();

	event->dx = dx;
	event->dy = dy;
	APP_EVENT_SUBMIT(event);
}

static uint64_t get_timestamp(void)
{
	/* Use higher resolution if available. */
	if (IS_ENABLED(CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER)) {
		return k_cyc_to_us_near64(k_cycle_get_64());
	} else {
		return k_cyc_to_us_near64(k_cycle_get_32());
	}
}

static void generate_motion_event(void)
{
	BUILD_ASSERT((edge_time & (edge_time - 1)) == 0,
			 "Edge time must be power of 2");

	uint64_t t = get_timestamp();
	size_t v1_id = (t / edge_time) % ARRAY_SIZE(coords);
	size_t v2_id = (v1_id + 1) % ARRAY_SIZE(coords);

	const struct vertex *v1 = &coords[v1_id];
	const struct vertex *v2 = &coords[v2_id];

	int edge_pos = t % edge_time;
	int x_new = v1->x + (v2->x - v1->x) * edge_pos / edge_time;
	int y_new = v1->y + (v2->y - v1->y) * edge_pos / edge_time;

	k_busy_wait(motion_sim_busy_wait_us);

	motion_event_send(x_new - x_cur, y_new - y_cur);

	x_cur = x_new;
	y_cur = y_new;
}

static void update_config(const uint8_t opt_id, const uint8_t *data, const size_t size)
{
	switch (opt_id) {
	case SENSOR_OPT_ACTIVE:
		/* TODO check size!! */
		if (data[0] == 0x01) {
			if (atomic_cas(&state, STATE_IDLE, STATE_FETCHING)) {
				if (atomic_get(&connected)) {
					generate_motion_event();
				}
			}
		} if (data[0] == 0x00) {
			atomic_cas(&state, STATE_FETCHING, STATE_IDLE);
		}
		break;

	case SENSOR_OPT_BUSY_WAIT_US:
		if (size == sizeof(motion_sim_busy_wait_us)) {
			motion_sim_busy_wait_us = sys_get_le32(data);
		} else {
			LOG_WRN("Invalid data size of SENSOR_OPT_BUSY_WAIT_US: %zu", size);
		}
		break;

	default:
		LOG_WRN("Unknown opt %" PRIu8, opt_id);
		return;
	}
}

static void fetch_config(const uint8_t opt_id, uint8_t *data, size_t *size)
{
	switch (opt_id) {
	case SENSOR_OPT_VARIANT:
		*size = strlen(MODULE_VARIANT);
		__ASSERT_NO_MSG((*size != 0) && (*size < CONFIG_CHANNEL_FETCHED_DATA_MAX_SIZE));
		strcpy(data, MODULE_VARIANT);
		break;

	case SENSOR_OPT_ACTIVE:
		data[0] = (atomic_get(&state) == STATE_FETCHING) ? (0x01) : (0x00);
		*size = 1;
		break;

	case SENSOR_OPT_BUSY_WAIT_US:
		sys_put_le32(motion_sim_busy_wait_us, data);
		*size = sizeof(motion_sim_busy_wait_us);
		break;

	default:
		LOG_WRN("Unknown opt: %" PRIu8, opt_id);
	}
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_hid_report_subscription_event(aeh)) {
		const struct hid_report_subscription_event *event =
			cast_hid_report_subscription_event(aeh);

		if ((event->report_id == REPORT_ID_MOUSE) ||
		    (event->report_id == REPORT_ID_BOOT_MOUSE)) {
			static uint8_t peer_count;

			if (event->enabled) {
				__ASSERT_NO_MSG(peer_count < UCHAR_MAX);
				peer_count++;
			} else {
				__ASSERT_NO_MSG(peer_count > 0);
				peer_count--;
			}

			bool new_state = (peer_count != 0);
			bool old_state = atomic_set(&connected, new_state);

			if (old_state != new_state && new_state) {
				if (atomic_get(&state) == STATE_FETCHING) {
					generate_motion_event();
				}
			}
		}
		return false;
	}

	if (is_hid_report_sent_event(aeh)) {
		const struct hid_report_sent_event *event =
			cast_hid_report_sent_event(aeh);

		if ((event->report_id == REPORT_ID_MOUSE) ||
		    (event->report_id == REPORT_ID_BOOT_MOUSE)) {
			if (atomic_get(&state) == STATE_FETCHING) {
				generate_motion_event();
			}
		}

		return false;
	}

	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			set_default_state();
			LOG_INF("Simulated motion: ready");
		}

		return false;
	}

	if (IS_ENABLED(CONFIG_DESKTOP_MOTION_PM_EVENTS) &&
	    is_power_down_event(aeh)) {
		atomic_set(&state, STATE_SUSPENDED);

		return false;
	}

	if (IS_ENABLED(CONFIG_DESKTOP_MOTION_PM_EVENTS) &&
	    is_wake_up_event(aeh)) {
		set_default_state();

		return false;
	}

	GEN_CONFIG_EVENT_HANDLERS(STRINGIFY(MODULE), opt_descr, update_config, fetch_config);

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
#if CONFIG_DESKTOP_MOTION_PM_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, power_down_event);
APP_EVENT_SUBSCRIBE(MODULE, wake_up_event);
#endif
APP_EVENT_SUBSCRIBE(MODULE, hid_report_sent_event);
APP_EVENT_SUBSCRIBE(MODULE, hid_report_subscription_event);
#if CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE
APP_EVENT_SUBSCRIBE_EARLY(MODULE, config_event);
#endif

#if CONFIG_SHELL

static int start_motion(const struct shell *shell, size_t argc, char **argv)
{
	if (!atomic_cas(&state, STATE_IDLE, STATE_FETCHING)) {
		shell_print(shell, "Simulated motion inactive"
				    " or already fetching");
		return 0;
	}
	if (atomic_get(&connected)) {
		generate_motion_event();
	}
	shell_print(shell, "Started generating simulated motion");

	return 0;
}

static int stop_motion(const struct shell *shell, size_t argc, char **argv)
{
	if (!atomic_cas(&state, STATE_FETCHING, STATE_IDLE)) {
		shell_print(shell, "Simulated motion is not fetching");
		return 0;
	}
	shell_print(shell, "Stopped generating simulated motion");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_motion_sim,
	SHELL_CMD(start, NULL, "Start motion", start_motion),
	SHELL_CMD(stop, NULL, "Stop motion", stop_motion),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(motion_sim, &sub_motion_sim,
		   "Simulated motion commands", NULL);

#endif /* CONFIG_SHELL */

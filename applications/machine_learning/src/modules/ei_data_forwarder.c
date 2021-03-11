/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>
#include <bluetooth/services/nus.h>

#include <caf/events/sensor_event.h>
#include <caf/events/ble_common_event.h>

#define MODULE ei_data_forwarder
#include <caf/events/module_state_event.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_ML_APP_EI_DATA_FORWARDER_LOG_LEVEL);

#if defined(CONFIG_ML_APP_EI_DATA_FORWARDER_UART)
	#define UART_LABEL		CONFIG_ML_APP_EI_DATA_FORWARDER_UART_DEV
#else
	#define UART_LABEL		""
#endif

#define UART_BUF_SIZE		CONFIG_ML_APP_EI_DATA_FORWARDER_BUF_SIZE

#define MIN_CONN_INT		CONFIG_BT_PERIPHERAL_PREF_MIN_INT
#define MAX_CONN_INT		CONFIG_BT_PERIPHERAL_PREF_MAX_INT

enum {
	CONN_SECURED			= BIT(0),
	CONN_INTERVAL_VALID		= BIT(1),
	CONN_ERROR			= BIT(2),
};

static uint8_t conn_state;

static const struct device *dev;
static struct bt_conn *nus_conn;
static atomic_t uart_busy;

enum state {
	STATE_DISABLED,
	STATE_ACTIVE,
	STATE_ERROR
};

static enum state state;


static void report_error(void)
{
	state = STATE_ERROR;
	module_set_state(MODULE_STATE_ERROR);
}

static int snprintf_error_check(int res, size_t buf_size)
{
	if ((res < 0) || (res >= buf_size)) {
		LOG_ERR("snprintf returned %d", res);
		report_error();
		return res;
	}

	return 0;
}

static bool is_nus_conn_valid(struct bt_conn *conn, uint8_t conn_state)
{
	if (!conn) {
		return false;
	}

	if (conn_state & CONN_ERROR) {
		return false;
	}

	if (!(conn_state & CONN_INTERVAL_VALID)) {
		return false;
	}

	if (!(conn_state & CONN_SECURED)) {
		return false;
	}

	return true;
}

static bool handle_sensor_event(const struct sensor_event *event)
{
	if (state != STATE_ACTIVE) {
		return false;
	}

	if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS)) {
		if (!is_nus_conn_valid(nus_conn, conn_state)) {
			return false;
		}
	}

	__ASSERT_NO_MSG(sensor_event_get_data_cnt(event) > 0);

	/* Ensure that previous sensor_event was sent. */
	if (!atomic_cas(&uart_busy, false, true)) {
		LOG_ERR("UART not ready");
		LOG_ERR("Sampling frequency is too high");
		report_error();
		return false;
	}

	static uint8_t buf[UART_BUF_SIZE];
	const float *data_ptr = sensor_event_get_data_ptr(event);
	size_t data_cnt = sensor_event_get_data_cnt(event);
	int pos = 0;
	int tmp;

	for (size_t i = 0; i < 2 * data_cnt; i++) {
		if ((i % 2) == 0) {
			tmp = snprintf(&buf[pos], sizeof(buf) - pos, "%.2f", data_ptr[i / 2]);
		} else if (i == (2 * data_cnt - 1)) {
			tmp = snprintf(&buf[pos], sizeof(buf) - pos, "\r\n");
		} else {
			tmp = snprintf(&buf[pos], sizeof(buf) - pos, ",");
		}

		if (snprintf_error_check(tmp, sizeof(buf) - pos)) {
			return false;
		}
		pos += tmp;
	}

	if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_UART)) {
		int err = uart_tx(dev, buf, pos, SYS_FOREVER_MS);

		if (err) {
			LOG_ERR("uart_tx error: %d", err);
			report_error();
		}
	} else if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS)) {
		if (bt_nus_get_mtu(nus_conn) < pos) {
			atomic_cas(&uart_busy, true, false);
			LOG_WRN("GATT MTU too small");
			return false;
		}

		int err = bt_nus_send(nus_conn, buf, pos);

		if (err) {
			LOG_ERR("bt_nus_tx error: %d", err);
			atomic_cas(&uart_busy, true, false);
                        //report_error();
                }
	}

	return false;
}

static void uart_cb(const struct device *dev, struct uart_event *evt,
		    void *user_data)
{
	if (evt->type == UART_TX_DONE) {
		atomic_cas(&uart_busy, true, false);
	}
}

static int init_uart(void)
{
	dev = device_get_binding(UART_LABEL);

	if (!dev) {
		LOG_ERR("UART device binding failed");
		return -ENXIO;
	}

	int err = uart_callback_set(dev, uart_cb, NULL);

	if (err) {
		LOG_ERR("Cannot set UART callback (err %d)", err);
	}

	return err;
}

void bt_nus_sent_cb(struct bt_conn *conn)
{
	atomic_cas(&uart_busy, true, false);
}

static struct bt_nus_cb nus_cb = {
	.sent = bt_nus_sent_cb,
};

static int init_nus(void)
{
	int err = bt_nus_init(&nus_cb);

	if (err) {
		LOG_ERR("Cannot initialize NUS (err %d)", err);
		report_error();
	}

	return err;
}

static bool handle_module_state_event(const struct module_state_event *event)
{
	int err = 0;

	if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_UART) &&
	    check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {

		__ASSERT_NO_MSG(state == STATE_DISABLED);
		err = init_uart();

		if (!err) {
			state = STATE_ACTIVE;
			module_set_state(MODULE_STATE_READY);
		} else {
			report_error();
		}
	} else if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS) &&
		  check_state(event, MODULE_ID(ble_state), MODULE_STATE_READY)) {

		__ASSERT_NO_MSG(state == STATE_DISABLED);
		err = init_nus();

		if (!err) {
			state = STATE_ACTIVE;
			module_set_state(MODULE_STATE_READY);
		} else {
			report_error();
		}
	}

	return false;
}

static bool handle_ble_peer_event(const struct ble_peer_event *event)
{
	switch (event->state) {
	case PEER_STATE_CONNECTED:
		nus_conn = event->id;
		break;

	case PEER_STATE_SECURED:
		__ASSERT_NO_MSG(nus_conn);
		conn_state |= CONN_SECURED;
		break;

	case PEER_STATE_DISCONNECTED:
	case PEER_STATE_DISCONNECTING:
		conn_state &= ~CONN_SECURED;
		nus_conn = NULL;
		break;

	default:
		break;
	}

	return false;
}

static bool handle_ble_peer_conn_params_event(const struct ble_peer_conn_params_event *event)
{
	if (!event->updated) {
		return false;
	}

	__ASSERT_NO_MSG(event->interval_min == event->interval_max);

	uint16_t interval = event->interval_min;

	if ((interval >= MIN_CONN_INT) && (interval <= MAX_CONN_INT)) {
		conn_state |= CONN_INTERVAL_VALID;
	} else {
		conn_state &= ~CONN_INTERVAL_VALID;
	}

	return false;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sensor_event(eh)) {
		return handle_sensor_event(cast_sensor_event(eh));
	}

	if (is_module_state_event(eh)) {
		return handle_module_state_event(cast_module_state_event(eh));
	}

	if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS) &&
	    is_ble_peer_event(eh)) {
		return handle_ble_peer_event(cast_ble_peer_event(eh));
	}

	if (IS_ENABLED(CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS) &&
	    is_ble_peer_conn_params_event(eh)) {
		return handle_ble_peer_conn_params_event(cast_ble_peer_conn_params_event(eh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, sensor_event);
#ifdef CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS
EVENT_SUBSCRIBE(MODULE, ble_peer_event);
EVENT_SUBSCRIBE(MODULE, ble_peer_conn_params_event);
#endif /* CONFIG_ML_APP_EI_DATA_FORWARDER_BT_NUS */

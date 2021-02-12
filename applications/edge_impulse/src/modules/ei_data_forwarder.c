/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>

#include "sensor_event.h"

#define MODULE ei_data_forwarder
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_EI_APP_DATA_FORWARDER_LOG_LEVEL);

#define UART_LABEL		DT_LABEL(DT_NODELABEL(uart0))
#define UART_BUF_SIZE		CONFIG_EI_APP_DATA_FORWARDER_BUF_SIZE
#define DATA_FORMAT		"%.2f"

static const struct device *dev;
static atomic_t uart_busy;


static void report_error(void)
{
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

static bool handle_sensor_event(const struct sensor_event *event)
{
	/* Ensure that previous sensor_event was sent. */
	if (!atomic_cas(&uart_busy, false, true)) {
		LOG_ERR("UART not ready");
		LOG_ERR("Sampling frequency is too high");
		report_error();
		return false;
	}

	static uint8_t buf[UART_BUF_SIZE];
	const float *data_ptr = sensor_event_get_data_ptr(event);
	int pos = 0;
	int tmp = snprintf(&buf[pos], sizeof(buf), DATA_FORMAT, *data_ptr);

	if (snprintf_error_check(tmp, sizeof(buf))) {
		return false;
	}
	pos += tmp;

	for (size_t i = 1; i < sensor_event_get_data_cnt(event); i++) {
		tmp = snprintf(&buf[pos], sizeof(buf) - pos, "," DATA_FORMAT,
			       *(data_ptr + i));

		if (snprintf_error_check(tmp, sizeof(buf) - pos)) {
			return false;
		}
		pos += tmp;
	}

	tmp = snprintf(&buf[pos], sizeof(buf) - pos, "\r\n");

	if (snprintf_error_check(tmp, sizeof(buf) - pos)) {
		return false;
	}
	pos += tmp;

	int err = uart_tx(dev, buf, pos, SYS_FOREVER_MS);

	if (err) {
		LOG_ERR("uart_tx error: %d", err);
		report_error();
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

static int init_fn(void)
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

static bool event_handler(const struct event_header *eh)
{
	if (is_sensor_event(eh)) {
		return handle_sensor_event(cast_sensor_event(eh));
	}

	if (is_module_state_event(eh)) {
		const struct module_state_event *event = cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			if (!init_fn()) {
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}

		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, sensor_event);

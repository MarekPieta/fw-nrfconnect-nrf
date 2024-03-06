/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/drivers/gpio.h>

#include "coremark_zephyr.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#ifndef CONFIG_APP_MODE_FLASH_AND_RUN
/*
 * Get button configuration from the devicetree. This is mandatory.
 */
#define BUTTON_NODE             DT_ALIAS(button)
#define LED_NODE                DT_ALIAS(led)

#if !DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
	#error "Unsupported board: /"button/" alias is not defined."
#endif

#if !DT_NODE_HAS_STATUS(LED_NODE, okay)
	#error "Unsupported board: /"led/" alias is not defined."
#endif

#define BUTTON_LABEL            DT_PROP(DT_ALIAS(button), label)
#define LED_ON()                gpio_pin_set_dt(&status_led, GPIO_ACTIVE_LOW)
#define LED_OFF()               gpio_pin_set_dt(&status_led, GPIO_ACTIVE_HIGH)

static const struct gpio_dt_spec start_button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

static struct gpio_callback button_cb_data;

#else

#define BUTTON_LABEL            "Reset"
#define LED_ON()
#define LED_OFF()

#endif

static K_SEM_DEFINE(start_coremark, 0, 1);
static bool coremark_in_progress;

static void flush_log(void)
{
	if (IS_ENABLED(CONFIG_LOG_PROCESS_THREAD)) {
		while (log_data_pending()) {
			k_sleep(K_MSEC(10));
		}
		k_sleep(K_MSEC(10));
	} else {
		while (LOG_PROCESS()) {
		}
	}
}

#ifndef CONFIG_APP_MODE_FLASH_AND_RUN

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if (gpio_pin_get_dt(&start_button) && !coremark_in_progress) {
		LOG_INF("%s pressed!", BUTTON_LABEL);
		coremark_in_progress = true;
		k_sem_give(&start_coremark);
	}
}

static void led_init(void)
{
	int ret;

	if (!device_is_ready(status_led.port)) {
		LOG_INF("Error: led device %s is not ready",
			status_led.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		LOG_INF("Error %d: failed to configure %s pin %d",
			ret, status_led.port->name, status_led.pin);
		return;
	}

	gpio_pin_set_dt(&status_led, GPIO_ACTIVE_HIGH);
}

static void button_init(void)
{
	int ret;

	if (!device_is_ready(start_button.port)) {
		LOG_INF("Error: button1 device %s is not ready",
			start_button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&start_button, GPIO_INPUT);
	if (ret != 0) {
		LOG_INF("Error %d: failed to configure %s pin %d",
			ret, start_button.port->name, start_button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&start_button, GPIO_INT_EDGE_BOTH);

	if (ret != 0) {
		LOG_INF("Error %d: failed to configure interrupt on %s pin %d",
			ret, start_button.port->name, start_button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(start_button.pin));
	gpio_add_callback(start_button.port, &button_cb_data);
}
#endif

int main(void)
{
	LOG_INF("CoreMark sample for %s", CONFIG_BOARD);

	if (IS_ENABLED(CONFIG_APP_MODE_FLASH_AND_RUN) || IS_ENABLED(CONFIG_SOC_NRF54H20_CPUPPR)) {
		coremark_in_progress = true;
		k_sem_give(&start_coremark);
	} else if (!IS_ENABLED(CONFIG_APP_MODE_FLASH_AND_RUN)) {
		led_init();
		button_init();

		LOG_INF("%s to start the test ...",  BUTTON_LABEL);
	}

	while (true) {
		k_sem_take(&start_coremark, K_FOREVER);
		LOG_INF("CoreMark started!");
		LOG_INF("CPU FREQ: %d Hz", SystemCoreClock);
		LOG_INF("(threads: %d, data size: %d; iterations: %d)\n",
			CONFIG_COREMARK_THREADS_NUMBER,
			CONFIG_COREMARK_DATA_SIZE,
			CONFIG_COREMARK_ITERATIONS);
		flush_log();

		LED_ON();
		coremark_run();
		LED_OFF();

		LOG_INF("CoreMark finished! %s to restart the test ...\n", BUTTON_LABEL);

		coremark_in_progress = false;
	};

	return 0;
}

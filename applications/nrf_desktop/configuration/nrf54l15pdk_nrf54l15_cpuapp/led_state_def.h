/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "led_state.h"
#include <caf/led_effect.h>

/* This configuration file is included only once from led_state module and holds
 * information about LED effect associated with each state.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} led_state_def_include_once;


/* Map function to LED ID */
static const uint8_t led_map[LED_ID_COUNT] = {
	[LED_ID_SYSTEM_STATE] = 0,
	[LED_ID_PEER_STATE] = 1
};

/* For nRF54L15 SoC, only GPIO1 port can be used for PWM hardware peripheral output.
 * Because of that, when using an nRF54L15 PDK, the following limitations must be considered:
 *
 * - For v0.2.x onboard LED1 cannot be used for PWM output.
 * - For v0.3.0 onboard LED0 and LED2 cannot be used for PWM output.
 *
 * You can still use these LEDs with PWM LED driver, but LED color must be set either to
 * LED_COLOR(255, 255, 255) or LED_COLOR(0, 0, 0) to avoid using PWM peripheral for the LEDs.
 */

static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_CHARGING] = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_ERROR]    = LED_EFFECT_LED_BLINK(200, LED_COLOR(255, 255, 255)),
};

static const struct led_effect led_peer_state_effect[LED_PEER_COUNT][LED_PEER_STATE_COUNT] = {
	{[LED_PEER_STATE_DISCONNECTED]   = LED_EFFECT_LED_OFF(),
	 [LED_PEER_STATE_CONNECTED]      = LED_EFFECT_LED_ON(LED_COLOR(100, 100, 100)),
	 [LED_PEER_STATE_PEER_SEARCH]    = LED_EFFECT_LED_BREATH(1000, LED_COLOR(100, 100, 100)),
	 [LED_PEER_STATE_CONFIRM_SELECT] = LED_EFFECT_LED_BLINK(50, LED_COLOR(100, 100, 100)),
	 [LED_PEER_STATE_CONFIRM_ERASE]  = LED_EFFECT_LED_BLINK(25, LED_COLOR(100, 100, 100)),
	 [LED_PEER_STATE_ERASE_ADV]      = LED_EFFECT_LED_BREATH(100, LED_COLOR(100, 100, 100)),
	},
};

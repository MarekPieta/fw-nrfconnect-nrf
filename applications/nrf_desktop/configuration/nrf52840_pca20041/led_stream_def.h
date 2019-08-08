/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/* This configuration file is included only once from led_state module and holds
 * information about LED effect for led stream.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} led_stream_def_include_once;

#define LED_ID_STREAM 0
#define LED_STREEM_LOOP_SIZE 100

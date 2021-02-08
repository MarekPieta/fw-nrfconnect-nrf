/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _SENSOR_SIM_H_
#define _SENSOR_SIM_H_

/**
 * @file sensor_sim.h
 *
 * @brief Public API for the sensor_sim driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/types.h>


/** @brief Available generated wave types.
 */
enum wave_type {
	WAVE_TYPE_SINE,
	WAVE_TYPE_TRIANGLE,
	WAVE_TYPE_SQUARE,
	WAVE_TYPE_NONE,

	WAVE_TYPE_COUNT,
};

/** @brief Set simulated acceleration parameters.
 *
 * @note	This function can be used only if acceleration is generated as wave signal.
 * @warning	This function is thread-safe, but cannot be used in interrupts.
 *
 * @param[in] type		Selected wave type.
 * @param[in] amplitude		Wave amplitude.
 * @param[in] period_ms		Wave period.
 * @param[in] noise		Random noise amplitude.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int sensor_sim_set_accel_params(enum wave_type type, double amplitude, uint32_t period_ms,
				double noise);

#ifdef __cplusplus
}
#endif

#endif /* _SENSOR_SIM_H_ */

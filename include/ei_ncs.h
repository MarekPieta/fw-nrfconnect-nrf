/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 * @brief Edge Impulse NCS header.
 */

#ifndef _EI_NCS_H_
#define _EI_NCS_H_


/**
 * @defgroup ei_runner Edge Impulse NCS
 * @brief Edge Impulse NCS
 *
 * @{
 */

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ei_ncs_result_ready_cb)(int err);

size_t ei_ncs_get_frame_size(void);

size_t ei_ncs_sample_surplus(void);
int ei_ncs_add_sample_data(const float *data, size_t record_cnt);
int ei_ncs_move_input_window(size_t frame_cnt);
int ei_ncs_start_prediction(ei_ncs_result_ready_cb cb);

int ei_ncs_classification_label_get(const char **label);
int ei_ncs_anomaly_get(float *anomaly);

int ei_ncs_get_dsp_timing(int *dsp_time);
int ei_ncs_get_classification_timing(int *classification_time);
int ei_ncs_get_anomaly_timing(int *anomaly_time);

void ei_ncs_init(void);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _EI_NCS_H_ */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __SIGNAL_GEN_H__
#define __SIGNAL_GEN_H__

#include <zephyr/types.h>

struct gen_signal {
	size_t channel_cnt;
	struct gen_signal_channel *channels;
};

struct gen_signal_channel {
	size_t val_cnt;
	const float *vals;
};

int signal_gen_init(const struct gen_signal *signals_def, size_t signals_def_cnt);
int signal_gen_start(size_t signal_idx, int64_t signal_duration);
void signal_gen_get_data(float *out_val, size_t data_cnt);

#endif /* __SIGNAL_GEN_H__ */

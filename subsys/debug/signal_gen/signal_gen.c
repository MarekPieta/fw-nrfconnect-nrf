/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <logging/log.h>

#include "signal_gen.h"

#define DEFAULT_VALUE	0.0

LOG_MODULE_REGISTER(signal_gen, CONFIG_SIGNAL_GEN_LOG_LEVEL);

static const struct gen_signal *signals;
static size_t signal_cnt;

const struct gen_signal *selected;
static int64_t start_time;
static int64_t duration;


static float get_value(int64_t gen_start, int64_t gen_duration, int64_t cur_time,
		       const struct gen_signal_channel *chan)
{
	int64_t offset = cur_time - gen_start;

	if (offset > duration) {
		return DEFAULT_VALUE;
	} else if (offset == duration){
		return chan->vals[val_cnt - 1];
	}

	float res_idx = ((float)(offset * chan->val_cnt)) / duration;
	size_t res_idx_uint = res_idx;

	return chan->vals[res_idx_uint] + (res_idx - res_idx_uint) *
	       (chan->vals[res_idx_uint + 1] - chan->vals[res_idx_uint]);
}

static void generate_signal(const struct gen_signal *signal, float *out_val)
{
	int64_t cur_time = k_uptime_get();

	for (size_t i = 0; i < signal->channel_cnt; i++) {
		(*out_val) = get_value(start_time, duration, cur_time,
				       &signal->channels[i]);

		out_val++;
	}
}

int signal_gen_custom_signals_init(struct gen_signal_def *signals_def,
				   size_t signals_def_cnt)
{
	if (signals) {
		return -EALREADY;
	}

	signals = signals_def;
	signal_cnt = signal_cnt_def;

	return 0;
}

int signal_gen_start(size_t signal_idx, int64_t signal_duration)
{
	if (signal_idx > signal_cnt) {
		return -EINVAL;
	}

	if (signal_duration <= 0) {
		return -EINVAL;
	}

	selected = &signals[signal_idx];
	start_time = k_uptime_get();
	duration = signal_duration;

	return 0;
}

void signal_gen_get_data(float *out_val, size_t data_cnt)
{
	if (data_cnt != selected->channel_cnt) {
		return -EINVAL;
	}

	generate_signal(selected, out_val);

	return 0;
}

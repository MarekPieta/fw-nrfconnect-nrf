/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <assert.h>

#include <numpy.hpp>
#include <ei_run_classifier.h>

#include <ei_ncs.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(edge_impulse_runner, 0);

#define DATA_BUFFER_SIZE	5000
#define THREAD_STACK_SIZE	12000
#define THREAD_PRIORITY 	K_PRIO_PREEMPT(K_LOWEST_APPLICATION_THREAD_PRIO)
#define DEBUG_MODE		false

enum state {
	STATE_DISABLED,
	STATE_RUNNING,
	STATE_READY,
};

struct data_buffer {
	float buf[DATA_BUFFER_SIZE];
	float *processing_head;
	float *append_head;
};

static atomic_t state;

static K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread;

static K_SEM_DEFINE(ei_sem, 0, 1);

static struct data_buffer ei_input;
static ei_impulse_result_t ei_result;
static ei_ncs_result_ready_cb user_cb;

BUILD_ASSERT(DATA_BUFFER_SIZE >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);


static size_t buf_calc_max_processing_move(struct data_buffer *b)
{
	size_t res;
	float *buf_start = (float*)b->buf;
	float *buf_end = buf_start + ARRAY_SIZE(b->buf);
	float *processing_tail = b->processing_head + EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

	if (processing_tail >= buf_end) {
		processing_tail -= ARRAY_SIZE(b->buf);
	}

	if (b->append_head >= processing_tail) {
		res = b->append_head - processing_tail;
	} else {
		res = (buf_end - processing_tail) + (b->append_head - buf_start);
	}

	return res;
}

static size_t buf_calc_free_space(struct data_buffer *b)
{
	size_t res;
	float *buf_start = (float*)b->buf;
	float *buf_end = buf_start + ARRAY_SIZE(b->buf);

	if (b->processing_head > b->append_head) {
		res = b->processing_head - b->append_head - 1;
	} else {
		res = (buf_end - b->append_head) + (b->processing_head - buf_start) - 1;
	}

	return res;
}

static int buf_append(struct data_buffer *b, const float *data, size_t len)
{
	if (buf_calc_free_space(b) < len) {
		return -ENOMEM;
	}

	float *buf_start = (float*)b->buf;
	float *buf_end = buf_start + ARRAY_SIZE(b->buf);
	float *new_head = b->append_head + len;

	if (new_head >= buf_end) {
		size_t buf_left = buf_end - b->append_head;

		memcpy(b->append_head, data, buf_left * sizeof(float));
		memcpy(buf_start, data + buf_left, (len - buf_left) * sizeof(float));

		new_head -= ARRAY_SIZE(b->buf);
	} else {
		memcpy(b->append_head, data, len * sizeof(float));
	}

	b->append_head = new_head;
	return 0;
}

static void buf_get(struct data_buffer *b, float *b_res, size_t offset, size_t len)
{
	__ASSERT_NO_MSG((offset + len) <= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);

	float *buf_start = b->buf;
	float *buf_end = buf_start + ARRAY_SIZE(b->buf);
	float *read_start = b->processing_head + offset;
	float *read_end = read_start + len;

	if (read_start >= buf_end) {
		read_start -= ARRAY_SIZE(b->buf);
	}

	if (read_end > buf_end) {
		size_t buf_left = buf_end - read_start;

		memcpy(b_res, read_start, buf_left * sizeof(float));
		memcpy(b_res + buf_left, buf_start, (len - buf_left) * sizeof(float));
	} else {
		memcpy(b_res, read_start, len * sizeof(float));
	}
}

static int buf_processing_move(struct data_buffer *b, size_t move)
{
	float *buf_start = b->buf;
	float *buf_end = buf_start + ARRAY_SIZE(b->buf);

	if (move > buf_calc_max_processing_move(b)) {
		return -EINVAL;
	}

	b->processing_head += move;
	if (b->processing_head >= buf_end) {
		b->processing_head -= ARRAY_SIZE(b->buf);
	}

	return 0;
}

size_t ei_ncs_get_frame_size(void)
{
	return EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
}

size_t ei_ncs_frame_surplus(void)
{
	__ASSERT_NO_MSG(atomic_get(&state) != STATE_DISABLED);

	return buf_calc_max_processing_move(&ei_input) / EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
}

int ei_ncs_add_sample_data(const float *data, size_t record_cnt)
{
	__ASSERT_NO_MSG(atomic_get(&state) != STATE_DISABLED);

	if (record_cnt % EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
		return -EINVAL;
	}

	return buf_append(&ei_input, data, record_cnt);
}

int ei_ncs_move_input_window(size_t frame_cnt)
{
	__ASSERT_NO_MSG(atomic_get(&state) != STATE_DISABLED);

	/* Window cannot be moved when processing is ongoing. */
	if (atomic_get(&state) == STATE_RUNNING) {
		return -EBUSY;
	}

	size_t move = frame_cnt * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;

	return buf_processing_move(&ei_input, move);
}

int ei_ncs_start_prediction(ei_ncs_result_ready_cb cb)
{
	__ASSERT_NO_MSG(atomic_get(&state) != STATE_DISABLED);

	if (!atomic_cas(&state, STATE_READY, STATE_RUNNING)) {
		return -EBUSY;
	}

	user_cb = cb;
	k_sem_give(&ei_sem);

	return 0;
}

static int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
	__ASSERT_NO_MSG(atomic_get(&state) == STATE_RUNNING);

	buf_get(&ei_input, out_ptr, offset, length);

	return 0;
}

static void processing_finished(int err)
{
	__ASSERT_NO_MSG(user_cb);
	__ASSERT_NO_MSG(atomic_get(&state) == STATE_RUNNING);

	ei_ncs_result_ready_cb old_cb = user_cb;
	user_cb = NULL;
	atomic_set(&state, STATE_READY);

	old_cb(err);
}

static void edge_impulse_thread_fn(void)
{
	while (true) {
		k_sem_take(&ei_sem, K_FOREVER);

		/* Periodically perform prediction and print results. */
		signal_t features_signal;
		features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
		features_signal.get_data = &raw_feature_get_data;

		/* Invoke the impulse. */
		EI_IMPULSE_ERROR err = run_classifier(&features_signal, &ei_result, DEBUG_MODE);

		if (err) {
			LOG_ERR("run_classifier err=%d", err);
			processing_finished(err);
			continue;
		}

		processing_finished(err);
	}
}

int ei_ncs_classification_label_get(const char **label)
{
	float max_val = ei_result.classification[0].value;
	size_t max_ix = 0;

	for (size_t ix = 1; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
		if (max_val < ei_result.classification[ix].value) {
			max_ix = ix;
		}
	}
	*label = ei_result.classification[max_ix].label;

	return 0;
}

int ei_ncs_anomaly_get(float *anomaly)
{
	if (!EI_CLASSIFIER_HAS_ANOMALY) {
		return -ENOTSUP;
	}

	if (atomic_get(&state) != STATE_READY) {
		return -EBUSY;
	}

	*anomaly = ei_result.anomaly;

	return 0;
}

int ei_ncs_get_dsp_timing(int *dsp_time)
{
	if (atomic_get(&state) != STATE_READY) {
		return -EBUSY;
	}

	*dsp_time = ei_result.timing.dsp;

	return 0;
}

int ei_ncs_get_classification_timing(int *classification_time)
{
	if (atomic_get(&state) != STATE_READY) {
		return -EBUSY;
	}

	*classification_time = ei_result.timing.classification;

	return 0;
}

int ei_ncs_get_anomaly_timing(int *anomaly_time)
{
	if (!EI_CLASSIFIER_HAS_ANOMALY) {
		return -ENOTSUP;
	}

	if (atomic_get(&state) != STATE_READY) {
		return -EBUSY;
	}

	*anomaly_time = ei_result.timing.anomaly;

	return 0;
}

void ei_ncs_init(void)
{
	__ASSERT_NO_MSG(atomic_get(&state) == STATE_DISABLED);

	ei_input.processing_head = ei_input.buf;
	ei_input.append_head = ei_input.buf;

	k_thread_create(&thread, thread_stack,
			THREAD_STACK_SIZE,
			(k_thread_entry_t)edge_impulse_thread_fn,
			NULL, NULL, NULL,
			THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&thread, "edge_impulse_thread");
	atomic_set(&state, STATE_READY);
}

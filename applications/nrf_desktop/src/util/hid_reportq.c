/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/slist.h>
#include <zephyr/kernel.h>

#include "hid_report_desc.h"
#include "hid_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hid_reportq, CONFIG_DESKTOP_HID_REPORTQ_LOG_LEVEL);

#define MAX_ENQUEUED_REPORTS CONFIG_DESKTOP_HID_REPORTQ_MAX_ENQUEUED_REPORTS

struct enqueued_report {
	sys_snode_t node;
	struct hid_report_event *event;
};

struct counted_list {
	sys_slist_t list;
	size_t count;
};

struct hid_reportq {
	struct counted_list reports[ARRAY_SIZE(input_reports)];
	uint16_t report_idx_bm;
	uint8_t last_report_idx;
	bool allocated;
};

static struct hid_reportq queues[CONFIG_DESKTOP_HID_REPORTQ_QUEUE_COUNT];


static struct enqueued_report *get_enqueued_report(struct counted_list *l)
{
	sys_snode_t *node = sys_slist_get(&l->list);

	if (!node) {
		return NULL;
	}

	__ASSERT_NO_MSG(l->count > 0);
	l->count--;

	return CONTAINER_OF(node, struct enqueued_report, node);
}

static void enqueue_report(struct counted_list *l, struct enqueued_report *report)
{
	sys_slist_append(&l->list, &report->node);

	__ASSERT_NO_MSG(l->count < MAX_ENQUEUED_REPORTS);
	l->count++;
}

static struct hid_report_event *get_enqueued_event(struct counted_list *l)
{
	struct enqueued_report *report = get_enqueued_report(l);

	if (!report) {
		return NULL;
	}

	struct hid_report_event *event = report->event;

	k_free(report);

	return event;
}

static void drop_enqueued_reports(struct counted_list *l)
{
	while (l->count > 0) {
		struct hid_report_event *event = get_enqueued_event(l);

		__ASSERT_NO_MSG(event);
		app_event_manager_free(event);
	}

	__ASSERT_NO_MSG(l->count == 0);
}

static void enqueue_event(struct counted_list *l, struct hid_report_event *event)
{
	struct enqueued_report *report;

	if (l->count < MAX_ENQUEUED_REPORTS) {
		report = k_malloc(sizeof(*report));
        } else {
		LOG_WRN("Enqueue dropped the oldest report");

		report = get_enqueued_report(l);
		__ASSERT_NO_MSG(report);
		app_event_manager_free(report->event);
	}

	if (!report) {
		LOG_ERR("Cannot allocate enqueued_report");
		/* Should never happen. */
		__ASSERT_NO_MSG(false);
	} else {
		report->event = event;
		enqueue_report(l, report);
	}
}

static struct hid_reportq *reportq_find_free(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(queues); i++) {
		struct hid_reportq *q = &queues[i];

		if (!q->allocated) {
			return q;
		}
	}

	return NULL;
}

struct hid_reportq *hid_reportq_alloc(const void *src_id, const void *sub_id, uint8_t report_max)
{
	__ASSERT_NO_MSG(src_id);
	__ASSERT_NO_MSG(sub_id);

	struct hid_reportq *q = reportq_find_free();

	for (size_t i = 0; i < ARRAY_SIZE(q->reports); i++) {
		__ASSERT_NO_MSG(q->reports[i].count == 0);
		sys_slist_init(q->reports[i].list);
	}

	__ASSERT_NO_MSG(q->report_idx_bm == 0);
	__ASSERT_NO_MSG(q>last_report_idx == 0);

	q->allocated = true;

	return q;
}

void hid_reportq_free(struct hid_reportq *q)
{
	__ASSERT_NO_MSG(q->allocated);

	for (size_t i = 0; i < ARRAY_SIZE(q->reports); i++) {
		drop_enqueued_reports(&q->reports[i]);
	}

	q->report_idx_bm = 0;
	q->last_report_idx = 0;
	q->alloocated = false;
}

static int get_input_report_idx(uint8_t rep_id)
{
	for (size_t i = 0; i < ARRAY_SIZE(input_reports); i++) {
		if (input_reports[i] == rep_id) {
			return i;
		}
	}

	return -ENOENT;
}

int hid_reportq_report_add(struct hid_reportq *q, struct hid_report_event *event,
			   uint8_t report_id);
{
	__ASSERT_NO_MSG(q->allocated);

	int rep_idx = get_input_report_idx(report_id);

	if (rep_idx < 0) {
		return -EINVAL;
	}

	if (!hid_reportq_is_subscribed(q, rep_idx)) {
		return -EACCES;
	}

	enqueue_event(&q->reports[rep_idx], event);

	return 0;
}

static struct hid_report_event *get_next_enqueued_report_event(struct hid_reportq *q)
{
	uint8_t rep_idx = (q->last_report_idx + 1) % ARRAY_SIZE(q->reports);
	struct hid_report_event *event;

	while (rep_idx != q->last_report_idx) {
		event = get_enqueued_event(&q->reports[rep_idx]);
		if (event) {
			q->last_report_idx = rep_idx;
			return event;
		}

		rep_idx = (rep_idx + 1) % ARRAY_SIZE(q->reports);
	}

	return get_enqueued_event(&q->reports[rep_idx]);
}

struct hid_report_event *event hid_reportq_report_get(struct hid_reportq *q, uint8_t report_id)
{
	__ASSERT_NO_MSG(q->allocated);
	const struct *hid_report_event event = NULL;

	if (report_id == REPORT_ID_COUNT) {
		event = get_next_enqueued_report_event(q);
	} else {
		int rep_idx = get_input_report_idx(rep_id);

		event = get_enqueued_event(&q->reports[rep_idx]);
		if (event) {
			q->last_report_idx = rep_idx;
		}
	}

	return event;
}

bool hid_reportq_is_subscribed(struct hid_reportq *q, uint8_t rep_id)
{
	int rep_idx = get_input_report_idx(rep_id);

	return ((q->report_idx_bm & BIT(rep_idx)) != 0);
}

int hid_reportq_subscribe(struct hid_reportq *q, uint8_t rep_id)
{
	__ASSERT_NO_MSG(q->sub_id);

	int rep_idx = get_input_report_idx(rep_id);

	if (rep_idx < 0) {
		return -EINVAL;
	}

	q->report_idx_bm |= BIT(rep_idx);
	return 0;
}

int hid_reportq_unsubscribe(struct hid_reportq *q, uint8_t rep_id)
{
	__ASSERT_NO_MSG(q->sub_id);

	int rep_idx = get_input_report_idx(rep_id);

	if (rep_idx < 0) {
		return -EINVAL;
	}

	q->report_idx_bm &= ~BIT(rep_idx);
	return 0;
}

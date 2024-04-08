/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief HID report queue header.
 */

#ifndef _HID_REPORTQ_H_
#define _HID_REPORTQ_H_

/**
 * @defgroup hid_reportq HID report queue
 * @brief Utility module that simplifies queuing HID input reports.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque type representing HID report queue object. */
struct hid_reportq;

/**
 * @brief Allocate a HID report queue object instance.
 *
 * The function allocates HID report queue object instance from internal poll. Assertion failure
 * is triggered in case of too small object poll.
 *
 * @return Pointer to the allocated object instance.
 */
struct hid_reportq *hid_reportq_alloc(void);

/**
 * @brief Free HID report queue object instance.
 *
 * @param[in] q		Pointer to the queue to be freed.
 */
void hid_reportq_free(struct hid_reportq *q);

/**
 * @brief Add a HID report to the queue.
 *
 * @param[in] q		Pointer to the queue instance.
 * @param[in] event     Pointer to the HID report event to be enqueued.
 * @param[in] report_id Report ID.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int hid_reportq_report_add(struct hid_reportq *q, struct hid_report_event *event,
			   uint8_t report_id);

/**
 * @brief Get HID report event from the queue.
 *
 * @param[in] q		Pointer to the queue instance.
 * @param[in] rep_id	Chosen HID report ID. Use REPORT_ID_COUNT to get any HID report ID. In that
 *                      case, enqueued reports are returned in round-robin fashion.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
struct hid_report_event *event hid_reportq_report_get(struct hid_reportq *q, uint8_t report_id);

/**
 * @brief Check if HID report queue is subscribed for HID report with given ID.
 *
 * @param[in] q		Pointer to the queue instance.
 * @param[in] rep_id	HID report ID.
 *
 * @return true if subscribed, false otherwise.
 */
bool hid_reportq_is_subscribed(struct hid_reportq *q, uint8_t rep_id);

/**
 * @brief Subscribe for HID report with given ID.
 *
 * @param[in] q		Pointer to the queue instance.
 * @param[in] rep_id	HID report ID.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int hid_reportq_subscribe(struct hid_reportq *q, uint8_t rep_id);

/**
 * @brief Unsubscribe from HID report with given ID.
 *
 * @param[in] q		Pointer to the queue instance.
 * @param[in] rep_id	HID report ID.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int hid_reportq_unsubscribe(struct hid_reportq *q, uint8_t rep_id);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /*_HID_REPORTQ_H_ */

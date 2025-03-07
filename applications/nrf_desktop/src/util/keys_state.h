/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _KEYS_STATE_H_
#define _KEYS_STATE_H_

/**
 * @file
 * @defgroup keys_state Keys state
 * @{
 * @brief Utility used to track state of active keys.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define KEYS_MAX_CNT	CONFIG_DESKTOP_KEYS_STATE_KEY_CNT_MAX

/** @brief Structure used to track an active key. */
struct active_key {
	uint16_t id; /**< Key ID. */
	uint16_t press_cnt; /**< Keypress counter. */
};

/**@brief Keys state structure. */
struct keys_state {
	struct active_key keys[KEYS_MAX_CNT]; /**< Active keys. */
	uint8_t cnt; /**< Current number of active keys. */
	uint8_t cnt_max; /**< Maximum numer of active keys. */
};

/**
 * @brief Initialize a keys state object.
 *
 * A keys state object must be initialized before use.
 *
 * @param[out] ks		A keys state object.
 * @param[in] key_cnt_max	Maximum number of active keys.
 */
void keys_state_init(struct hid_keys_state *ks, uint8_t key_cnt_max);

/**
 * @brief Notify keys state about a key press/release
 *
 * The function is called to notify the keys state that a key was pressed or released.
 *
 * @param[out] ks	A keys state object.
 * @param[in] key_id	Key ID.
 * @param[in] pressed	Information if key was pressed or released.
 *
 * @retval 0 on success.
 * @retval -ENOBUFS if number of currently active keys exceeds the maximum number of active keys.
 */
int keys_state_key_update(struct hid_keys_state *ks, uint16_t key_id, bool pressed);

/**
 * @brief Clear keys state
 *
 * The function removes all of the tracked active key presses.
 *
 * @param[out] ks		A keys state object.
 */
void keys_state_clear(struct hid_keys_state *ks);

/**
 * @brief Get keys state
 *
 * The function fills the provided array with key IDs of all the active keys.
 * The utility keeps key IDs sorted ascending to ensure consistent behavior.
 *
 * @param[in] ks	A keys state object.
 * @param[out] keys	Array to be filled with key IDs of active keys.
 * @param[in] keys_size	Size of the "keys" array.
 *
 * @retval Number of keys written on success.
 * @retval -EINVAL The provided keys array is too small.
 */
int keys_state_keys_get(const struct hid_keys_state *ks, uint16_t *keys, uint8_t keys_size);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /*_KEYS_STATE_H_ */

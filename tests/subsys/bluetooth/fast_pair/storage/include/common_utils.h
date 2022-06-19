/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COMMON_UTILS_H_
#define _COMMON_UTILS_H_

/**
 * @defgroup fp_storage_test_common_utils Fast Pair storage unit test common utilities
 * @brief Common utilities of the Fast Pair storage unit test
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Generate mocked Account Key data.
 *
 * @param[in] key_id 		ID used to generate the Account Key.
 * @param[out] account_key 	128-bit (16-byte) buffer used to store generated Account Key.
 */
void generate_account_key(uint8_t key_id, uint8_t *account_key);

/** Check if Account Key data was generated from provided ID.
 *
 * @param[in] key_id		ID to be verified.
 * @param[in] account_key	128-bit (16-byte) Account Key data to be verified.
 *
 * @retval true If the Account Key data was generated from provided ID.
 * @retval false If the Account Key data was not generated from provided ID.
 */
bool check_account_key_id(uint8_t key_id, const uint8_t *account_key);

/** Validate that Fast Pair storage data is not loaded.
 *
 * The function uses Fast Pair storage API to validate that the data is not loaded from settings.
 * Ztest asserts are used to signalize error.
 */
void validate_fp_storage_unloaded(void);

/** Validate the loaded Fast Pair storage data.
 *
 * The function uses Fast Pair storage API to validate that the data is valid.
 * The function assumes that @ref generate_account_key was used to generate subsequent Account Keys.
 * Ztest asserts are used to signalize error.
 *
 * @param[in] first_id		ID assigned to the first generated Account Key.
 * @param[in] stored_cnt	Number of stored keys.
 */
void validate_fp_storage_loaded(uint8_t first_id, uint8_t stored_cnt);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _COMMON_UTILS_H_ */

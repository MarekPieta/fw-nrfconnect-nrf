/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ztest.h>

#include "fp_storage.h"
#include "fp_crypto.h"

#define ACCOUNT_KEY_MAX_CNT	CONFIG_BT_FAST_PAIR_STORAGE_ACCOUNT_KEY_MAX
#define ACCOUNT_KEY_LEN		FP_CRYPTO_ACCOUNT_KEY_LEN


void generate_account_key(uint8_t key_id, uint8_t *account_key)
{
        memset(account_key, key_id, ACCOUNT_KEY_LEN);
}

bool check_account_key_id(uint8_t key_id, const uint8_t *account_key)
{
	if (account_key[0] != key_id) {
		return false;
	}

	for (size_t i = 1; i < ACCOUNT_KEY_LEN; i++) {
		zassert_equal(account_key[i], key_id, "Invalid Account Key");
	}

	return true;
}

static bool account_key_find_cb(const uint8_t *account_key, void *context)
{
	uint8_t *key_id = context;

	return check_account_key_id(*key_id, account_key);
}

void validate_fp_storage_unloaded(void)
{
	int err;
	uint8_t all_keys[ACCOUNT_KEY_MAX_CNT][ACCOUNT_KEY_LEN];
	size_t key_count = ACCOUNT_KEY_MAX_CNT;
	uint8_t key_id = 0;

	err = fp_storage_account_key_count();
	zassert_equal(err, -ENODATA, "Expected error before settings load");

	err = fp_storage_account_keys_get(all_keys, &key_count);
	zassert_equal(err, -ENODATA, "Expected error before settings load");

	err = fp_storage_account_key_find(all_keys[0], account_key_find_cb, &key_id);
	zassert_equal(err, -ENODATA, "Expected error before settings load");
}

void validate_fp_storage_loaded(uint8_t first_id, uint8_t stored_cnt)
{
	int res;

	int key_cnt;
	uint8_t read_keys[ACCOUNT_KEY_MAX_CNT][ACCOUNT_KEY_LEN];
	size_t read_cnt = ACCOUNT_KEY_MAX_CNT;

	key_cnt = fp_storage_account_key_count();

	res = fp_storage_account_keys_get(read_keys, &read_cnt);
	zassert_ok(res, "Getting Account Keys failed");
	zassert_equal(key_cnt, read_cnt, "Invalid key count");
	zassert_equal(key_cnt, MIN(stored_cnt, ACCOUNT_KEY_MAX_CNT), "Invalid key count");

	size_t key_id_min = (stored_cnt < ACCOUNT_KEY_MAX_CNT) ?
			    (0) : (stored_cnt - ACCOUNT_KEY_MAX_CNT);
	size_t key_id_max = stored_cnt - 1;

	key_id_min += first_id;
	key_id_max += first_id;

	for (size_t i = key_id_min; i <= key_id_max; i++) {
		bool found = false;

		for (size_t j = 0; j < read_cnt; j++) {
			if (found) {
				zassert_false(check_account_key_id(i, read_keys[j]),
					      "Key duplicate found");
			}

			if (check_account_key_id(i, read_keys[j])) {
				found = true;
			}
		}

		zassert_true(found, "Key dropped by the Fast Pair storage");
	}
}

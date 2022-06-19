/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ztest.h>
#include <zephyr/settings/settings.h>

#include "fp_storage.h"
#include "fp_crypto.h"

#include "storage_mock.h"
#include "common_utils.h"

#define ACCOUNT_KEY_MAX_CNT	CONFIG_BT_FAST_PAIR_STORAGE_ACCOUNT_KEY_MAX
#define ACCOUNT_KEY_LEN		FP_CRYPTO_ACCOUNT_KEY_LEN


static bool account_key_find_cb(const uint8_t *account_key, void *context)
{
	uint8_t *key_id = context;

	return check_account_key_id(*key_id, account_key);
}

static void reload_keys_from_storage(void)
{
	int err;

	fp_storage_ram_clear();
	validate_fp_storage_unloaded();
	err = settings_load();

	zassert_ok(err, "Failed to load settings");
}

static void setup_fn(void)
{
	int err;

	validate_fp_storage_unloaded();

	/* Load empty settings before the test to initialize fp_storage. */
	err = settings_load();

	zassert_ok(err, "Settings load failed");
}

static void teardown_fn(void)
{
	fp_storage_ram_clear();
	storage_mock_clear();
	validate_fp_storage_unloaded();
}

static void test_unloaded(void)
{
	static const uint8_t test_key_id = 0;

	int err;
	uint8_t account_key[ACCOUNT_KEY_LEN];

	validate_fp_storage_unloaded();
	generate_account_key(test_key_id, account_key);

	err = fp_storage_account_key_save(account_key);
	zassert_equal(err, -ENODATA, "Expected error before settings load");
}

static void test_one_key(void)
{
	static const uint8_t test_key_id = 5;

	int err;
	uint8_t account_key[ACCOUNT_KEY_LEN];

	generate_account_key(test_key_id, account_key);
	err = fp_storage_account_key_save(account_key);
	zassert_ok(err, "Unexpected error during Account Key save");

	uint8_t read_keys[ACCOUNT_KEY_MAX_CNT][ACCOUNT_KEY_LEN];
	size_t read_cnt = ACCOUNT_KEY_MAX_CNT;

	zassert_equal(fp_storage_account_key_count(), 1, "Invalid Account Key count");
	err = fp_storage_account_keys_get(read_keys, &read_cnt);
	zassert_ok(err, "Getting Account Keys failed");
	zassert_equal(read_cnt, 1, "Invalid Account Key count");
	zassert_true(check_account_key_id(test_key_id, read_keys[0]), "Invalid key on read");

	/* Reload keys from storage and validate them again. */
	reload_keys_from_storage();
	memset(read_keys, 0, sizeof(read_keys));
	read_cnt = ACCOUNT_KEY_MAX_CNT;

	zassert_equal(fp_storage_account_key_count(), 1, "Invalid Account Key count");
	err = fp_storage_account_keys_get(read_keys, &read_cnt);
	zassert_ok(err, "Getting Account Keys failed");
	zassert_equal(read_cnt, 1, "Invalid Account Key count");
	zassert_true(check_account_key_id(test_key_id, read_keys[0]), "Invalid key on read");
}

static void test_duplicate(void)
{
	static const uint8_t test_key_id = 3;

	int err;
	uint8_t account_key[ACCOUNT_KEY_LEN];

	generate_account_key(test_key_id, account_key);
	err = fp_storage_account_key_save(account_key);
	zassert_ok(err, "Unexpected error during Account Key save");

	/* Try to add key duplicate. */
	err = fp_storage_account_key_save(account_key);
	zassert_ok(err, "Unexpected error during Account Key save");

	uint8_t read_keys[ACCOUNT_KEY_MAX_CNT][ACCOUNT_KEY_LEN];
	size_t read_cnt = ACCOUNT_KEY_MAX_CNT;

	zassert_equal(fp_storage_account_key_count(), 1, "Invalid Account Key count");
	err = fp_storage_account_keys_get(read_keys, &read_cnt);
	zassert_ok(err, "Getting Account Keys failed");
	zassert_equal(read_cnt, 1, "Invalid Account Key count");
	zassert_true(check_account_key_id(test_key_id, read_keys[0]), "Invalid key on read");

	/* Reload keys from storage and validate them again. */
	reload_keys_from_storage();
	memset(read_keys, 0, sizeof(read_keys));
	read_cnt = ACCOUNT_KEY_MAX_CNT;

	zassert_equal(fp_storage_account_key_count(), 1, "Invalid Account Key count");
	err = fp_storage_account_keys_get(read_keys, &read_cnt);
	zassert_ok(err, "Getting Account Keys failed");
	zassert_equal(read_cnt, 1, "Invalid Account Key count");
	zassert_true(check_account_key_id(test_key_id, read_keys[0]), "Invalid key on read");
}

static void generate_and_store_keys(uint8_t first_id, uint8_t gen_count)
{
	int err;
	uint8_t account_key[ACCOUNT_KEY_LEN];

	for (uint8_t i = first_id; i < (first_id + gen_count); i++) {
		generate_account_key(i, account_key);

		err = fp_storage_account_key_save(account_key);
		zassert_ok(err, "Failed to store Account Key");
	}
}

static void test_invalid_calls(void)
{
	static const uint8_t test_key_cnt = 3;

	int err;
	uint8_t found_key[ACCOUNT_KEY_LEN];
	uint8_t read_keys[ACCOUNT_KEY_MAX_CNT][ACCOUNT_KEY_LEN];

	generate_and_store_keys(0, test_key_cnt);

	for (size_t read_cnt = 0; read_cnt <= ACCOUNT_KEY_MAX_CNT; read_cnt++) {
		size_t read_cnt_ret = read_cnt;

		err = fp_storage_account_keys_get(read_keys, &read_cnt_ret);

		if (read_cnt < test_key_cnt) {
			zassert_equal(err, -EINVAL, "Expected error when array is too small");
		} else {
			zassert_ok(err, "Unexpected error during Account Keys get");
			zassert_equal(read_cnt_ret, test_key_cnt, "Inavlid account key count");

			for (size_t i = 0; i < read_cnt_ret; i++) {
				zassert_true(check_account_key_id(i, read_keys[i]),
					     "Invalid key on read");
			}
		}
	}

	uint8_t key_id = 0;

	err = fp_storage_account_key_find(found_key, NULL, &key_id);
	zassert_equal(err, -EINVAL, "Expected error when callback is NULL");
}

static void test_find(void)
{
	static const size_t test_key_cnt = 3;
	int err = 0;

	generate_and_store_keys(0, test_key_cnt);

	for (uint8_t key_id = 0; key_id < test_key_cnt; key_id++) {
		uint8_t account_key[ACCOUNT_KEY_LEN];
		uint8_t prev_key_id = key_id;

		err = fp_storage_account_key_find(account_key, account_key_find_cb, &key_id);
		zassert_ok(err, "Failed to find Account Key");
		zassert_equal(prev_key_id, key_id, "Context should not be modified");
		zassert_true(check_account_key_id(key_id, account_key), "Found wrong Account Key");
	}

	for (uint8_t key_id = 0; key_id < (test_key_cnt + 1); key_id++) {
		uint8_t prev_key_id = key_id;

		err = fp_storage_account_key_find(NULL, account_key_find_cb, &key_id);
		zassert_equal(prev_key_id, key_id, "Context should not be modified");
		if (err) {
			zassert_equal(key_id, test_key_cnt, "Failed to find Acount Key");
			break;
		}
	}

	zassert_equal(err, -ESRCH, "Expected error when key cannot be found");
}

static void test_loop(void)
{
	static const uint8_t first_key_id = 0;

	for (uint8_t i = 1; i < UCHAR_MAX; i++) {
		setup_fn();

		generate_and_store_keys(first_key_id, i);
		validate_fp_storage_loaded(first_key_id, i);

		/* Reload keys from storage and validate them again. */
		reload_keys_from_storage();
		validate_fp_storage_loaded(first_key_id, i);

		teardown_fn();
	}
}

void api_test_run(void)
{
	ztest_test_suite(fast_pair_storage_api_tests,
			 ztest_unit_test(test_unloaded),
			 ztest_unit_test_setup_teardown(test_one_key, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_duplicate, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_invalid_calls, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_find, setup_fn, teardown_fn),
			 ztest_unit_test(test_loop)
			 );

	ztest_run_test_suite(fast_pair_storage_api_tests);
}

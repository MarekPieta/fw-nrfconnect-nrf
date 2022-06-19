/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ztest.h>
#include <zephyr/settings/settings.h>

#include "fp_storage.h"
#include "fp_storage_priv.h"

#include "storage_mock.h"
#include "common_utils.h"


static void setup_fn(void)
{
	/* Make sure that settings are not yet loaded. Settings will be populated with data and
	 * loaded by the unit test.
	 */
	validate_fp_storage_unloaded();
}

static void teardown_fn(void)
{
	int err = settings_load();

	zassert_not_equal(err, 0, "Expected error in settings load");
	validate_fp_storage_unloaded();

	fp_storage_ram_clear();
	storage_mock_clear();
}

/* Self test is done to ensure that the test method is valid. */
static void self_test_teardown_fn(void)
{
	fp_storage_ram_clear();
	storage_mock_clear();
	validate_fp_storage_unloaded();
}

static void test_wrong_settings_key(void)
{
	int err;
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;
	struct account_key_data data;

	data.account_key_id = key_id;
	generate_account_key(key_id, data.account_key);

	err = settings_save_one(SETTINGS_AK_SUBTREE_NAME SETTINGS_NAME_CONNECTOR "not_account_key",
				&data, sizeof(data));
	zassert_ok(err, "Unexpected error in settings save");
}

static void test_wrong_data_len(void)
{
	int err;
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;
	char settings_key[SETTINGS_AK_NAME_MAX_SIZE] = SETTINGS_AK_FULL_PREFIX;
	struct account_key_data data;

	zassert_true(('0' + account_key_id_to_idx(key_id)) <= '9', "Key index out of range");
	settings_key[ARRAY_SIZE(settings_key) - 2] = ('0' + account_key_id_to_idx(key_id));
	settings_key[ARRAY_SIZE(settings_key) - 1] = '\0';

	data.account_key_id = key_id;
	generate_account_key(key_id, data.account_key);

	err = settings_save_one(settings_key, &data, sizeof(data) - 1);
	zassert_ok(err, "Unexpected error in settings save");
}

static void test_inconsistent_key_id(void)
{
	int err;
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;
	char settings_key[SETTINGS_AK_NAME_MAX_SIZE] = SETTINGS_AK_FULL_PREFIX;
	struct account_key_data data;

	zassert_true(('0' + account_key_id_to_idx(key_id)) <= '9', "Key index out of range");
	settings_key[ARRAY_SIZE(settings_key) - 2] = ('0' + account_key_id_to_idx(key_id));
	settings_key[ARRAY_SIZE(settings_key) - 1] = '\0';

	data.account_key_id = next_account_key_id(key_id);
	generate_account_key(next_account_key_id(key_id), data.account_key);

	err = settings_save_one(settings_key, &data, sizeof(data));
	zassert_ok(err, "Unexpected error in settings save");
}

static void store_key(uint8_t key_id)
{
	int err;
	char settings_key[SETTINGS_AK_NAME_MAX_SIZE] = SETTINGS_AK_FULL_PREFIX;
	struct account_key_data data;

	zassert_true(('0' + account_key_id_to_idx(key_id)) <= '9', "Key index out of range");
	settings_key[ARRAY_SIZE(settings_key) - 2] = ('0' + account_key_id_to_idx(key_id));
	settings_key[ARRAY_SIZE(settings_key) - 1] = '\0';

	data.account_key_id = key_id;
	generate_account_key(key_id, data.account_key);

	err = settings_save_one(settings_key, &data, sizeof(data));
	zassert_ok(err, "Unexpected error in settings save");
}

static void self_test(void)
{
	int err;
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;
	uint8_t key_cnt = 0;

	store_key(key_id);
	key_cnt++;
	key_id = next_account_key_id(key_id);

	store_key(key_id);
	key_cnt++;
	key_id = next_account_key_id(key_id);

	store_key(key_id);
	key_cnt++;

	err = settings_load();
	zassert_ok(err, "Unexpected error in settings load");

	validate_fp_storage_loaded(ACCOUNT_KEY_MIN_ID, key_cnt);
}

static void self_test_rollover(void)
{
	int err;
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;
	uint8_t key_cnt = 0;

	for (size_t i = 0; i < ACCOUNT_KEY_CNT; i++) {
		store_key(key_id);
		key_cnt++;
		key_id = next_account_key_id(key_id);
	}

	store_key(key_id);
	key_cnt++;
	key_id = next_account_key_id(key_id);

	store_key(key_id);
	key_cnt++;

	err = settings_load();
	zassert_ok(err, "Unexpected error in settings load");

	validate_fp_storage_loaded(ACCOUNT_KEY_MIN_ID, key_cnt);
}

static void test_drop_first_key(void)
{
	store_key(next_account_key_id(ACCOUNT_KEY_MIN_ID));
}

static void test_drop_key(void)
{
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;

	store_key(key_id);

	key_id = next_account_key_id(key_id);
	store_key(key_id);

	key_id = next_account_key_id(key_id);
	key_id = next_account_key_id(key_id);
	store_key(key_id);
}

static void test_drop_keys(void)
{
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;

	store_key(key_id);

	for (size_t i = 0; i < ACCOUNT_KEY_CNT; i++) {
		key_id = next_account_key_id(key_id);
	}

	store_key(key_id);
}

static void test_drop_rollover(void)
{
	uint8_t key_id = ACCOUNT_KEY_MIN_ID;

	for (size_t i = 0; i < ACCOUNT_KEY_CNT; i++) {
		store_key(key_id);
		key_id = next_account_key_id(key_id);
	}

	zassert_equal(account_key_id_to_idx(ACCOUNT_KEY_MIN_ID), account_key_id_to_idx(key_id),
		      "Test should drop key exactly after rollover");
	key_id = next_account_key_id(key_id);
	store_key(key_id);
}

void corrupted_data_test_run(void)
{
	ztest_test_suite(fast_pair_storage_courrupted_data_tests,
			 ztest_unit_test_setup_teardown(self_test, setup_fn, self_test_teardown_fn),
			 ztest_unit_test_setup_teardown(self_test_rollover, setup_fn,
							self_test_teardown_fn),
			 ztest_unit_test_setup_teardown(test_wrong_settings_key, setup_fn,
							teardown_fn),
			 ztest_unit_test_setup_teardown(test_wrong_data_len, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_inconsistent_key_id, setup_fn,
							teardown_fn),
			 ztest_unit_test_setup_teardown(test_drop_first_key, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_drop_key, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_drop_keys, setup_fn, teardown_fn),
			 ztest_unit_test_setup_teardown(test_drop_rollover, setup_fn, teardown_fn)
			 );

	ztest_run_test_suite(fast_pair_storage_courrupted_data_tests);
}

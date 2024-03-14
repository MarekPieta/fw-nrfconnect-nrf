/*
 * Copyright (c) 2022 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <psa/crypto.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util_macro.h>

#include "tfm_sp_log.h"

#include "psa/crypto.h"
#include "psa/service.h"
#include "psa_manifest/tfm_secure_peripheral_partition.h"

#include "fp_crypto.h"
#include "fp_common.h"

static void handle_ping(void)
{
	static psa_status_t res = PSA_SUCCESS;

	psa_status_t status;
	psa_msg_t msg;

	res++;

	status = psa_get(TFM_PING_SIGNAL, &msg);
	psa_reply(msg.handle, res);
}

int fp_crypto_sha256(uint8_t *out, const uint8_t *in, size_t data_len)
{
	size_t hash_len = 0;
	psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, in, data_len,
					       out, FP_CRYPTO_SHA256_HASH_LEN, &hash_len);

	if (status != PSA_SUCCESS) {
		return -1;
	}

	if (hash_len != FP_CRYPTO_SHA256_HASH_LEN) {
		return -1;
	}

	return 0;
}

size_t fp_crypto_account_key_filter_size(size_t n)
{
	if (n == 0) {
		return 0;
	} else {
		return 1.2 * n + 3;
	}
}

int fp_crypto_account_key_filter(uint8_t *out, const struct fp_account_key *account_key_list, 
                                 size_t n, uint16_t salt, const uint8_t *battery_info)
{
	size_t s = fp_crypto_account_key_filter_size(n);
	uint8_t v[FP_ACCOUNT_KEY_LEN + sizeof(salt) + FP_CRYPTO_BATTERY_INFO_LEN];
	uint8_t h[FP_CRYPTO_SHA256_HASH_LEN];
	uint32_t x;
	uint32_t m;
	int err;

	memset(out, 0, s);
	for (size_t i = 0; i < n; i++) {
		size_t pos = 0;

		memcpy(v, account_key_list[i].key, FP_ACCOUNT_KEY_LEN);
		pos += FP_ACCOUNT_KEY_LEN;

		sys_put_be16(salt, &v[pos]);
		pos += sizeof(salt);

		if (battery_info) {
			memcpy(&v[pos], battery_info, FP_CRYPTO_BATTERY_INFO_LEN);
			pos += FP_CRYPTO_BATTERY_INFO_LEN;
		}

		err = fp_crypto_sha256(h, v, pos);
		if (err) {
			return err;
		}

		for (size_t j = 0; j < FP_CRYPTO_SHA256_HASH_LEN / sizeof(x); j++) {
			x = sys_get_be32(&h[j * sizeof(x)]);
			m = x % (s * __CHAR_BIT__);
			WRITE_BIT(out[m / __CHAR_BIT__], m % __CHAR_BIT__, 1);
		}
	}

	return 0;
}

static void handle_bloom_filter(void)
{
	psa_status_t status;
	psa_msg_t msg;

	size_t ak_cnt = 0;
	size_t read_size = 0;
	uint16_t salt;
	uint8_t battery_info[FP_CRYPTO_BATTERY_INFO_LEN];
	struct fp_account_key account_key_list[5];

	status = psa_get(TFM_FAST_PAIR_BLOOM_FILTER_SIGNAL, &msg);

	read_size = psa_read(msg.handle, 0, &account_key_list, sizeof(account_key_list));
	ak_cnt = read_size / sizeof(struct fp_account_key);
	read_size = psa_read(msg.handle, 1, &salt, sizeof(salt));
	read_size = psa_read(msg.handle, 2, battery_info, sizeof(battery_info));

	size_t olen = fp_crypto_account_key_filter_size(ak_cnt);
	uint8_t out[olen];

	int err = fp_crypto_account_key_filter(out, account_key_list, ak_cnt, salt,
					       (read_size > 0) ? battery_info : 0);
	if (err) {
		psa_reply(msg.handle, -1);
	} else {
		psa_write(msg.handle, 0, out, olen);
		psa_reply(msg.handle, PSA_SUCCESS);
	}
}

psa_status_t tfm_spp_main(void)
{
	psa_signal_t signals = 0;

	while (1) {
		signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);

		if (signals & TFM_PING_SIGNAL) {
			handle_ping();
                } else if (signals & TFM_FAST_PAIR_BLOOM_FILTER_SIGNAL) {
			handle_bloom_filter();
		} else {
			psa_panic();
		}
	}

	return PSA_ERROR_SERVICE_FAILURE;
}

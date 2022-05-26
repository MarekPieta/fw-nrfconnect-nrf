/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <zephyr/device.h>
#include <psa/crypto.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fast_pair, CONFIG_BT_FAST_PAIR_LOG_LEVEL);

#include "fp_crypto.h"

#define AES128_ALG	PSA_ALG_ECB_NO_PADDING
/* Marker of the uncompressed binary format for a point on an elliptic curve. */
#define UNCOMPRESSED_FORMAT_MARKER (0x04)


int fp_crypto_sha256(uint8_t *out, const uint8_t *in, size_t data_len)
{
	size_t hash_len;
	psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, in, data_len, 
					       out, FP_CRYPTO_SHA256_HASH_LEN, &hash_len);

	if (status != PSA_SUCCESS) {
		LOG_ERR("sha256 failed (err: %d)", status);
                return -EIO;
        }

	if (hash_len != FP_CRYPTO_SHA256_HASH_LEN) {
		LOG_ERR("Invalid sha256 output len: %zu", hash_len);
                return -EIO;
        }

	return 0;
}

static psa_status_t import_aes128_key(const uint8_t *key_bytes, psa_key_id_t *key_id)
{
	static const size_t key_len = 16;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, AES128_ALG);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, key_len * CHAR_BIT);

	return psa_import_key(&key_attributes, key_bytes, key_len, key_id);
}

int fp_crypto_aes128_encrypt(uint8_t *out, const uint8_t *in, const uint8_t *k)
{
	psa_key_id_t key_id;
	uint32_t olen;
	psa_status_t status;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

	status = import_aes128_key(k, &key_id);
	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_import_key (err: %d)", status);
		return -EIO;
	}

	status = psa_cipher_encrypt_setup(&operation, key_id, AES128_ALG);
	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_encrypt_setup (err: %d)", status);
		return -EIO;
        }

	status = psa_cipher_update(&operation,
				   in, FP_CRYPTO_AES128_BLOCK_LEN,
				   out, FP_CRYPTO_AES128_BLOCK_LEN,
				   &olen);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_update (err: %d)", status);
                return -EIO;
        }

	status = psa_cipher_finish(&operation,
				   out + olen, FP_CRYPTO_AES128_BLOCK_LEN - olen,
				   &olen);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_finish (err: %d)", status);
                return -EIO;
        }

	status = psa_cipher_abort(&operation);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_abort (err: %d)", status);
                return -EIO;
        }

	return 0;
}

int fp_crypto_aes128_decrypt(uint8_t *out, const uint8_t *in, const uint8_t *k)
{
	psa_key_id_t key_id;
	uint32_t olen;
	psa_status_t status;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

	status = import_aes128_key(k, &key_id);
	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_import_key (err: %d)", status);
		return -EIO;
	}

	status = psa_cipher_decrypt_setup(&operation, key_id, AES128_ALG);
	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_encrypt_setup (err: %d)", status);
		return -EIO;
        }

	status = psa_cipher_update(&operation,
				   in, FP_CRYPTO_AES128_BLOCK_LEN,
				   out, FP_CRYPTO_AES128_BLOCK_LEN,
				   &olen);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_update (err: %d)", status);
                return -EIO;
        }

	status = psa_cipher_finish(&operation,
				   out + olen, FP_CRYPTO_AES128_BLOCK_LEN - olen,
				   &olen);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_finish (err: %d)", status);
                return -EIO;
        }

	status = psa_cipher_abort(&operation);

	if (status != PSA_SUCCESS) {
                LOG_ERR("psa_cipher_abort (err: %d)", status);
                return -EIO;
        }

	return 0;
}

int fp_crypto_ecdh_shared_secret(uint8_t *secret_key, const uint8_t *public_key,
				 const uint8_t *private_key)
{
	uint8_t public_key_uncompressed[65];

	uint32_t out_len;
	psa_status_t status;
	psa_key_handle_t priv_key;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);

	status = psa_import_key(&key_attributes, private_key, 32, &priv_key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_generate_key (err: %d)", status);
		return -EIO;
	}

	/* Use the uncompressed binary format [0x04 X Y] for the public key point. */
	public_key_uncompressed[0] = UNCOMPRESSED_FORMAT_MARKER;
	memcpy(&public_key_uncompressed[1],
	       public_key,
	       sizeof(public_key_uncompressed) - 1);

        status = psa_raw_key_agreement(PSA_ALG_ECDH, priv_key,
				       public_key_uncompressed, sizeof(public_key_uncompressed),
				       secret_key, FP_CRYPTO_ECDH_SHARED_KEY_LEN,
				       &out_len);

	if (status != PSA_SUCCESS) {
                LOG_INF("psa_raw_key_agreement (err: %d)", status);
                return -EIO;
        }

	return 0;
}

static int fp_crypto_nrf_security_init(const struct device *unused)
{
	ARG_UNUSED(unused);

        psa_status_t status = psa_crypto_init();

	__ASSERT(status == PSA_SUCCESS, "PSA initialization failed");

	return (status == PSA_SUCCESS) ? (0) : (-EIO);
}

SYS_INIT(fp_crypto_nrf_security_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

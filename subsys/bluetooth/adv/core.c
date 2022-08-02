/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/adv.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nrf_bt_adv, CONFIG_NRF_BT_ADV_LOG_LEVEL);

enum provider_set {
	PROVIDER_SET_AD,
	PROVIDER_SET_SD
};

static void get_section_ptrs(enum provider_set set, struct nrf_bt_adv_data_provider **start,
			     struct nrf_bt_adv_data_provider **end)
{
	switch (set) {
		case PROVIDER_SET_AD:;
			extern struct nrf_bt_adv_data_provider _nrf_bt_adv_ad_list_start[];
			extern struct nrf_bt_adv_data_provider _nrf_bt_adv_ad_list_end[];

			*start = _nrf_bt_adv_ad_list_start;
			*end = _nrf_bt_adv_ad_list_end;
			break;

		case PROVIDER_SET_SD:;
			extern struct nrf_bt_adv_data_provider _nrf_bt_adv_sd_list_start[];
			extern struct nrf_bt_adv_data_provider _nrf_bt_adv_sd_list_end[];

			*start = _nrf_bt_adv_sd_list_start;
			*end = _nrf_bt_adv_sd_list_end;
			break;

		default:
			/* Should not happen. */
			__ASSERT_NO_MSG(false);
			start = NULL;
			end = NULL;
			break;
	}
}

static size_t get_section_size(enum provider_set set)
{
	struct nrf_bt_adv_data_provider *start;
	struct nrf_bt_adv_data_provider *end;

	get_section_ptrs(set, &start, &end);

	return end - start;
}

size_t nrf_bt_adv_get_ad_prov_cnt(void)
{
	return get_section_size(PROVIDER_SET_AD);
}

size_t nrf_bt_adv_get_sd_prov_cnt(void)
{
	return get_section_size(PROVIDER_SET_SD);
}

static void update_common_feedback(struct nrf_bt_adv_feedback *common_fb,
				   const struct nrf_bt_adv_feedback *fb)
{
	common_fb->grace_period_s = MAX(common_fb->grace_period_s, fb->grace_period_s);
}

static int get_providers_data(enum provider_set set, struct bt_data *d, size_t *d_len,
			      const struct nrf_bt_adv_state *state, struct nrf_bt_adv_feedback *fb)
{
	struct nrf_bt_adv_data_provider *start = NULL;
	struct nrf_bt_adv_data_provider *end = NULL;
	struct nrf_bt_adv_feedback common_fb;

	size_t pos = 0;
	int err = 0;

	get_section_ptrs(set, &start, &end);
	memset(&common_fb, 0, sizeof(common_fb));

	for (struct nrf_bt_adv_data_provider *p = start; p < end; p++) {
		memset(&fb, 0, sizeof(*fb));
		err = p->get_data(&d[pos], state, fb);

		if (!err) {
			pos++;
			update_common_feedback(&common_fb, fb);
		} else if (err == -ENOENT) {
			err = 0;
		} else {
			break;
		}
	}

	*d_len = pos;
	memcpy(fb, &common_fb, sizeof(common_fb));

	return err;
}

int nrf_bt_adv_get_ad(struct bt_data *ad, size_t *ad_len, const struct nrf_bt_adv_state *state,
		      struct nrf_bt_adv_feedback *fb)
{
	return get_providers_data(PROVIDER_SET_AD, ad, ad_len, state, fb);
}

int nrf_bt_adv_get_sd(struct bt_data *sd, size_t *sd_len, const struct nrf_bt_adv_state *state,
		      struct nrf_bt_adv_feedback *fb)
{
	return get_providers_data(PROVIDER_SET_SD, sd, sd_len, state, fb);
}

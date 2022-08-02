/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/byteorder.h>
#include <bluetooth/adv.h>


static int get_data(struct bt_data *ad, const struct nrf_bt_adv_state *state,
		    struct nrf_bt_adv_feedback *fb)
{
	ARG_UNUSED(fb);

	if (state->bond_cnt > 0) {
		return -ENOENT;
	}

	static uint8_t data[sizeof(uint16_t)];

	sys_put_le16(CONFIG_NRF_BT_ADV_PROV_GAP_APPEARANCE_VAL, data);

	ad->type = BT_DATA_GAP_APPEARANCE;
	ad->data_len = sizeof(data);
	ad->data = data;

	return 0;
}

NRF_BT_ADV_AD_PROVIDER_REGISTER(gap_appearance, get_data);

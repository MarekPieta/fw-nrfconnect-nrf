/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/adv.h>


static int get_data(struct bt_data *ad, const struct nrf_bt_adv_state *state,
		    struct nrf_bt_adv_feedback *fb)
{
	ARG_UNUSED(fb);

	static uint8_t flags = BT_LE_AD_NO_BREDR;

	if (state->bond_cnt == 0) {
		flags |= BT_LE_AD_GENERAL;
	}

	ad->type = BT_DATA_FLAGS;
	ad->data_len = sizeof(flags);
	ad->data = &flags;

	return 0;
}

NRF_BT_ADV_AD_PROVIDER_REGISTER(flags, get_data);

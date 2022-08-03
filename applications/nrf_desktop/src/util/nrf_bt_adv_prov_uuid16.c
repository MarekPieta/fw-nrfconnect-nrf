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

	static const uint8_t data[] = {
#if CONFIG_DESKTOP_HIDS_ENABLE
		0x12, 0x18,   /* HID Service */
#endif
#if CONFIG_DESKTOP_BAS_ENABLE
		0x0f, 0x18,   /* Battery Service */
#endif
	};

	ad->type = BT_DATA_UUID16_ALL;
	ad->data_len = sizeof(data);
	ad->data = data;

	return 0;
}

NRF_BT_ADV_AD_PROVIDER_REGISTER(uui16_all, get_data);

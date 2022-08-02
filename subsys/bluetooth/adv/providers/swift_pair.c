/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/byteorder.h>
#include <bluetooth/adv.h>

#define GRACE_PERIOD_S	CONFIG_NRF_BT_ADV_PROV_SWIFT_PAIR_GRACE_PERIOD


static int get_data(struct bt_data *ad, const struct nrf_bt_adv_state *state,
		    struct nrf_bt_adv_feedback *fb)
{
	static const uint8_t data[] = {
		0x06, 0x00,	/* Microsoft Vendor ID */
		0x03,		/* Microsoft Beacon ID */
		0x00,		/* Microsoft Beacon Sub Scenario */
		0x80		/* Reserved RSSI Byte */
	};

	/* Do not advertise Swift Pair data during grace period. */
	if (state->in_grace_period) {
		return -ENOENT;
	}

	ad->type = BT_DATA_MANUFACTURER_DATA;
	ad->data_len = sizeof(data);
	ad->data = data;

	fb->grace_period_s = GRACE_PERIOD_S;

	return 0;
}

NRF_BT_ADV_AD_PROVIDER_REGISTER(swift_pair, get_data);

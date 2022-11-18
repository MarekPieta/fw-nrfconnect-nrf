/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/adv_prov.h>
#include <bluetooth/services/fast_pair.h>

#define ADV_DATA_BUF_SIZE	CONFIG_BT_ADV_PROV_FAST_PAIR_ADV_BUF_SIZE

static bool enabled = true;
static bool show_ui_pairing = IS_ENABLED(CONFIG_BT_ADV_PROV_FAST_PAIR_SHOW_UI_PAIRING);

static struct bt_data gen_ad;
static enum bt_fast_pair_adv_mode gen_adv_mode = BT_FAST_PAIR_ADV_MODE_COUNT;


void bt_le_adv_prov_fast_pair_enable(bool enable)
{
	enabled = enable;

	if (!enabled) {
		/* Invalidate generated payload. */
		gen_adv_mode = BT_FAST_PAIR_ADV_MODE_COUNT;
	}
}

void bt_le_adv_prov_fast_pair_show_ui_pairing(bool enable)
{
	show_ui_pairing = enable;
}

static enum bt_fast_pair_adv_mode get_adv_mode(bool pairing_mode)
{
	enum bt_fast_pair_adv_mode adv_mode;

	if (pairing_mode) {
		adv_mode = BT_FAST_PAIR_ADV_MODE_DISCOVERABLE;
	} else {
		if (show_ui_pairing) {
			adv_mode = BT_FAST_PAIR_ADV_MODE_NOT_DISCOVERABLE_SHOW_UI_IND;
		} else {
			adv_mode = BT_FAST_PAIR_ADV_MODE_NOT_DISCOVERABLE_HIDE_UI_IND;
		}
	}

	return adv_mode;
}

static int generate_payload(enum bt_fast_pair_adv_mode adv_mode)
{
	static uint8_t buf[ADV_DATA_BUF_SIZE];

	__ASSERT_NO_MSG(bt_fast_pair_adv_data_size(adv_mode) <= sizeof(buf));

	int err = bt_fast_pair_adv_data_fill(&gen_ad, buf, sizeof(buf), adv_mode);

	if (!err) {
		gen_adv_mode = adv_mode;
	} else {
		gen_adv_mode = BT_FAST_PAIR_ADV_MODE_COUNT;
	}

	return err;
}

static int get_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	enum bt_fast_pair_adv_mode expected_adv_mode = get_adv_mode(state->pairing_mode);
	bool generate_new_payload = (expected_adv_mode != gen_adv_mode);
	int err = 0;

	if (!enabled) {
		return -ENOENT;
	}

	if (IS_ENABLED(CONFIG_BT_ADV_PROV_FAST_PAIR_STOP_DISCOVERABLE_ON_RPA_ROTATION) &&
	    (gen_adv_mode == BT_FAST_PAIR_ADV_MODE_DISCOVERABLE) &&
	    state->rpa_rotated && !state->new_adv_session) {
		LOG_WRN("RPA rotated during Fast Pair discoverable advertising session");
		LOG_WRN("Removed Fast Pair advertising payload");
		return -ENOENT;
	}

	if ((gen_adv_mode == BT_FAST_PAIR_ADV_MODE_NOT_DISCOVERABLE_SHOW_UI_IND) ||
	    (gen_adv_mode == BT_FAST_PAIR_ADV_MODE_NOT_DISCOVERABLE_HIDE_UI_IND)) {
		if (state->rpa_rotated) {
			generate_new_payload = true;
		}
	}

	if (generate_new_payload) {
		err = generate_payload(expected_adv_mode);
	}

	if (!err) {
		ad = gen_ad;
		if (IS_ENABLED(CONFIG_BT_ADV_PROV_FAST_PAIR_AUTO_SET_PAIRING_MODE)) {
			/* Set Fast Pair pairing mode to match Fast Pair advertising mode. */
			bt_fast_pair_set_pairing_mode(expected_adv_mode ==
						      BT_FAST_PAIR_ADV_MODE_DISCOVERABLE);
		}
	}

	return err;
}

BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(fast_pair, get_data);

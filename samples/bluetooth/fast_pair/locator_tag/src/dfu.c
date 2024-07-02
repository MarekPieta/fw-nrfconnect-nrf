/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/settings/settings.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/adv_prov.h>

#include <bluetooth/services/fast_pair/fast_pair.h>
#include <bluetooth/services/fast_pair/fmdn.h>

#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

#include "app_dfu.h"
#include "app_fp_adv.h"
#include "app_ui.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fp_fmdn, LOG_LEVEL_INF);

/* DFU mode timeout in minutes. */
#define DFU_MODE_TIMEOUT (1)

/* UUID of the SMP service used for the DFU. */
#define BT_UUID_SMP_SVC_VAL \
	BT_UUID_128_ENCODE(0x8D53DC1D, 0x1DB7, 0x4CD3, 0x868B, 0x8A527460AA84)

/* UUID of the SMP characteristic used for the DFU. */
#define BT_UUID_SMP_CHAR_VAL	\
	BT_UUID_128_ENCODE(0xda2e7828, 0xfbce, 0x4e01, 0xae9e, 0x261174997c48)

#define BT_UUID_SMP_CHAR	BT_UUID_DECLARE_128(BT_UUID_SMP_CHAR_VAL)

static bool fmdn_provisioned;
static bool dfu_mode;

static bool ad_enabled;
static bool sd_enabled;

static void dfu_mode_timeout_work_handle(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(dfu_mode_timeout_work, dfu_mode_timeout_work_handle);

static void fp_adv_provisioning_state_changed(bool provisioned)
{
	fmdn_provisioned = provisioned;
}

static struct bt_fast_pair_fmdn_info_cb fmdn_info_cb = {
	.provisioning_state_changed = fp_adv_provisioning_state_changed,
};

bool app_dfu_is_smp_char(const struct bt_uuid *uuid)
{
	return bt_uuid_cmp(uuid, BT_UUID_SMP_CHAR) == 0;
}

bool app_dfu_is_dfu_mode(void)
{
	return dfu_mode;
}

void app_dfu_adv_prov_enable(bool ad_enable, bool sd_enable)
{
	__ASSERT(!(ad_enable && sd_enable), "Both AD and SD cannot be enabled at the same time");

	ad_enabled = ad_enable;
	sd_enabled = sd_enable;
}

static void fill_data(struct bt_data *ad)
{
	/* UUID of the SMP service used for the DFU. */
	static const uint8_t data[] = {BT_UUID_SMP_SVC_VAL};

	ad->type = BT_DATA_UUID128_ALL;
	ad->data_len = sizeof(data);
	ad->data = data;
}

static int get_ad_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	if (!ad_enabled) {
		return -ENOENT;
	}

	fill_data(ad);

	return 0;
}

static int get_sd_data(struct bt_data *sd, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	if (!sd_enabled) {
		return -ENOENT;
	}

	fill_data(sd);

	return 0;
}

/* Used in discoverable adv */
BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(smp_ad, get_ad_data);

/* Used in the not-discoverable adv */
BT_LE_ADV_PROV_SD_PROVIDER_REGISTER(smp_sd, get_sd_data);

static enum mgmt_cb_return smp_cmd_recv(uint32_t event, enum mgmt_cb_return prev_status,
					int32_t *rc, uint16_t *group, bool *abort_more,
					void *data, size_t data_size)
{
	const struct mgmt_evt_op_cmd_arg *cmd_recv;

	if (event != MGMT_EVT_OP_CMD_RECV) {
		LOG_ERR("Spurious event in recv cb: %" PRIu32, event);
		*rc = MGMT_ERR_EUNKNOWN;
		return MGMT_CB_ERROR_RC;
	}

	LOG_DBG("MCUmgr SMP Command Recv Event");

	if (data_size != sizeof(*cmd_recv)) {
		LOG_ERR("Invalid data size in recv cb: %zu (expected: %zu)",
			data_size, sizeof(*cmd_recv));
		*rc = MGMT_ERR_EUNKNOWN;
		return MGMT_CB_ERROR_RC;
	}

	cmd_recv = data;

	/* Ignore commands not related to DFU over SMP. */
	if (!(cmd_recv->group == MGMT_GROUP_ID_IMAGE) &&
	    !((cmd_recv->group == MGMT_GROUP_ID_OS) && (cmd_recv->id == OS_MGMT_ID_RESET))) {
		return MGMT_CB_OK;
	}

	LOG_DBG("MCUmgr %s event", (cmd_recv->group == MGMT_GROUP_ID_IMAGE) ?
		"Image Management" : "OS Management Reset");

	k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT));

	return MGMT_CB_OK;
}

static struct mgmt_callback cmd_recv_cb = {
	.callback = smp_cmd_recv,
	.event_id = MGMT_EVT_OP_CMD_RECV,
};

static void dfu_mode_action_handle(void)
{
	if (dfu_mode) {
		LOG_INF("DFU: refreshing the DFU mode timeout");
	} else {
		LOG_INF("DFU: entering the DFU mode for %d minute(s)",
			DFU_MODE_TIMEOUT);
		LOG_INF("DFU: advertising mode will automatically change to %sdiscoverable",
			fmdn_provisioned ? "not " : "");
	}

	k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT));

	dfu_mode = true;

	app_fp_adv_mode_set(fmdn_provisioned ?
			    APP_FP_ADV_MODE_NOT_DISCOVERABLE :
			    APP_FP_ADV_MODE_DISCOVERABLE);

	app_ui_state_change_indicate(APP_UI_STATE_DFU_MODE, dfu_mode);
}

static void dfu_mode_timeout_work_handle(struct k_work *w)
{
	LOG_INF("DFU: timeout expired");
	LOG_INF("DFU: advertising mode will be refreshed");

	dfu_mode = false;

	app_fp_adv_mode_set(fmdn_provisioned ?
			    APP_FP_ADV_MODE_NOT_DISCOVERABLE :
			    APP_FP_ADV_MODE_DISCOVERABLE);

	app_ui_state_change_indicate(APP_UI_STATE_DFU_MODE, dfu_mode);
}

void app_dfu_fw_version_log(void)
{
	LOG_INF("Firmware version: %s", CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION);
}

int app_dfu_init(void)
{
	int err;

	err = bt_fast_pair_fmdn_info_cb_register(&fmdn_info_cb);
	if (err) {
		LOG_ERR("Fast Pair: bt_fast_pair_fmdn_info_cb_register returned error: %d", err);
		return err;
	}

	mgmt_callback_register(&cmd_recv_cb);

	return 0;
}

static void dfu_mode_request_handle(enum app_ui_request request)
{
	/* It is assumed that the callback executes in the cooperative
	 * thread context as it interacts with the FMDN API.
	 */
	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (request == APP_UI_REQUEST_DFU_MODE_ENTER) {
		dfu_mode_action_handle();
	}
}

APP_UI_REQUEST_LISTENER_REGISTER(dfu_mode_request_handler, dfu_mode_request_handle);

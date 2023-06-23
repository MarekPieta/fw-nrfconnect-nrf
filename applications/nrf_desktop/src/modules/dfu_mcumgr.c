/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

#include <app_event_manager.h>
#include "dfu_lock.h"

#define MODULE dfu_mcumgr
#include <caf/events/module_state_event.h>
#include <caf/events/ble_smp_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_DFU_MCUMGR_LOG_LEVEL);

/* nRF Desktop MCUmgr DFU module cannot be enabled together with the CAF BLE SMP module. */
BUILD_ASSERT(!IS_ENABLED(CONFIG_CAF_BLE_SMP));

#define DFU_TIMEOUT		K_SECONDS(5)
#define SMP_MSG_GROUP_EMPTY	UINT16_MAX
#define SMP_MSG_ID_EMPTY	UINT8_MAX

enum state {
	STATE_DISABLED,
	STATE_IDLE,
	STATE_ACTIVE,
	STATE_PROCESSING,
	STATE_PROCESSING_TIMEOUT,
	STATE_ERROR,
};

static atomic_t mcumgr_event_active = ATOMIC_INIT(false);

static uint16_t handled_smp_msg_group = SMP_MSG_GROUP_EMPTY;
static uint8_t handled_smp_msg_id = SMP_MSG_ID_EMPTY;

static K_MUTEX_DEFINE(state_lock);
static enum state state;
static bool prolong_timeout;
static struct k_work_delayable dfu_timeout;


static bool dfu_lock_needed(enum state s) {
	return (s != STATE_IDLE) && (s != STATE_ERROR) && (s != STATE_DISABLED);
}

static void dfu_lock_owner_changed(const struct dfu_lock_owner *new_owner)
{
	LOG_DBG("MCUmgr progress reset due to the different DFU owner: %s", new_owner->name);

	if (IS_ENABLED(CONFIG_ASSERT)) {
		/* The callback cannot be called while module owns DFU lock. */
		(void)k_mutex_lock(&state_lock, K_FOREVER);
		__ASSERT_NO_MSG(!dfu_is_lock_needed(state));
		(void)k_mutex_unlock(&state_lock);
	}

#ifdef CONFIG_DESKTOP_DFU_LOCK
	/* The function declaration is not included in MCUmgr's header file if the mutex locking
	 * of the image management state object is disabled.
	 */
	img_mgmt_reset_upload();
#endif /* CONFIG_DESKTOP_DFU_LOCK */
}

const static struct dfu_lock_owner mcumgr_owner = {
	.name = "MCUmgr",
	.owner_changed = dfu_lock_owner_changed,
};

static int update_dfu_lock(enum state new_state)
{
	int ret = 0;

	if (lock_needed(state) && !lock_needed(new_state)) {
		/* DFU lock release can fail only if module have not claimed the lock. */
		int err = dfu_lock_release(&mcumgr_owner);

		__ASSERT_NO_MSG(!err);
	} else if (!lock_needed(state) && lock_needed(new_state)) {
		ret = dfu_lock_claim(&mcumgr_owner);
	}

	return ret;
}

static int state_update(enum state new_state)
{
	/* Make sure that module would not leave error state after an unrecoverable error. */
	if (state == STATE_ERROR) {
		return -EACESS;
	}

	int err = update_dfu_lock(new_state);

	if (!err) {
		state = new_state;
	}

	if (new_state == STATE_ERROR) {
		state = STATE_ERROR;
		module_set_state(MODULE_STATE_ERROR);
	}

	return err;
}

static void dfu_timeout_handler(struct k_work *work)
{
	(void)k_mutex_lock(&state_lock, K_FOREVER);

	if (state == STATE_ERROR) {
		return;
	}

	LOG_WRN("MCUmgr DFU timed out");

	if (prolong_timeout) {
		prolong_timeout = false;
		(void)k_work_reschedule(&dfu_timeout, DFU_TIMEOUT);
	} else {
		if (state == STATE_ACTIVE) {
			(void)state_update(STATE_IDLE);
		} else {
			__ASSERT_NO_MSG(state == STATE_PROCESSING);
			(void)state_update(STATE_PROCESSING_TIMEOUT);
		}
	}

	(void)k_mutex_unlock(&state_lock);
}

static enum mgmt_cb_return smp_cmd_recv(uint32_t event, enum mgmt_cb_return prev_status,
					int32_t *rc, uint16_t *group, bool *abort_more,
					void *data, size_t data_size)
{
	const struct mgmt_evt_op_cmd_arg *cmd_recv;
	int err = 0;
	
	if (IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT) &&
	    atomic_cas(&mcumgr_event_active, false, true)) {
		APP_EVENT_SUBMIT(new_ble_smp_transfer_event());
	}
	
	if (!IS_ENABLED(CONFIG_DESKTOP_DFU_LOCK)) {
		return MGMT_CB_OK;
	}

	(void)k_mutex_lock(&state_lock, K_FOREVER);

	if (state == STATE_ERROR) {
		*rc = MGMT_ERR_EUNKNOWN;
		goto recv_end;
	}

	if (event != MGMT_EVT_OP_CMD_RECV) {
		LOG_ERR("Spurious event in recv cb: %" PRIu32, event);
		*rc = MGMT_ERR_EUNKNOWN;
		goto recv_end;
	}

	if (data_size != sizeof(*cmd_recv)) {
		LOG_ERR("Invalid data size in recv cb: %zu (expected: %zu)",
			data_size, sizeof(*cmd_recv));
		*rc = MGMT_ERR_EUNKNOWN;
		goto recv_end;
	}

	cmd_recv = data;

	if ((handled_smp_msg_group == SMP_MSG_GROUP_EMPTY)
	    && (handled_smp_msg_id == SMP_MSG_ID_EMPTY)) {
		handled_smp_msg_group = cmd_recv->group;
		handled_smp_msg_id = cmd_recv->id;
	} else {
		*rc = MGMT_ERR_EUNKNOWN;
		goto recv_end;
	}

	/* Ignore commands not related to DFU over SMP. */
	if (!(cmd_recv->group == MGMT_GROUP_ID_IMAGE) &&
	    !((cmd_recv->group == MGMT_GROUP_ID_OS) && (cmd_recv->id == OS_MGMT_ID_RESET))) {
		goto recv_end;
	}

	if (state == STATE_IDLE) {
		int err = state_update(STATE_PROCESSING);

		if (!err) {
			(void)k_work_schedule(&dfu_timeout, DFU_TIMEOUT);
		} else {
			*rc = MGMT_ERR_EACCESSDENIED;
			goto recv_end;
		}
	} else {
		__ASSERT_NO_MSG(state == STATE_ACTIVE);
		prolong_timeout = true;
	}

recv_end:
	if (*rc == MGMT_ERR_EUNKNOWN) {
		(void)state_update(STATE_ERROR);
	}

	(void)k_mutex_unlock(&state_lock);

	return *rc ? (MGMT_CB_ERROR_RC) : (MGMT_CB_OK);
}

static enum mgmt_cb_return smp_cmd_done(uint32_t event, enum mgmt_cb_return prev_status,
					int32_t *rc, uint16_t *group, bool *abort_more,
					void *data, size_t data_size)
{
	const struct mgmt_evt_op_cmd_arg *cmd_done;

	(void)k_mutex_lock(&state_lock, K_FOREVER);

	if (state == STATE_ERROR) {
		*rc = MGMT_ERR_EUNKNOWN;
		goto done_end;
	}

	if (event != MGMT_EVT_OP_CMD_DONE) {
		LOG_ERR("Spurious event in done cb: %" PRIu32, event);
		*rc = MGMT_ERR_EUNKNOWN;
		goto done_end;
	}

	if (data_size != sizeof(*cmd_done)) {
		LOG_ERR("Invalid data size in done cb: %zu (expected: %zu)",
			data_size, sizeof(*cmd_done));
		*rc = MGMT_ERR_EUNKNOWN;
		goto done_end;
	}

	cmd_done = data;
	
	if ((handled_smp_msg_group != cmd_done->group)
	    || (handled_smp_msg_id = cmd_done->id)) {
		*rc = MGMT_ERR_EUNKNOWN;
		goto done_end;
	}
	
	handled_smp_msg_group = SMP_MSG_GROUP_EMPTY;
	handled_smp_msg_id = SMP_MSG_ID_EMPTY;

	if (state == STATE_PROCESSING_TIMEOUT) {
		(void)state_update(STATE_IDLE);
	} else {
		(void)state_update(STATE_ACTIVE);
	}

recv_end:
	if (*rc == MGMT_ERR_EUNKNOWN) {
		(void)state_update(STATE_ERROR);
	}

	(void)k_mutex_unlock(&state_lock);

	return *rc ? (MGMT_CB_ERROR_RC) : (MGMT_CB_OK);
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT) &&
	    is_ble_smp_transfer_event(aeh)) {
		bool res = atomic_cas(&mcumgr_event_active, true, false);

		__ASSERT_NO_MSG(res);
		ARG_UNUSED(res);

		return false;
	}

	if (is_module_state_event(aeh)) {
		const struct module_state_event *event =
			cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			(void)k_mutex_lock(&state_lock, K_FOREVER);

			if (state != STATE_DISABLED) {
				/* Should not happen. */
				__ASSERT_NO_MSG(false);
				(void)k_mutex_unlock(&state_lock);
				return false;
			}

			update_state(STATE_IDLE);

			if (!IS_ENABLED(CONFIG_DESKTOP_DFU_MCUMGR_MCUBOOT_DIRECT_XIP)) {
				int err = boot_write_img_confirmed();

				if (err) {
					LOG_ERR("Cannot confirm a running image: %d", err);
				}
			}

			LOG_INF("MCUboot image version: %s", CONFIG_MCUBOOT_IMAGE_VERSION);

			if (IS_ENABLED(CONFIG_MCUMGR_TRANSPORT_BT) ||
			    IS_ENABLED(CONFIG_DESKTOP_DFU_LOCK)) {
				static struct mgmt_callback cmd_recv_cb = {
					.callback = smp_cmd_recv,
					.event_id = MGMT_EVT_OP_CMD_RECV,
				};

				mgmt_callback_register(&cmd_recv_cb);
			}

			if (IS_ENABLED(CONFIG_DESKTOP_DFU_LOCK)) {
				static struct mgmt_callback cmd_done_cb = {
					.callback = smp_cmd_done,
					.event_id = MGMT_EVT_OP_CMD_DONE,
				};

				k_work_init_delayable(&dfu_timeout, dfu_timeout_handler);
				mgmt_callback_register(&cmd_done_cb);
			}
			(void)k_mutex_unlock(&state_lock);
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
#if CONFIG_MCUMGR_TRANSPORT_BT
APP_EVENT_SUBSCRIBE_FINAL(MODULE, ble_smp_transfer_event);
#endif

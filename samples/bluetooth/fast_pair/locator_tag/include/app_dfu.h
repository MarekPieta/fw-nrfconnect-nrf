/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_DFU_H_
#define APP_DFU_H_

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/bluetooth/uuid.h>

/**
 * @defgroup app_dfu Device Firmware Update (DFU) API
 * @brief Device Firmware Update (DFU) API
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Check if the UUID is the SMP characteristic UUID.
 *
 *  @param uuid UUID of the GATT characteristic.
 *  @return true if checked GATT characteristic UUID is the SMP characteristic, otherwise false
 */
bool app_dfu_is_smp_char(const struct bt_uuid *uuid);

/** Enable the SMP UUID in the advertising or scan response data.
 *
 *  Due to using legacy advertising set size, add SMP UUID to either AD or SD,
 *  depending on the space availability related to the advertising mode.
 *  Otherwise, the advertising set size would be exceeded and the advertising
 *  would not start.
 *  The SMP UUID can be added only to one of the data sets.
 *
 *  @param ad_enable Add the SMP UUID in the advertising data.
 *  @param sd_enable Add the SMP UUID in the scan response data.
 */
void app_dfu_adv_prov_enable(bool ad_enable, bool sd_enable);

/** Log the current firmware version. */
void app_dfu_fw_version_log(void);

/** Check if the device is in the DFU mode.
 *
 *  @return true if the device is in the DFU mode, otherwise false
 */
bool app_dfu_is_dfu_mode(void);

/** Initialize the DFU module.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* APP_DFU_H_ */

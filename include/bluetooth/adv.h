/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief nRF Bluetooth advertising subsystem header.
 */

#ifndef NRF_BT_ADV_H_
#define NRF_BT_ADV_H_

#include <zephyr/bluetooth/bluetooth.h>

/**
 * @defgroup nrf_adv nRF Bluetooth advertising subsystem
 * @brief The subsystem manages advertising packets and scan response packets.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Structure describing Bluetooth advertising state. */
struct nrf_bt_adv_state {
	/** Number of Bluetooth bonds of Bluetooth local identity used for advertising. */
	size_t bond_cnt;

	/** Advertising in grace period. */
	bool in_grace_period;
};

/** Structure describing feedback reported by advertising providers. */
struct nrf_bt_adv_feedback {
	/** Required grace period [seconds]. */
	uint16_t grace_period_s;
};

/**
 * @typedef nrf_bt_adv_data_get
 * Callback used to get provider's advertising or scan response data.
 *
 * @param[out] d	Pointer to the structure to be filled with data.
 * @param[in]  state	Pointer to structure describing Bluetooth advertising state.
 * @param[out] fb	Pointer to structure describing feedback reported by advertising provider.
 *
 * @retval 0		If the operation was successful.
 * @retval (-ENOENT)	If provider does not provide data.
 * @return		Other negative value on error specific to provider.
 */
typedef int (*nrf_bt_adv_data_get)(struct bt_data *d, const struct nrf_bt_adv_state *state,
				   struct nrf_bt_adv_feedback *fb);

/** Structure describing advertising data provider. */
struct nrf_bt_adv_data_provider {
	/** Function used to get provider's data. */
	nrf_bt_adv_data_get get_data;
};

/** Register advertising data provider.
 *
 * The macro statically registers an advertising data provider. The provider appends data to
 * advertising packet managed by the nRF advertising subsystem.
 *
 * @param pname		Provider name.
 * @param get_data_fn	Function used to get provider's advertising data.
 */
#define NRF_BT_ADV_AD_PROVIDER_REGISTER(pname, get_data_fn)					\
	STRUCT_SECTION_ITERABLE_ALTERNATE(nrf_bt_adv_ad, nrf_bt_adv_data_provider, pname) = {	\
		.get_data = get_data_fn,							\
	}

/** Register scan response data provider.
 *
 * The macro statically registers a scan response data provider. The provider appends data to
 * scan response packet managed by the nRF advertising subsystem.
 *
 * @param pname		Provider name.
 * @param get_data_fn	Function used to get provider's scan response data.
 */
#define NRF_BT_ADV_SD_PROVIDER_REGISTER(pname, get_data_fn)					\
	STRUCT_SECTION_ITERABLE_ALTERNATE(nrf_bt_adv_sd, nrf_bt_adv_data_provider, pname) = {	\
		.get_data = get_data_fn,							\
	}

/** Get number of advertising data packet providers.
 *
 * The number of advetising data packet providers defines maximum number of elements in advertising
 * packet that can be provided by providers. An advertising data provider may not provide data.
 *
 * @return Number of advertising data packet providers.
 */
size_t nrf_bt_adv_get_ad_prov_cnt(void);

/** Get number of scan response data packet providers.
 *
 * The number of scan response data packet providers defines maximum number of elements in scan
 * response packet that can be provided by providers. A scan response data provider may not provide
 * data.
 *
 * @return Number of scan response data packet providers.
 */
size_t nrf_bt_adv_get_sd_prov_cnt(void);

/** Fill data used in advertising packet.
 *
 * Number of elements in array pointed by ad must be at least equal to @ref
 * nrf_bt_adv_get_ad_prov_cnt.
 *
 * @param[out]    ad		Pointer to array with data for advertising packet.
 * @param[in/out] ad_len        Value describing number of elements in the array pointed by ad.
 *				The value is then set by the function to number of filled elements.
 * @param[in]     state		Structure describing advertising state.
 * @param[out]    fb		Structure filled with feedback from advertising data providers.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int nrf_bt_adv_get_ad(struct bt_data *ad, size_t *ad_len, const struct nrf_bt_adv_state *state,
		      struct nrf_bt_adv_feedback *fb);

/** Fill data used in scan response packet.
 *
 * Number of elements in array pointed by sd must be at least equal to @ref
 * nrf_bt_adv_get_sd_prov_cnt.
 *
 * @param[out]    sd		Pointer to array with data for scan response packet.
 * @param[in/out] sd_len        Value describing number of elements in the array pointed by sd.
 *				The value is then set by the function to number of filled elements.
 * @param[in]     state		Structure describing advertising state.
 * @param[out]    fb		Structure filled with feedback from advertising data providers.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int nrf_bt_adv_get_sd(struct bt_data *sd, size_t *sd_len, const struct nrf_bt_adv_state *state,
		      struct nrf_bt_adv_feedback *fb);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* NRF_BT_ADV_H_ */

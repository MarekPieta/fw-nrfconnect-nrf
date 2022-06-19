/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _API_TEST_H_
#define _API_TEST_H_

/**
 * @defgroup fp_storage_test_api_test Fast Pair storage API unit test
 * @brief Unit test of the Fast Pair storage API
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Start test suite for Fast Pair storage API.
 *
 *  The test suite checks correctness of the Fast Pair storage API implementation.
 */
void api_test_run(void);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _API_TEST_H_ */

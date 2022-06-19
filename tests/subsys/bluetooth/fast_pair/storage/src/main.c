/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "api_test.h"
#include "corrupted_data_test.h"


void test_main(void)
{
	api_test_run();
	corrupted_data_test_run();
}

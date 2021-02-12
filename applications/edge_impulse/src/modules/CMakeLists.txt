#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

target_sources_ifdef(CONFIG_EI_APP_SENSOR_SIM_CTRL_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sensor_sim_ctrl.c)

target_sources_ifdef(CONFIG_EI_APP_SENSOR_SAMPLER_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sensor_sampler.c)

target_sources_ifdef(CONFIG_EI_APP_DATA_FORWARDER_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ei_data_forwarder.c)

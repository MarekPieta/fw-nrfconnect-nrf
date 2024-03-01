# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

if(${images_added})
  # same source directory is used for all targets
  # to avoid recursive calls do not add project if remote was already added
  return()
endif()

if(NOT SB_CONFIG_IMAGE_2_BOARD STREQUAL "")

  list(INSERT IMAGES 0 coremark_image_2)

  ExternalZephyrProject_Add(
    APPLICATION coremark_image_2
    SOURCE_DIR ${APP_DIR}
    BOARD ${SB_CONFIG_IMAGE_2_BOARD}
    BOARD_REVISION ${BOARD_REVISION}
  )

endif()

if(NOT SB_CONFIG_IMAGE_3_BOARD STREQUAL "")

  list(INSERT IMAGES 0 coremark_image_3)

  ExternalZephyrProject_Add(
    APPLICATION coremark_image_3
    SOURCE_DIR ${APP_DIR}
    BOARD ${SB_CONFIG_IMAGE_3_BOARD}
    BOARD_REVISION ${BOARD_REVISION}
  )

endif()

set(images_added 1)

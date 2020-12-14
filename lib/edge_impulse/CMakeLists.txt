#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.18)

# Set subdirectory in build dir
set(EI_BASE_DIR ${CMAKE_BINARY_DIR}/ei)
set(EI_UNZIPPED_DIR ${EI_BASE_DIR}/unpacked)
set(EI_BINARY_DIR ${EI_BASE_DIR}/bin)

if(NOT EXISTS ${EI_BASE_DIR})
  file(MAKE_DIRECTORY ${EI_BASE_DIR})
endif()

# Prepare Edge Impulse library zip archive
if(CONFIG_EDGE_IMPULSE_BUILD_FROM_ZIP)

  if(CONFIG_EDGE_IMPULSE_ZIP_PATH STREQUAL "")
    message(FATAL_ERROR "CONFIG_EDGE_IMPULSE_ZIP_PATH must be specified in Kconfig.")
  endif()

  set(EI_ZIP_PATH ${APPLICATION_SOURCE_DIR}/${CONFIG_EDGE_IMPULSE_ZIP_PATH})
  if (NOT EXISTS ${EI_ZIP_PATH})
    message(FATAL_ERROR "Specified Edge Impulse archive (${EI_ZIP_PATH}) does "
             "not exist. Download the archive manually and place it under "
             "defined path or use CONFIG_EDGE_IMPULSE_BUILD_FROM_URI Kconfig "
             "option to let the build system download the library "
             "automatically.")
  else()
    message(STATUS "Using Edge Impulse archive: ${EI_ZIP_PATH}.")
  endif()

elseif(CONFIG_EDGE_IMPULSE_BUILD_FROM_URI)

  if(CONFIG_EDGE_IMPULSE_DOWNLOAD_URI STREQUAL "")
    message(FATAL_ERROR "CONFIG_EDGE_IMPULSE_DOWNLOAD_URI must be specified in Kconfig.")
  endif()

  set(EI_ZIP_PATH ${EI_BASE_DIR}/src.zip)
  set(EI_URI_DUMP ${EI_BASE_DIR}/uri.txt)

  if(EXISTS ${EI_URI_DUMP})
    file(READ ${EI_URI_DUMP} EI_URI)
  else()
    set(EI_URI " ")
  endif()

  if((NOT EXISTS ${EI_ZIP_PATH}) OR
     (NOT CONFIG_EDGE_IMPULSE_DOWNLOAD_URI STREQUAL ${EI_URI}))
    # Start download, use API key that was provided.
    message(STATUS "Downloading Edge Impulse files, please wait...")

    if(DEFINED EI_API_KEY_HEADER)
      file(DOWNLOAD ${CONFIG_EDGE_IMPULSE_DOWNLOAD_URI} ${EI_ZIP_PATH}
           SHOW_PROGRESS
           STATUS download_status
           HTTPHEADER "Accept: application/zip"
           HTTPHEADER ${EI_API_KEY_HEADER})
    else()
      file(DOWNLOAD ${CONFIG_EDGE_IMPULSE_DOWNLOAD_URI} ${EI_ZIP_PATH}
           SHOW_PROGRESS
           STATUS download_status
           HTTPHEADER "Accept: application/zip")
    endif()

    list(GET download_status 0 status_code)
    if (status_code EQUAL 0)
      message(STATUS "Successfully downloaded Edge Impulse archive.")
      file(WRITE ${EI_URI_DUMP} ${CONFIG_EDGE_IMPULSE_DOWNLOAD_URI})
    else()
      file(REMOVE ${EI_ZIP_PATH})
      file(REMOVE ${EI_URI_DUMP})
      message(FATAL_ERROR "Failed to download Edge Impulse files. Please make "
               "sure that CONFIG_EDGE_IMPULSE_DOWNLOAD_URI is properly defined "
               "and EI_API_KEY_HEADER is specified (if needed).")
    endif()
  else()
    message(STATUS "Edge Impulse zip archive is already downloaded.")
  endif()

else()
  message(FATAL_ERROR "Unknown Edge Impulse build source.")
endif()

# Rerun CMake when zip file content changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${EI_ZIP_PATH})

# Unzip Edge impulse source files
file(REMOVE_RECURSE ${EI_UNZIPPED_DIR})
file(MAKE_DIRECTORY ${EI_UNZIPPED_DIR})
file(ARCHIVE_EXTRACT INPUT ${EI_ZIP_PATH} DESTINATION ${EI_UNZIPPED_DIR})

# Build Edge Impulse library
zephyr_library_named(edge_impulse)

# Edge impulse SDK include directories
set(EI_SDK_INCLUDES
    ${EI_UNZIPPED_DIR}/.
    ${EI_UNZIPPED_DIR}/tflite-model
    ${EI_UNZIPPED_DIR}/model-parameters
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/third_party/ruy
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/third_party/gemmlowp
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/third_party/flatbuffers/include
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/third_party
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/tensorflow
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/dsp
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/classifier
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/anomaly
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/CMSIS/NN/Include
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/CMSIS/DSP/PrivateInclude
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/CMSIS/DSP/Include
    ${EI_UNZIPPED_DIR}/edge-impulse-sdk/CMSIS/Core/Include
    )

# Library source files
macro(RECURSIVE_FIND_FILE return_list dir pattern)
    file(GLOB_RECURSE new_list "${dir}/${pattern}")
    set(dir_list "")
    foreach(file_path ${new_list})
        set(dir_list ${dir_list} ${file_path})
    endforeach()
    list(REMOVE_DUPLICATES dir_list)
    set(${return_list} ${dir_list})
endmacro()

RECURSIVE_FIND_FILE(EI_SOURCE_FILES ${EI_UNZIPPED_DIR}/edge-impulse-sdk "*.cpp")
RECURSIVE_FIND_FILE(EI_CC_FILES ${EI_UNZIPPED_DIR}/edge-impulse-sdk "*.cc")
RECURSIVE_FIND_FILE(EI_S_FILES ${EI_UNZIPPED_DIR}/edge-impulse-sdk "*.s")
RECURSIVE_FIND_FILE(EI_C_FILES ${EI_UNZIPPED_DIR}/edge-impulse-sdk "*.c")
RECURSIVE_FIND_FILE(EI_MODEL_FILES ${EI_UNZIPPED_DIR}/tflite-model "*.cpp")
list(APPEND EI_SOURCE_FILES ${EI_S_FILES})
list(APPEND EI_SOURCE_FILES ${EI_C_FILES})
list(APPEND EI_SOURCE_FILES ${EI_CC_FILES})
list(APPEND EI_SOURCE_FILES ${EI_MODEL_FILES})

# Inform user if Edge Impulse source files are missing
list(LENGTH EI_SOURCE_FILES EI_SOURCE_FILES_LEN)
if(EI_SOURCE_FILES_LEN EQUAL 0)
  message(FATAL_ERROR
      "No Edge Impulse library source files available. "
      "That could be related to problems with Edge Impulse *.zip archive. "
      "Please make sure that CONFIG_EDGE_IMPULSE_ZIP_PATH is set correctly.")
endif()

zephyr_library_sources(${EI_SOURCE_FILES})
zephyr_library_include_directories(${EI_SDK_INCLUDES})

# Use hardware acceleration for DSP and Neural Network code on ARM cores
if(CONFIG_ARM)
  zephyr_library_compile_definitions(-DEIDSP_USE_CMSIS_DSP=1
                                     -DEIDSP_LOAD_CMSIS_DSP_SOURCES=1
                                     -DEI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN=1
                                     -DEIDSP_QUANTIZE_FILTERBANK=0
                                     -DARM_MATH_LOOPUNROLL
                                     )
endif()

# Add includes to let application use the library
zephyr_include_directories(${EI_SDK_INCLUDES})

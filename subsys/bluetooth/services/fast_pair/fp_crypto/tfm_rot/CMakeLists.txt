get_target_property(TFM_BINARY_DIR tfm TFM_BINARY_DIR)
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/secure_peripheral_partition/tfm_manifest_list.yaml.in
  ${CMAKE_CURRENT_BINARY_DIR}/secure_peripheral_partition/tfm_manifest_list.yaml
)
set_property(TARGET zephyr_property_target
  APPEND PROPERTY TFM_CMAKE_OPTIONS
  -DTFM_EXTRA_MANIFEST_LIST_FILES=${CMAKE_CURRENT_BINARY_DIR}/secure_peripheral_partition/tfm_manifest_list.yaml
  -DTFM_EXTRA_PARTITION_PATHS=${CMAKE_CURRENT_LIST_DIR}/secure_peripheral_partition
)

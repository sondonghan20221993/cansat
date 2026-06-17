###########################################################
#
# CFS_CORE_APP platform build setup
#
###########################################################

set(CFS_CORE_APP_PLATFORM_CONFIG_FILE_LIST
  cfs_core_app_internal_cfg_values.h
  cfs_core_app_platform_cfg.h
  cfs_core_app_perfids.h
  cfs_core_app_msgids.h
  cfs_core_app_msgid_values.h
)

generate_configfile_set(${CFS_CORE_APP_PLATFORM_CONFIG_FILE_LIST})


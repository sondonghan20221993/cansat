###########################################################
#
# UPLINK_APP platform build setup
#
###########################################################

set(UPLINK_APP_PLATFORM_CONFIG_FILE_LIST
  uplink_app_internal_cfg_values.h
  uplink_app_platform_cfg.h
  uplink_app_perfids.h
  uplink_app_msgids.h
  uplink_app_msgid_values.h
)

generate_configfile_set(${UPLINK_APP_PLATFORM_CONFIG_FILE_LIST})


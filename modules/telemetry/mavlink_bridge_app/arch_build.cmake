###########################################################
#
# MAVLINK_BRIDGE_APP platform build setup
#
###########################################################

set(MAVLINK_BRIDGE_APP_PLATFORM_CONFIG_FILE_LIST
  mavlink_bridge_app_internal_cfg_values.h
  mavlink_bridge_app_platform_cfg.h
  mavlink_bridge_app_perfids.h
  mavlink_bridge_app_msgids.h
  mavlink_bridge_app_msgid_values.h
)

generate_configfile_set(${MAVLINK_BRIDGE_APP_PLATFORM_CONFIG_FILE_LIST})

###########################################################
#
# MAVLINK_BRIDGE_APP mission build setup
#
###########################################################

set(MAVLINK_BRIDGE_APP_MISSION_CONFIG_FILE_LIST
  mavlink_bridge_app_fcncode_values.h
  mavlink_bridge_app_interface_cfg_values.h
  mavlink_bridge_app_mission_cfg.h
  mavlink_bridge_app_perfids.h
  mavlink_bridge_app_msg.h
  mavlink_bridge_app_msgdefs.h
  mavlink_bridge_app_msgstruct.h
  mavlink_bridge_app_topicid_values.h
)

generate_configfile_set(${MAVLINK_BRIDGE_APP_MISSION_CONFIG_FILE_LIST})

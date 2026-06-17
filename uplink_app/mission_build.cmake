###########################################################
#
# UPLINK_APP mission build setup
#
###########################################################

set(UPLINK_APP_MISSION_CONFIG_FILE_LIST
  uplink_app_fcncode_values.h
  uplink_app_interface_cfg_values.h
  uplink_app_mission_cfg.h
  uplink_app_perfids.h
  uplink_app_msg.h
  uplink_app_msgdefs.h
  uplink_app_msgstruct.h
  uplink_app_topicid_values.h
)

generate_configfile_set(${UPLINK_APP_MISSION_CONFIG_FILE_LIST})


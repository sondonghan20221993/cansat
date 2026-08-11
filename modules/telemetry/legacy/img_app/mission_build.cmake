###########################################################
#
# IMG_APP mission build setup
#
###########################################################

set(IMG_APP_MISSION_CONFIG_FILE_LIST
  img_app_fcncode_values.h
  img_app_interface_cfg_values.h
  img_app_mission_cfg.h
  img_app_perfids.h
  img_app_msg.h
  img_app_msgdefs.h
  img_app_msgstruct.h
  img_app_topicid_values.h
)

generate_configfile_set(${IMG_APP_MISSION_CONFIG_FILE_LIST})

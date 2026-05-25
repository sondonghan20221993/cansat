###########################################################
#
# CFS_CORE_APP mission build setup
#
###########################################################

set(CFS_CORE_APP_MISSION_CONFIG_FILE_LIST
  cfs_core_app_fcncode_values.h
  cfs_core_app_interface_cfg_values.h
  cfs_core_app_mission_cfg.h
  cfs_core_app_perfids.h
  cfs_core_app_msg.h
  cfs_core_app_msgdefs.h
  cfs_core_app_msgstruct.h
  cfs_core_app_topicid_values.h
)

generate_configfile_set(${CFS_CORE_APP_MISSION_CONFIG_FILE_LIST})


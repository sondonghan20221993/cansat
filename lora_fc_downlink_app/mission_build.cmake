###########################################################
#
# LORA_FC_DOWNLINK_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the LORA_FC_DOWNLINK_APP configuration
set(LORA_FC_DOWNLINK_APP_MISSION_CONFIG_FILE_LIST
  lora_fc_downlink_app_fcncode_values.h
  lora_fc_downlink_app_interface_cfg_values.h
  lora_fc_downlink_app_mission_cfg.h
  lora_fc_downlink_app_perfids.h
  lora_fc_downlink_app_msg.h
  lora_fc_downlink_app_msgdefs.h
  lora_fc_downlink_app_msgstruct.h
  lora_fc_downlink_app_topicid_values.h
)

generate_configfile_set(${LORA_FC_DOWNLINK_APP_MISSION_CONFIG_FILE_LIST})

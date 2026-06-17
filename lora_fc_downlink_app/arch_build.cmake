###########################################################
#
# LORA_FC_DOWNLINK_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the LORA_FC_DOWNLINK_APP configuration
set(LORA_FC_DOWNLINK_APP_PLATFORM_CONFIG_FILE_LIST
  lora_fc_downlink_app_internal_cfg_values.h
  lora_fc_downlink_app_platform_cfg.h
  lora_fc_downlink_app_perfids.h
  lora_fc_downlink_app_msgids.h
  lora_fc_downlink_app_msgid_values.h
)

generate_configfile_set(${LORA_FC_DOWNLINK_APP_PLATFORM_CONFIG_FILE_LIST})


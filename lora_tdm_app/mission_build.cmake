set(LORA_TDM_APP_MISSION_CONFIG_FILE_LIST
  lora_tdm_app_interface_cfg_values.h
  lora_tdm_app_mission_cfg.h
  lora_tdm_app_msg.h
  lora_tdm_app_msgdefs.h
  lora_tdm_app_msgstruct.h
  lora_tdm_app_topicid_values.h
  lora_tdm_app_fcncode_values.h
)

generate_configfile_set(${LORA_TDM_APP_MISSION_CONFIG_FILE_LIST})

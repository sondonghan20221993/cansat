set(LORA_TDM_APP_PLATFORM_CONFIG_FILE_LIST
  lora_tdm_app_internal_cfg_values.h
  lora_tdm_app_platform_cfg.h
  lora_tdm_app_perfids.h
  lora_tdm_app_msgids.h
  lora_tdm_app_msgid_values.h
)

generate_configfile_set(${LORA_TDM_APP_PLATFORM_CONFIG_FILE_LIST})

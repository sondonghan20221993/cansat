###########################################################
#
# IMG_APP platform build setup
#
###########################################################

set(IMG_APP_PLATFORM_CONFIG_FILE_LIST
  img_app_internal_cfg_values.h
  img_app_platform_cfg.h
  img_app_perfids.h
  img_app_msgids.h
  img_app_msgid_values.h
)

generate_configfile_set(${IMG_APP_PLATFORM_CONFIG_FILE_LIST})

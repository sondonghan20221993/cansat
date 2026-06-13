###########################################################
#
# TELEMETRY_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the TELEMETRY_APP configuration
set(TELEMETRY_APP_PLATFORM_CONFIG_FILE_LIST
  telemetry_app_internal_cfg_values.h
  telemetry_app_platform_cfg.h
  telemetry_app_perfids.h
  telemetry_app_msgids.h
  telemetry_app_msgid_values.h
)

generate_configfile_set(${TELEMETRY_APP_PLATFORM_CONFIG_FILE_LIST})


###########################################################
#
# TELEMETRY_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the TELEMETRY_APP configuration
set(TELEMETRY_APP_MISSION_CONFIG_FILE_LIST
  telemetry_app_fcncode_values.h
  telemetry_app_interface_cfg_values.h
  telemetry_app_mission_cfg.h
  telemetry_app_perfids.h
  telemetry_app_msg.h
  telemetry_app_msgdefs.h
  telemetry_app_msgstruct.h
  telemetry_app_topicid_values.h
)

generate_configfile_set(${TELEMETRY_APP_MISSION_CONFIG_FILE_LIST})

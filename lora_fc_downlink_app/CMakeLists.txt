project(CFE_LORA_FC_DOWNLINK_APP C)

set(APP_SRC_FILES
  fsw/src/lora_fc_downlink_app.c
  fsw/src/lora_fc_downlink_app_cmds.c
  fsw/src/lora_fc_downlink_app_utils.c
  fsw/src/lora_fc_downlink_app_dispatch.c
)

# Create the app module
# NOTE: target/.so name kept short (lora_fc_dl_app.so = 17 chars) to stay under
# OS_MAX_FILE_NAME=20. Full name lora_fc_downlink_app.so (23) caused OSAL -104.
add_cfe_app(lora_fc_dl_app ${APP_SRC_FILES})

target_include_directories(lora_fc_dl_app PUBLIC fsw/inc)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)

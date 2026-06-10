project(CFE_LORA_TDM_APP C)

set(APP_SRC_FILES
  fsw/src/lora_tdm_app.c
  fsw/src/lora_tdm_app_cmds.c
  fsw/src/lora_tdm_app_utils.c
  fsw/src/lora_tdm_app_dispatch.c
)

add_cfe_app(lora_tdm_app ${APP_SRC_FILES})

target_include_directories(lora_tdm_app PUBLIC fsw/inc)

if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)

project(CFE_UPLINK_APP C)

set(APP_SRC_FILES
  fsw/src/uplink_app.c
  fsw/src/uplink_app_cmds.c
  fsw/src/uplink_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/uplink_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/uplink_app_dispatch.c
  )
endif()

add_cfe_app(uplink_app ${APP_SRC_FILES})

target_include_directories(uplink_app PUBLIC fsw/inc)
target_link_libraries(uplink_app m)

if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)


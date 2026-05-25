project(CFE_MAVLINK_BRIDGE_APP C)

set(APP_SRC_FILES
  fsw/src/mavlink_bridge_app.c
  fsw/src/mavlink_bridge_app_cmds.c
  fsw/src/mavlink_bridge_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/mavlink_bridge_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/mavlink_bridge_app_dispatch.c
  )
endif()

add_cfe_app(mavlink_bridge_app ${APP_SRC_FILES})

target_include_directories(mavlink_bridge_app PUBLIC fsw/inc)

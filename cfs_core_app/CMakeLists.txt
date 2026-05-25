project(CFE_CFS_CORE_APP C)

set(APP_SRC_FILES
  fsw/src/cfs_core_app.c
  fsw/src/cfs_core_app_cmds.c
  fsw/src/cfs_core_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/cfs_core_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/cfs_core_app_dispatch.c
  )
endif()

add_cfe_app(cfs_core_app ${APP_SRC_FILES})

target_include_directories(cfs_core_app PUBLIC fsw/inc)

if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)


project(CFE_TELEMETRY_APP C)

set(APP_SRC_FILES
  fsw/src/telemetry_app.c
  fsw/src/telemetry_app_cmds.c
  fsw/src/telemetry_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/telemetry_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/telemetry_app_dispatch.c
  )
endif()

# Create the app module
add_cfe_app(telemetry_app ${APP_SRC_FILES})

target_include_directories(telemetry_app PUBLIC fsw/inc)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)

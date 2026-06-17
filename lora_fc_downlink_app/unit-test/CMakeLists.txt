##################################################################
#
# Coverage Unit Test build recipe
#
##################################################################

include_directories(../fsw/src)
include_directories(../fsw/inc)
include_directories(../config)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

# NOTE: coverage module name must match add_cfe_app target (lora_fc_dl_app).
add_cfe_coverage_stubs(lora_fc_dl_app
  stubs/lora_fc_downlink_app_global_stubs.c
  stubs/lora_fc_downlink_app_stubs.c
  stubs/lora_fc_downlink_app_cmds_stubs.c
  stubs/lora_fc_downlink_app_dispatch_stubs.c
  stubs/lora_fc_downlink_app_utils_stubs.c
)

add_library(lora_fc_dl_app_ut_common STATIC
    common/eventcheck.c
    common/setup.c
)

target_include_directories(lora_fc_dl_app_ut_common PUBLIC common)
target_link_libraries(lora_fc_dl_app_ut_common core_api ut_assert)

foreach(SRCFILE ${APP_SRC_FILES})
    get_filename_component(UNIT_NAME "${SRCFILE}" NAME_WE)
    set(TESTS_NAME "coveragetest_${UNIT_NAME}")
    set(TESTS_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/coveragetest/${TESTS_NAME}.c")

    add_cfe_coverage_test(lora_fc_dl_app "${UNIT_NAME}" "${TESTS_SOURCE_FILE}" "../${SRCFILE}")
    add_cfe_coverage_dependency(lora_fc_dl_app "${UNIT_NAME}" lora_fc_dl_app)
    target_link_libraries(coverage-lora_fc_dl_app-${UNIT_NAME}-testrunner
        coverage-lora_fc_dl_app-stubs
        lora_fc_dl_app_ut_common)
endforeach()

##################################################################
#
# Coverage Unit Test build recipe
#
##################################################################

include_directories(../fsw/src)
include_directories(../fsw/inc)
include_directories(../config)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../shared_msgs)

add_cfe_coverage_stubs(lora_tdm_app
  stubs/lora_tdm_app_global_stubs.c
  stubs/lora_tdm_app_stubs.c
  stubs/lora_tdm_app_cmds_stubs.c
  stubs/lora_tdm_app_dispatch_stubs.c
  stubs/lora_tdm_app_utils_stubs.c
)

add_library(lora_tdm_app_ut_common STATIC
    common/eventcheck.c
    common/setup.c
)

target_include_directories(lora_tdm_app_ut_common PUBLIC common)
target_link_libraries(lora_tdm_app_ut_common core_api ut_assert)

foreach(SRCFILE ${APP_SRC_FILES})
    get_filename_component(UNIT_NAME "${SRCFILE}" NAME_WE)
    set(TESTS_NAME "coveragetest_${UNIT_NAME}")
    set(TESTS_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/coveragetest/${TESTS_NAME}.c")

    add_cfe_coverage_test(lora_tdm_app "${UNIT_NAME}" "${TESTS_SOURCE_FILE}" "../${SRCFILE}")
    add_cfe_coverage_dependency(lora_tdm_app "${UNIT_NAME}" lora_tdm_app)
    target_link_libraries(coverage-lora_tdm_app-${UNIT_NAME}-testrunner
        coverage-lora_tdm_app-stubs
        lora_tdm_app_ut_common)
endforeach()

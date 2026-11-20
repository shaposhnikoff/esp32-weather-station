set(COMMON -fsyntax-only -x c -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE
    "-I${PROJECT_ROOT}/include"
    "-I${PROJECT_ROOT}/components/weather_ui/include"
    "-I${PROJECT_ROOT}/components/weather_data/include"
    "-I${LVGL_SOURCE}")
if(SYSROOT)
    list(APPEND COMMON -isysroot "${SYSROOT}")
endif()
execute_process(COMMAND "${COMPILER}" ${COMMON} "${PROJECT_ROOT}/tests/lvgl_config_guard.c"
    RESULT_VARIABLE valid ERROR_VARIABLE valid_error)
if(NOT valid EQUAL 0)
    message(FATAL_ERROR "Shared LVGL configuration rejected: ${valid_error}")
endif()
execute_process(COMMAND "${COMPILER}" ${COMMON} -DLV_CONF_SKIP
    "${PROJECT_ROOT}/tests/lvgl_config_guard.c"
    RESULT_VARIABLE invalid ERROR_VARIABLE invalid_error)
if(invalid EQUAL 0)
    message(FATAL_ERROR "Misconfigured LVGL unexpectedly compiled")
endif()
string(FIND "${invalid_error}" "Weather UI requires the shared lv_conf.h" reason)
if(reason EQUAL -1)
    message(FATAL_ERROR "Configuration failed for an unrelated reason: ${invalid_error}")
endif()
message(STATUS "Shared configuration accepted; skipped-header configuration rejected")

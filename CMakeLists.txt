cmake_minimum_required(VERSION 3.16)
option(WEATHER_DESKTOP "Build the shared LVGL UI on SDL2" OFF)
option(WEATHER_PREVIEW_NATIVE "Preview the attached 1024x600 panel" OFF)
if(WEATHER_DESKTOP)
    project(weather_station_desktop C CXX)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)
    include(FetchContent)
    set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h" CACHE STRING "")
    set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "")
    set(LV_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(LV_BUILD_DEMOS OFF CACHE BOOL "")
    FetchContent_Declare(lvgl
        URL https://github.com/lvgl/lvgl/archive/refs/tags/v8.4.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(lvgl)
    find_package(SDL2 REQUIRED CONFIG)
    find_package(CURL REQUIRED)
    set(ENABLE_CJSON_TEST OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    FetchContent_Declare(cjson
        URL https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.18.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(cjson)
    target_include_directories(cjson PUBLIC "$<BUILD_INTERFACE:${cjson_SOURCE_DIR}>")
    add_library(weather_data
        components/weather_data/weather_parse.c
        components/weather_data/weather_time.c
        components/weather_data/weather_cache.c
        components/weather_data/weather_policy.c)
    target_include_directories(weather_data PUBLIC components/weather_data/include)
    target_link_libraries(weather_data PUBLIC cjson m)
    target_compile_options(weather_data PRIVATE -Wall -Wextra -Werror)
    add_library(weather_config components/weather_app/weather_config.c
        components/weather_app/weather_config_lex.c)
    target_include_directories(weather_config PUBLIC components/weather_app/include)
    target_link_libraries(weather_config PUBLIC weather_data)
    target_compile_options(weather_config PRIVATE -Wall -Wextra -Werror)
    add_executable(weather-config-tests tests/test_config.c)
    target_link_libraries(weather-config-tests PRIVATE weather_config)
    target_compile_options(weather-config-tests PRIVATE -Wall -Wextra -Werror)
    add_executable(weather-config-check tools/check-config.c)
    target_link_libraries(weather-config-check PRIVATE weather_config)
    target_compile_options(weather-config-check PRIVATE -Wall -Wextra -Werror)
    file(GLOB WEATHER_ASSETS CONFIGURE_DEPENDS assets/generated/*.c)
    add_library(weather_ui components/weather_ui/ui_base.c
        components/weather_ui/ui_main.c components/weather_ui/ui_update.c
        components/weather_ui/ui_icons.c components/weather_ui/ui_details.c
        components/weather_ui/ui_strings.c ${WEATHER_ASSETS})
    target_include_directories(weather_ui PUBLIC components/weather_ui/include include)
    target_link_libraries(weather_ui PUBLIC lvgl::lvgl weather_data)
    target_compile_options(weather_ui PRIVATE -Wall -Wextra -Werror)
    add_executable(weather-preview desktop/main.c desktop/display.c desktop/network.c)
    target_link_libraries(weather-preview PRIVATE weather_ui weather_config SDL2::SDL2 CURL::libcurl)
    target_compile_definitions(weather-preview PRIVATE
        WEATHER_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(WEATHER_PREVIEW_NATIVE)
        target_compile_definitions(weather-preview PRIVATE WEATHER_BOARD_LCD5B)
    endif()
    target_compile_options(weather-preview PRIVATE -Wall -Wextra -Werror)
    enable_testing()
    add_executable(weather-network-tests tests/test_network.c)
    target_include_directories(weather-network-tests PRIVATE desktop)
    target_link_libraries(weather-network-tests PRIVATE weather_ui weather_config SDL2::SDL2 CURL::libcurl)
    target_compile_options(weather-network-tests PRIVATE -Wall -Wextra -Werror)
    foreach(scenario cycle cached first-offline location boundaries)
        add_test(NAME weather-network-${scenario} COMMAND weather-network-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/config.example.json" "${scenario}")
        set_tests_properties(weather-network-${scenario} PROPERTIES TIMEOUT 15)
    endforeach()
    add_test(NAME weather-config COMMAND weather-config-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/config.example.json")
    add_executable(weather-data-tests tests/test_weather.c)
    target_link_libraries(weather-data-tests PRIVATE weather_data)
    target_compile_options(weather-data-tests PRIVATE -Wall -Wextra -Werror)
    foreach(group fixture incomplete malformed time policy cache)
        add_test(NAME weather-${group} COMMAND weather-data-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.json" "${group}")
    endforeach()
    add_executable(weather-ui-tests tests/test_ui.c desktop/display.c)
    target_include_directories(weather-ui-tests PRIVATE desktop components/weather_ui)
    target_link_libraries(weather-ui-tests PRIVATE weather_ui SDL2::SDL2)
    target_compile_options(weather-ui-tests PRIVATE -Wall -Wextra -Werror)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/screenshots")
    add_test(NAME weather-ui COMMAND weather-ui-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.json" "${CMAKE_BINARY_DIR}/screenshots")
    set_tests_properties(weather-ui PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 30)
    add_executable(weather-ui-native-tests tests/test_ui.c desktop/display.c)
    target_include_directories(weather-ui-native-tests PRIVATE desktop components/weather_ui)
    target_link_libraries(weather-ui-native-tests PRIVATE weather_ui SDL2::SDL2)
    target_compile_definitions(weather-ui-native-tests PRIVATE WEATHER_BOARD_LCD5B)
    target_compile_options(weather-ui-native-tests PRIVATE -Wall -Wextra -Werror)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/screenshots-native")
    add_test(NAME weather-ui-native COMMAND weather-ui-native-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.json" "${CMAKE_BINARY_DIR}/screenshots-native")
    set_tests_properties(weather-ui-native PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 30)
    add_test(NAME weather-lvgl-config COMMAND "${CMAKE_COMMAND}"
        "-DCOMPILER=${CMAKE_C_COMPILER}" "-DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DLVGL_SOURCE=${lvgl_SOURCE_DIR}"
        "-DSYSROOT=${CMAKE_OSX_SYSROOT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_lvgl_configuration.cmake")
else()
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/main")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(weather_station)
endif()

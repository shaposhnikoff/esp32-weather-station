if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(DEMO_PATH "${CMAKE_CURRENT_LIST_DIR}/../../fixtures/demo.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DEMO_PATH}")
    file(READ "${DEMO_PATH}" DEMO_JSON)
    string(REPLACE "\\" "\\\\" DEMO_JSON "${DEMO_JSON}")
    string(REPLACE "\"" "\\\"" DEMO_JSON "${DEMO_JSON}")
    string(REPLACE "\n" "\\n" DEMO_JSON "${DEMO_JSON}")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/demo_fixture.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/demo_fixture.h" @ONLY)
endif()
idf_component_register(SRCS "app_main.c" "board.c" "weather_network.c"
    INCLUDE_DIRS "." "../../include" "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES weather_ui weather_data weather_app esp_lcd driver esp_timer esp_psram
        esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls littlefs)

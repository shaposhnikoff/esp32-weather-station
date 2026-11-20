file(GLOB ASSETS "../../assets/generated/*.c")
idf_component_register(SRCS "ui_base.c" "ui_main.c" "ui_update.c" "ui_icons.c"
    "ui_details.c" "ui_strings.c" ${ASSETS}
    INCLUDE_DIRS "include" REQUIRES lvgl weather_data)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

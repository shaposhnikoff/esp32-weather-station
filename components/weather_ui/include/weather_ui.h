#pragma once
#include "lvgl.h"
#include "weather_data.h"

#if LV_COLOR_DEPTH != 16 || !LV_MEM_CUSTOM || !LV_SPRINTF_USE_FLOAT
#error "Weather UI requires the shared lv_conf.h: RGB565, custom allocator and float formatting"
#endif

void weather_ui_diagnostics(void);

typedef struct {
    bool large_text;
    unsigned brightness;
    void (*refresh)(void *context);
    void *context;
} weather_ui_options_t;

typedef enum {
    WEATHER_UI_LOADING, WEATHER_UI_READY, WEATHER_UI_UPDATING, WEATHER_UI_OFFLINE,
    WEATHER_UI_STALE, WEATHER_UI_ERROR, WEATHER_UI_CONFIG_ERROR
} weather_ui_status_t;

typedef struct {
    weather_ui_status_t status;
    time_t now;
    unsigned stale_after_seconds;
    char message[160];
} weather_ui_state_t;

void weather_ui_create(const weather_ui_options_t *options);
void weather_ui_apply(const weather_snapshot_t *snapshot, const weather_ui_state_t *state);
void weather_ui_close_details(void);

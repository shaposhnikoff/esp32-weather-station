#pragma once
#include "weather_ui.h"
#include "ui_strings.h"

LV_FONT_DECLARE(weather_font_14);
LV_FONT_DECLARE(weather_font_16);
LV_FONT_DECLARE(weather_font_20);
LV_FONT_DECLARE(weather_font_28);
LV_FONT_DECLARE(weather_font_80);
LV_IMG_DECLARE(landscape_day);
LV_IMG_DECLARE(landscape_night);

#define UI_BG 0x0b1120
#define UI_TEXT 0xf5f7fc
#define UI_SECONDARY 0x8799be
#define UI_DIVIDER 0x283149
#define UI_RAIN 0x5daaff
#define UI_SUN 0xffc94a

typedef struct {
    lv_obj_t *box, *time, *icon, *temperature;
    time_t instant;
} ui_hour_slot_t;

typedef struct {
    lv_obj_t *box, *day, *icon, *precipitation, *minimum, *maximum;
    time_t instant;
} ui_day_slot_t;

typedef struct {
    weather_snapshot_t snapshot;
    weather_ui_state_t state;
    weather_ui_options_t options;
    bool valid;
    lv_obj_t *image, *location, *current_icon, *temperature, *description, *feels;
    lv_obj_t *humidity, *pressure, *freshness, *refresh, *refresh_label;
    lv_obj_t *sunrise, *sunset, *moon, *moon_icon, *notice, *notice_text;
    lv_obj_t *details, *details_title, *details_body;
    ui_hour_slot_t hours[7];
    ui_day_slot_t days[5];
    unsigned detail_kind;
    time_t detail_time;
} ui_context_t;

extern ui_context_t ui;
extern const char *const ui_conditions[8];
extern const char *const ui_weekdays[7];
extern const char *const ui_moon_phases[8];
extern const char ui_about_data[];

const lv_font_t *ui_body_font(void);
lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int width, int height);
lv_obj_t *ui_label(lv_obj_t *parent, int x, int y, int width, const lv_font_t *font);
void ui_value(lv_obj_t *label, float value, const char *suffix);
lv_obj_t *ui_icon_create(lv_obj_t *parent, int x, int y, int size);
void ui_icon_set(lv_obj_t *object, weather_condition_t condition, int daylight);
void ui_icon_moon(lv_obj_t *object, double phase);
void ui_icon_solar(lv_obj_t *object, bool rising);
void ui_open_hour(lv_event_t *event);
void ui_open_day(lv_event_t *event);
void ui_open_about(lv_event_t *event);
void ui_details_update(void);

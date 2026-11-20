#include "ui_internal.h"
#include <math.h>
#include <stdio.h>

void weather_ui_close_details(void)
{
    ui.detail_kind = 0;
    lv_obj_add_flag(ui.details, LV_OBJ_FLAG_HIDDEN);
}

void ui_open_about(lv_event_t *event)
{
    (void)event;
    ui.detail_kind = 3;
    lv_obj_clear_flag(ui.details, LV_OBJ_FLAG_HIDDEN);
    ui_details_update();
}

void ui_open_hour(lv_event_t *event)
{
    const ui_hour_slot_t *slot = lv_event_get_user_data(event);
    if (!slot->instant) return;
    ui.detail_kind = 1;
    ui.detail_time = slot->instant;
    lv_obj_clear_flag(ui.details, LV_OBJ_FLAG_HIDDEN);
    ui_details_update();
}

void ui_open_day(lv_event_t *event)
{
    const ui_day_slot_t *slot = lv_event_get_user_data(event);
    if (!slot->instant) return;
    ui.detail_kind = 2;
    ui.detail_time = slot->instant;
    lv_obj_clear_flag(ui.details, LV_OBJ_FLAG_HIDDEN);
    ui_details_update();
}

static void measure(char result[32], float value, const char *unit)
{
    if (isfinite(value)) snprintf(result, 32, "%.1f %s", (double)value, unit);
    else snprintf(result, 32, UI_NONE);
}

void ui_details_update(void)
{
    if (!ui.detail_kind) return;
    if (ui.detail_kind == 3) {
        lv_label_set_text(ui.details_title, UI_ABOUT);
        lv_label_set_text(ui.details_body, ui_about_data);
        return;
    }
    struct tm date;
    localtime_r(&ui.detail_time, &date);
    char title[40];
    strftime(title, sizeof(title), ui.detail_kind == 1 ? "%d.%m · %H:%M" : "%d.%m.%Y", &date);
    lv_label_set_text_fmt(ui.details_title, "%s%s", ui.snapshot.demo ? UI_DEMO_PREFIX : "", title);
    lv_label_set_text(ui.details_body, UI_UNAVAILABLE_NOTE);
    if (!ui.valid) return;
    char a[32], b[32], c[32];
    if (ui.detail_kind == 1) {
        for (size_t i = 0; i < ui.snapshot.hour_count; ++i) {
            const weather_hour_t *hour = &ui.snapshot.hours[i];
            if (hour->time != ui.detail_time || hour->time <= ui.state.now) continue;
            measure(a, hour->temperature, UI_CELSIUS);
            measure(b, hour->precipitation, "%");
            measure(c, hour->wind, UI_WIND_UNIT);
            lv_label_set_text_fmt(ui.details_body,
                UI_HOUR_DETAILS,
                ui_conditions[hour->condition], a, b, c);
            return;
        }
    } else {
        for (size_t i = 0; i < ui.snapshot.day_count; ++i) {
            const weather_day_t *day = &ui.snapshot.days[i];
            if (day->time != ui.detail_time) continue;
            measure(a, day->minimum, UI_CELSIUS);
            measure(b, day->maximum, UI_CELSIUS);
            measure(c, day->precipitation, "%");
            char rise[6], set[6];
            weather_clock(day->sunrise, rise);
            weather_clock(day->sunset, set);
            lv_label_set_text_fmt(ui.details_body,
                UI_DAY_DETAILS,
                ui_conditions[day->condition], a, b, c,
                day->sunrise ? rise : UI_NONE, day->sunset ? set : UI_NONE);
            return;
        }
    }
}

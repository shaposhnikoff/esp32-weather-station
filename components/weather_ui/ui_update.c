#include "ui_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void current_weather(void)
{
    if (!ui.valid) {
        lv_label_set_text(ui.location, UI_TITLE);
        lv_label_set_text(ui.temperature, UI_NONE);
        lv_label_set_text(ui.description, UI_NO_DATA);
        lv_label_set_text(ui.feels, UI_FEELS_UNKNOWN);
        lv_label_set_text(ui.humidity, UI_NONE);
        lv_label_set_text(ui.pressure, UI_NONE);
        ui_icon_set(ui.current_icon, WEATHER_UNKNOWN, -1);
        return;
    }
    const weather_current_t *current = &ui.snapshot.current;
    lv_label_set_text(ui.location, ui.snapshot.location.name);
    ui_value(ui.temperature, current->temperature, "");
    lv_label_set_text(ui.description, ui_conditions[current->condition]);
    if (isfinite(current->feels_like))
        lv_label_set_text_fmt(ui.feels, UI_FEELS_FORMAT, (double)current->feels_like);
    else lv_label_set_text(ui.feels, UI_FEELS_UNKNOWN);
    ui_value(ui.humidity, current->humidity, "%");
    ui_value(ui.pressure, current->pressure, UI_PRESSURE_UNIT);
    ui_icon_set(ui.current_icon, current->condition, current->is_day);
    const lv_img_dsc_t *image = current->is_day == 0 ? &landscape_night : &landscape_day;
    if (lv_img_get_src(ui.image) != image) lv_img_set_src(ui.image, image);
    lv_obj_set_style_img_opa(ui.image, current->is_day < 0 ? LV_OPA_30 : LV_OPA_COVER, 0);
}

static void hourly_weather(void)
{
    size_t indices[6];
    const bool clock_valid = ui.state.now >= 946684800;
    const size_t count = ui.valid && clock_valid ?
        weather_future_hours(&ui.snapshot, ui.state.now, indices) : 0;
    for (size_t i = 0; i < 7; ++i) {
        ui_hour_slot_t *slot = &ui.hours[i];
        slot->instant = 0;
        lv_label_set_text(slot->time, i == 0 ? UI_NOW : UI_NONE);
        lv_label_set_text(slot->temperature, UI_NONE);
        ui_icon_set(slot->icon, WEATHER_UNKNOWN, -1);
        if (!ui.valid) continue;
        if (i == 0) {
            ui_value(slot->temperature, ui.snapshot.current.temperature, "°");
            ui_icon_set(slot->icon, ui.snapshot.current.condition, ui.snapshot.current.is_day);
        } else if (i <= count) {
            const weather_hour_t *hour = &ui.snapshot.hours[indices[i - 1]];
            char clock[6];
            weather_clock(hour->time, clock);
            lv_label_set_text(slot->time, clock);
            ui_value(slot->temperature, hour->temperature, "°");
            ui_icon_set(slot->icon, hour->condition, hour->is_day);
            slot->instant = hour->time;
        }
    }
}

static void daily_weather(void)
{
    size_t indices[5];
    const size_t count = ui.valid && ui.state.now >= 946684800 ?
        weather_visible_days(&ui.snapshot, ui.state.now, indices) : 0;
    const weather_day_t *today = ui.valid ? weather_today(&ui.snapshot, ui.state.now) : NULL;
    for (size_t i = 0; i < 5; ++i) {
        ui_day_slot_t *slot = &ui.days[i];
        slot->instant = 0;
        lv_label_set_text(slot->day, UI_NONE);
        lv_label_set_text(slot->precipitation, UI_NONE);
        lv_label_set_text(slot->minimum, UI_NONE);
        lv_label_set_text(slot->maximum, UI_NONE);
        ui_icon_set(slot->icon, WEATHER_UNKNOWN, -1);
        if (i >= count) continue;
        const weather_day_t *day = &ui.snapshot.days[indices[i]];
        slot->instant = day->time;
        struct tm date;
        localtime_r(&day->time, &date);
        if (day == today) lv_label_set_text(slot->day, UI_TODAY);
        else lv_label_set_text_fmt(slot->day, "%s %02d", ui_weekdays[date.tm_wday], date.tm_mday);
        ui_value(slot->precipitation, day->precipitation, "%");
        ui_value(slot->minimum, day->minimum, "°");
        ui_value(slot->maximum, day->maximum, "°");
        ui_icon_set(slot->icon, day->condition, 1);
    }
    char rise[6], set[6];
    weather_clock(today ? today->sunrise : 0, rise);
    weather_clock(today ? today->sunset : 0, set);
    lv_label_set_text(ui.sunrise, today && today->sunrise ? rise : UI_NONE);
    lv_label_set_text(ui.sunset, today && today->sunset ? set : UI_NONE);
    const double phase = ui.state.now >= 946684800 && ui.state.status != WEATHER_UI_CONFIG_ERROR ?
        weather_moon_phase(ui.state.now) : NAN;
    if (isfinite(phase)) {
        const unsigned index = (unsigned)floor(phase * 8 + .5) % 8;
        lv_label_set_text(ui.moon, ui_moon_phases[index]);
        ui_icon_moon(ui.moon_icon, phase);
    } else {
        lv_label_set_text(ui.moon, UI_NONE);
        ui_icon_set(ui.moon_icon, WEATHER_UNKNOWN, -1);
    }
}

static void status_text(void)
{
    const bool busy = ui.state.status == WEATHER_UI_LOADING || ui.state.status == WEATHER_UI_UPDATING;
    lv_label_set_text(ui.refresh_label, busy ? UI_LOADING : UI_REFRESH);
    if (busy || ui.state.status == WEATHER_UI_CONFIG_ERROR)
        lv_obj_add_state(ui.refresh, LV_STATE_DISABLED);
    else lv_obj_clear_state(ui.refresh, LV_STATE_DISABLED);
    const bool stale = ui.valid && weather_is_stale(&ui.snapshot, ui.state.now,
        ui.state.stale_after_seconds ? ui.state.stale_after_seconds : 21600);
    const char *status = "";
    switch (ui.state.status) {
    case WEATHER_UI_LOADING: status = UI_CONNECTING; break;
    case WEATHER_UI_READY: status = stale ? UI_STALE : UI_UPDATED; break;
    case WEATHER_UI_UPDATING: status = UI_UPDATING; break;
    case WEATHER_UI_OFFLINE: status = stale ? UI_OFFLINE_STALE : UI_OFFLINE; break;
    case WEATHER_UI_STALE: status = UI_STALE; break;
    case WEATHER_UI_ERROR: status = UI_REQUEST_ERROR; break;
    case WEATHER_UI_CONFIG_ERROR: status = UI_CONFIG_ERROR; break;
    }
    char loaded[32] = "–";
    if (ui.valid) {
        struct tm local;
        localtime_r(&ui.snapshot.fetched_at, &local);
        strftime(loaded, sizeof(loaded), "%d.%m %H:%M", &local);
    }
    lv_label_set_text_fmt(ui.freshness, "%s%s\n%s",
        ui.valid && ui.snapshot.demo ? UI_DEMO_PREFIX : "", status, loaded);
    if (ui.valid) {
        lv_obj_add_flag(ui.notice, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui.notice, LV_OBJ_FLAG_HIDDEN);
        if (ui.state.status == WEATHER_UI_CONFIG_ERROR) {
            lv_label_set_text_fmt(ui.notice_text, UI_CONFIG_NOTE,
                ui.state.message[0] ? ui.state.message : "file");
        } else {
            const char *message = ui.state.message[0] ? ui.state.message :
                busy ? UI_LOADING_NOTE : UI_ERROR_NOTE;
            lv_label_set_text(ui.notice_text, message);
        }
    }
}

void weather_ui_apply(const weather_snapshot_t *snapshot, const weather_ui_state_t *state)
{
    if (snapshot) { ui.snapshot = *snapshot; ui.valid = true; }
    ui.state = *state;
    current_weather();
    hourly_weather();
    daily_weather();
    status_text();
    ui_details_update();
}

#include "ui_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

ui_context_t ui;

const lv_font_t *ui_body_font(void)
{
    return ui.options.large_text ? &weather_font_20 : &weather_font_16;
}

lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(object, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

lv_obj_t *ui_label(lv_obj_t *parent, int x, int y, int width, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_TEXT), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, UI_NONE);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

void ui_value(lv_obj_t *label, float value, const char *suffix)
{
    if (isfinite(value)) lv_label_set_text_fmt(label, "%.0f%s", (double)value, suffix);
    else lv_label_set_text(label, UI_NONE);
}

static void refresh_event(lv_event_t *event)
{
    (void)event;
    if (ui.options.refresh && ui.state.status != WEATHER_UI_UPDATING)
        ui.options.refresh(ui.options.context);
}

static void close_event(lv_event_t *event)
{
    (void)event;
    weather_ui_close_details();
}

static lv_obj_t *header_button(lv_obj_t *parent, int x, const char *text, lv_event_cb_t callback)
{
    lv_obj_t *button = ui_box(parent, x, 8, 140, 44);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x393e4c), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x48516a), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303441), LV_STATE_DISABLED);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_t *label = ui_label(button, 0, 11, 140, &weather_font_16);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, text);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    return button;
}

static void build_left(lv_obj_t *screen)
{
    lv_obj_t *left = ui_box(screen, 0, 60, 260, 420);
    ui.image = lv_img_create(left);
    lv_img_set_src(ui.image, &landscape_day);
    lv_obj_set_size(ui.image, 260, 420);
    lv_obj_t *shade = ui_box(left, 0, 0, 260, 420);
    lv_obj_set_style_bg_color(shade, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_30, 0);
    lv_obj_t *bottom = ui_box(left, 0, 226, 260, 194);
    lv_obj_set_style_bg_color(bottom, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_60, 0);
    ui.location = ui_label(left, 14, 12, 232, &weather_font_20);
    lv_obj_set_style_bg_color(ui.location, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui.location, LV_OPA_30, 0);
    lv_obj_set_height(ui.location, 50);
    lv_obj_set_style_text_align(ui.location, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.location, LV_LABEL_LONG_DOT);
    ui.current_icon = ui_icon_create(left, 100, 62, 60);
    ui.temperature = ui_label(left, 4, 122, 212, &weather_font_80);
    lv_obj_set_style_text_align(ui.temperature, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *unit = ui_label(left, 211, 143, 48, &weather_font_28);
    lv_label_set_text(unit, UI_CELSIUS);
    ui.description = ui_label(left, 14, 236, 232, &weather_font_20);
    lv_obj_set_style_text_align(ui.description, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_height(ui.description, 52);
    ui.feels = ui_label(left, 12, 295, 236, &weather_font_14);
    lv_obj_set_style_text_align(ui.feels, LV_TEXT_ALIGN_CENTER, 0);
    ui.humidity = ui_label(left, 12, 333, 100, &weather_font_20);
    ui.pressure = ui_label(left, 116, 333, 138, &weather_font_20);
    lv_obj_set_style_text_align(ui.humidity, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(ui.pressure, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *humidity = ui_label(left, 12, 360, 108, &weather_font_14);
    lv_obj_t *pressure = ui_label(left, 128, 360, 116, &weather_font_14);
    lv_label_set_text(humidity, UI_HUMIDITY);
    lv_label_set_text(pressure, UI_PRESSURE);
    lv_obj_set_style_text_align(humidity, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(pressure, LV_TEXT_ALIGN_CENTER, 0);
    ui.freshness = ui_label(left, 8, 382, 244, &weather_font_14);
    lv_obj_set_height(ui.freshness, 38);
    lv_obj_set_style_text_align(ui.freshness, LV_TEXT_ALIGN_CENTER, 0);
}

static void build_forecast(lv_obj_t *screen)
{
    for (int i = 0; i < 7; ++i) {
        ui_hour_slot_t *slot = &ui.hours[i];
        const int x = 260 + i * 540 / 7, width = (i + 1) * 540 / 7 - i * 540 / 7;
        slot->box = ui_box(screen, x, 60, width, 150);
        lv_obj_set_style_border_color(slot->box, lv_color_hex(UI_DIVIDER), 0);
        lv_obj_set_style_border_width(slot->box, 1, 0);
        lv_obj_set_style_border_side(slot->box, LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_bg_color(slot->box, lv_color_hex(0x1a253b), LV_STATE_PRESSED);
        slot->time = ui_label(slot->box, 3, 18, width - 6, &weather_font_14);
        slot->icon = ui_icon_create(slot->box, (width - 42) / 2, 48, 42);
        slot->temperature = ui_label(slot->box, 2, 106, width - 4, &weather_font_20);
        lv_obj_set_style_text_color(slot->time, lv_color_hex(UI_SECONDARY), 0);
        lv_obj_set_style_text_align(slot->time, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_align(slot->temperature, LV_TEXT_ALIGN_CENTER, 0);
        if (i > 0) {
            lv_obj_add_flag(slot->box, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(slot->box, ui_open_hour, LV_EVENT_CLICKED, slot);
        }
    }
    for (int i = 0; i < 5; ++i) {
        ui_day_slot_t *slot = &ui.days[i];
        slot->box = ui_box(screen, 260, 210 + i * 54, 385, 54);
        lv_obj_add_flag(slot->box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_color(slot->box, lv_color_hex(UI_DIVIDER), 0);
        lv_obj_set_style_border_width(slot->box, 1, 0);
        lv_obj_set_style_border_side(slot->box, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_bg_color(slot->box, lv_color_hex(0x1a253b), LV_STATE_PRESSED);
        slot->day = ui_label(slot->box, 12, 16, 98, ui_body_font());
        slot->icon = ui_icon_create(slot->box, 112, 9, 34);
        slot->precipitation = ui_label(slot->box, 175, 16, 63, &weather_font_16);
        slot->minimum = ui_label(slot->box, 247, 16, 62, &weather_font_20);
        slot->maximum = ui_label(slot->box, 315, 16, 62, &weather_font_20);
        lv_obj_set_style_text_color(slot->precipitation, lv_color_hex(UI_RAIN), 0);
        lv_obj_set_style_text_color(slot->minimum, lv_color_hex(UI_SECONDARY), 0);
        lv_obj_set_style_text_align(slot->minimum, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_align(slot->maximum, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_event_cb(slot->box, ui_open_day, LV_EVENT_CLICKED, slot);
    }
    lv_obj_t *side = ui_box(screen, 645, 210, 155, 270);
    lv_obj_set_style_border_color(side, lv_color_hex(UI_DIVIDER), 0);
    lv_obj_set_style_border_width(side, 1, 0);
    lv_obj_set_style_border_side(side, LV_BORDER_SIDE_LEFT, 0);
    const char *titles[] = {UI_SUNRISE, UI_SUNSET, UI_MOON};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *zone = ui_box(side, 0, i * 90, 155, 90);
        lv_obj_set_style_border_color(zone, lv_color_hex(UI_DIVIDER), 0);
        lv_obj_set_style_border_width(zone, 1, 0);
        lv_obj_set_style_border_side(zone, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_t *title = ui_label(zone, 4, 5, 147, &weather_font_14);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(UI_SECONDARY), 0);
        lv_label_set_text(title, titles[i]);
        lv_obj_t *icon = ui_icon_create(zone, 62, 25, 30);
        if (i < 2) ui_icon_solar(icon, i == 0);
        else ui.moon_icon = icon;
        lv_obj_t *value = ui_label(zone, 3, 57, 149, i < 2 ? &weather_font_20 : &weather_font_14);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(i < 2 ? UI_SUN : UI_TEXT), 0);
        if (i == 0) ui.sunrise = value;
        if (i == 1) ui.sunset = value;
        if (i == 2) { ui.moon = value; lv_obj_set_y(value, 49); }
    }
}

void weather_ui_create(const weather_ui_options_t *options)
{
    memset(&ui, 0, sizeof(ui));
    ui.options = *options;
    lv_obj_t *physical = lv_scr_act();
    lv_obj_clean(physical);
    lv_obj_set_style_bg_color(physical, lv_color_hex(UI_BG), 0);
    lv_obj_clear_flag(physical, LV_OBJ_FLAG_SCROLLABLE);
    const int width = lv_disp_get_hor_res(NULL), height = lv_disp_get_ver_res(NULL);
    const int zoom = LV_MIN(width * 256 / 800, height * 256 / 480);
    lv_obj_t *screen = ui_box(physical, (width - 800 * zoom / 256) / 2,
        (height - 480 * zoom / 256) / 2, 800, 480);
    lv_obj_set_style_transform_pivot_x(screen, 0, 0);
    lv_obj_set_style_transform_pivot_y(screen, 0, 0);
    lv_obj_set_style_transform_zoom(screen, zoom, 0);
    lv_obj_t *header = ui_box(screen, 0, 0, 800, 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x282b36), 0);
    header_button(header, 8, UI_ABOUT, ui_open_about);
    lv_obj_t *title = ui_label(header, 180, 17, 440, &weather_font_20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(title, UI_TITLE);
    ui.refresh = header_button(header, 652, UI_REFRESH, refresh_event);
    ui.refresh_label = lv_obj_get_child(ui.refresh, 0);
    build_left(screen);
    build_forecast(screen);
    ui.notice = ui_box(screen, 280, 235, 345, 220);
    lv_obj_set_style_bg_color(ui.notice, lv_color_hex(0x162036), 0);
    lv_obj_set_style_radius(ui.notice, 8, 0);
    ui.notice_text = ui_label(ui.notice, 18, 28, 309, ui_body_font());
    ui.details = ui_box(screen, 50, 78, 700, 380);
    lv_obj_set_style_bg_color(ui.details, lv_color_hex(0x182238), 0);
    lv_obj_set_style_radius(ui.details, 10, 0);
    ui.details_title = ui_label(ui.details, 24, 20, 500, &weather_font_28);
    lv_obj_t *detail_content = ui_box(ui.details, 24, 83, 652, 276);
    lv_obj_add_flag(detail_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(detail_content, LV_DIR_VER);
    lv_obj_set_style_bg_color(detail_content, lv_color_hex(0x182238), 0);
    ui.details_body = ui_label(detail_content, 0, 0, 640, ui_body_font());
    header_button(ui.details, 552, UI_BACK, close_event);
    lv_obj_add_flag(ui.details, LV_OBJ_FLAG_HIDDEN);
    const weather_ui_state_t initial = {.status = WEATHER_UI_LOADING};
    weather_ui_apply(NULL, &initial);
    if (options->brightness < 100) {
        lv_obj_t *dimmer = ui_box(lv_layer_top(), 0, 0, width, height);
        lv_obj_set_style_bg_color(dimmer, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(dimmer, (lv_opa_t)((100 - options->brightness) * 255 / 100), 0);
    }
}

#include "display.h"
#include "ui_internal.h"
#include "board_profile.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static unsigned requests;
static const char *screenshots;
static weather_ui_state_t state;

static void advance(void)
{
    for (int i = 0; i < 8; ++i) CHECK(desktop_display_step(10));
}

static void capture(const char *name)
{
    advance();
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.bmp", screenshots, name);
    CHECK(desktop_display_save(path));
}

static void click(int x, int y)
{
    const int zoom = LV_MIN(BOARD_WIDTH * 256 / 800, BOARD_HEIGHT * 256 / 480);
    x = (BOARD_WIDTH - 800 * zoom / 256) / 2 + x * zoom / 256;
    y = (BOARD_HEIGHT - 480 * zoom / 256) / 2 + y * zoom / 256;
    SDL_Event event = {.type = SDL_MOUSEBUTTONDOWN};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    CHECK(SDL_PushEvent(&event) == 1);
    advance();
    event.type = SDL_MOUSEBUTTONUP;
    CHECK(SDL_PushEvent(&event) == 1);
    advance();
}

static void refresh(void *context)
{
    (void)context;
    ++requests;
    state.status = WEATHER_UI_UPDATING;
    weather_ui_apply(NULL, &state);
}

static void create(bool large)
{
    const weather_ui_options_t options = {
        .large_text = large, .brightness = 100, .refresh = refresh,
    };
    weather_ui_create(&options);
    state = (weather_ui_state_t){.status = WEATHER_UI_READY, .now = 1784569500};
}

static void navigation_and_snapshot_ownership(weather_snapshot_t model)
{
    create(false);
    weather_ui_apply(&model, &state);
    model.current.temperature = 99;
    CHECK(strcmp(lv_label_get_text(ui.temperature), "23") == 0);
    capture("day");
    click(375, 130);
    CHECK(!lv_obj_has_flag(ui.details, LV_OBJ_FLAG_HIDDEN));
    CHECK(strstr(lv_label_get_text(ui.details_body), "21.0") != NULL);
    capture("hour");
    model.current.temperature = 23;
    model.hours[1].temperature = 22;
    state.status = WEATHER_UI_UPDATING;
    weather_ui_apply(&model, &state);
    CHECK(!lv_obj_has_flag(ui.details, LV_OBJ_FLAG_HIDDEN));
    CHECK(strstr(lv_label_get_text(ui.details_body), "22.0") != NULL);
    model.hour_count = 0;
    weather_ui_apply(&model, &state);
    CHECK(strstr(lv_label_get_text(ui.details_body), "недоступны") != NULL);
    CHECK(!lv_obj_has_flag(ui.details, LV_OBJ_FLAG_HIDDEN));
    click(670, 108);
    CHECK(lv_obj_has_flag(ui.details, LV_OBJ_FLAG_HIDDEN));
    click(450, 238);
    CHECK(strstr(lv_label_get_text(ui.details_body), "Минимум") != NULL);
    capture("day-detail");
    SDL_Event escape = {.type = SDL_KEYDOWN};
    escape.key.keysym.sym = SDLK_ESCAPE;
    CHECK(SDL_PushEvent(&escape) == 1);
    advance();
    CHECK(lv_obj_has_flag(ui.details, LV_OBJ_FLAG_HIDDEN));
    click(75, 30);
    CHECK(strstr(lv_label_get_text(ui.details_body), "Open-Meteo") != NULL);
    capture("about-data");
    click(670, 108);
    state.status = WEATHER_UI_READY;
    weather_ui_apply(NULL, &state);
    click(720, 30);
    click(720, 30);
    CHECK(requests == 1);
    CHECK(lv_obj_has_state(ui.refresh, LV_STATE_DISABLED));
    capture("updating");
}

static void state_renders(weather_snapshot_t model)
{
    create(false);
    weather_ui_apply(&model, &state);
    model.current.is_day = 0;
    for (size_t i = 0; i < model.hour_count; ++i) model.hours[i].is_day = 0;
    weather_ui_apply(&model, &state);
    capture("night");
    state.status = WEATHER_UI_OFFLINE;
    state.now += 1800;
    weather_ui_apply(&model, &state);
    capture("offline");
    state.status = WEATHER_UI_STALE;
    state.now = model.fetched_at + 8 * 3600;
    weather_ui_apply(&model, &state);
    CHECK(strcmp(lv_label_get_text(ui.days[0].day), "Сегодня") == 0);
    CHECK(strcmp(lv_label_get_text(ui.days[4].day), "–") == 0);
    capture("stale");
    create(false);
    state.status = WEATHER_UI_LOADING;
    weather_ui_apply(NULL, &state);
    CHECK(!lv_obj_has_flag(ui.notice, LV_OBJ_FLAG_HIDDEN));
    capture("loading");
    state.status = WEATHER_UI_ERROR;
    weather_ui_apply(NULL, &state);
    CHECK(strcmp(lv_label_get_text(ui.temperature), "–") == 0);
    capture("no-cache-error");
    state.status = WEATHER_UI_CONFIG_ERROR;
    snprintf(state.message, sizeof(state.message), "wifi.ssid");
    weather_ui_apply(NULL, &state);
    CHECK(strstr(lv_label_get_text(ui.notice_text), "Исправьте внешний config.json") != NULL);
    CHECK(strstr(lv_label_get_text(ui.notice_text), "wifi.ssid") != NULL);
    CHECK(lv_obj_has_state(ui.refresh, LV_STATE_DISABLED));
    CHECK(strcmp(lv_label_get_text(ui.moon), "–") == 0);
    capture("config-error");
}

static void stress_renders(weather_snapshot_t model)
{
    create(false);
    snprintf(model.location.name, sizeof(model.location.name),
        "Петропавловская Борщаговка, Киевская область");
    model.current.condition = WEATHER_PARTLY_CLOUDY;
    weather_ui_apply(&model, &state);
    capture("long-location");
    model.current.temperature = -23;
    model.current.condition = WEATHER_SNOW;
    for (size_t i = 0; i < model.day_count; ++i) {
        model.days[i].minimum = -30;
        model.days[i].maximum = -18;
        model.days[i].condition = WEATHER_SNOW;
    }
    weather_ui_apply(&model, &state);
    capture("winter");
    for (size_t i = 0; i < 7; ++i) {
        model.hours[i].condition = (weather_condition_t)i;
        model.hours[i].is_day = 1;
    }
    model.current.condition = WEATHER_CLEAR;
    model.current.is_day = 1;
    weather_ui_apply(&model, &state);
    capture("icon-gallery");
    create(true);
    model.current.temperature = 123;
    model.days[0].minimum = -123;
    model.days[0].maximum = 123;
    weather_ui_apply(&model, &state);
    capture("large-three-digit");
    model.current.temperature = NAN;
    model.current.feels_like = NAN;
    model.current.condition = WEATHER_UNKNOWN;
    model.current.is_day = -1;
    model.days[0].sunrise = 0;
    model.days[0].precipitation = 0;
    model.days[1].precipitation = NAN;
    weather_ui_apply(&model, &state);
    CHECK(strcmp(lv_label_get_text(ui.sunrise), "–") == 0);
    CHECK(strcmp(lv_label_get_text(ui.days[0].precipitation), "0%") == 0);
    CHECK(strcmp(lv_label_get_text(ui.days[1].precipitation), "–") == 0);
    capture("partial");
    const uint32_t glyphs[] = {0x0416, 0x044f, 0x00b0, 0x00bb, 0x00fc, 0x2212, 0x2013};
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
        lv_font_glyph_dsc_t glyph;
        CHECK(lv_font_get_glyph_dsc(&weather_font_20, &glyph, glyphs[i], 0));
    }
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    screenshots = argv[2];
    FILE *file = fopen(argv[1], "rb");
    CHECK(file);
    char *json = calloc(WEATHER_MAX_RESPONSE + 1, 1);
    CHECK(json && fread(json, 1, WEATHER_MAX_RESPONSE, file) > 0);
    CHECK(fclose(file) == 0);
    weather_snapshot_t model = {.location = weather_default_location()};
    CHECK(weather_timezone_apply(&model.location));
    CHECK(weather_parse(json, &model, 1784569500));
    free(json);
    model.demo = true;
    CHECK(desktop_display_init());
    navigation_and_snapshot_ownership(model);
    state_renders(model);
    stress_renders(model);
    desktop_display_close();
    puts("PASS: rendered states, Cyrillic glyphs, real SDL navigation, refresh exclusion, card preservation");
    return 0;
}

#include <SDL.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "weather_ui.h"
#include "network.h"

int main(int argc, char **argv)
{
    const char *screenshot = NULL;
    const char *config_path = "config.json";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--config") && i + 1 < argc) config_path = argv[++i];
        else {
            fprintf(stderr, "Usage: weather-preview [--config config.json] [--screenshot image.bmp]\n");
            return 2;
        }
    }
    if (!desktop_display_init()) {
        fprintf(stderr, "SDL display: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    weather_config_t config = {0};
    char error[80], cache_path[1024];
    const bool valid = weather_config_load(config_path, &config, error) &&
        weather_timezone_apply(&config.location);
    const weather_ui_options_t options = {
        .brightness = valid ? config.brightness : 100,
        .large_text = valid && config.large_text,
        .refresh = desktop_network_refresh,
    };
    weather_ui_create(&options);
    weather_ui_state_t state = {.status = valid ? WEATHER_UI_LOADING : WEATHER_UI_CONFIG_ERROR,
        .now = time(NULL), .stale_after_seconds = config.stale_after_seconds};
    if (!valid) snprintf(state.message, sizeof(state.message), "%s", error);
    weather_ui_apply(NULL, &state);
    if (valid && config.mode == WEATHER_MODE_DEMO) {
        weather_snapshot_t snapshot = {.location = config.location};
        char *json = calloc(WEATHER_MAX_RESPONSE + 1, 1);
        FILE *file = fopen(WEATHER_PROJECT_DIR "/fixtures/demo.json", "rb");
        if (!json || !file || !fread(json, 1, WEATHER_MAX_RESPONSE, file) ||
            !weather_parse(json, &snapshot, 1784569500)) {
            fprintf(stderr, "Demo fixture cannot be loaded\n");
            if (file) fclose(file);
            free(json);
            desktop_display_close();
            return 1;
        }
        fclose(file); free(json);
        snapshot.demo = true;
        state.status = WEATHER_UI_READY;
        state.now = 1784569500;
        weather_ui_apply(&snapshot, &state);
    } else if (valid && config.mode == WEATHER_MODE_DIAGNOSTICS) {
        weather_ui_diagnostics();
        state.status = WEATHER_UI_READY;
    } else if (valid) {
        if (snprintf(cache_path, sizeof(cache_path), "%s.weather.cache", config_path) >= (int)sizeof(cache_path) ||
            !desktop_network_start(&config, cache_path)) {
            state.status = WEATHER_UI_ERROR;
            weather_ui_apply(NULL, &state);
        }
    }
    bool running = true;
    unsigned frames = 0;
    desktop_result_t result;
    uint32_t previous = SDL_GetTicks();
    while (running) {
        const uint32_t now = SDL_GetTicks();
        running = desktop_display_step(now - previous);
        previous = now;
        if (desktop_network_receive(&result)) {
            state = result.state;
            weather_ui_apply(result.valid ? &result.snapshot : NULL, &state);
        }
        if (valid && config.mode == WEATHER_MODE_LIVE && time(NULL) / 60 != state.now / 60) {
            state.now = time(NULL);
            weather_ui_apply(NULL, &state);
        }
        const bool ready = state.status != WEATHER_UI_LOADING && state.status != WEATHER_UI_UPDATING;
        if (screenshot && ready && ++frames == 10) {
            if (!desktop_display_save(screenshot)) {
                fprintf(stderr, "Screenshot: %s\n", SDL_GetError());
                desktop_network_close();
                desktop_display_close();
                return 1;
            }
            running = false;
        }
        SDL_Delay(5);
    }
    desktop_network_close();
    desktop_display_close();
    return 0;
}

#pragma once
#include "weather_data.h"

typedef enum {
    WEATHER_MODE_LIVE, WEATHER_MODE_DEMO, WEATHER_MODE_DIAGNOSTICS
} weather_mode_t;

typedef struct {
    weather_mode_t mode;
    weather_location_t location;
    char wifi_ssid[33], wifi_password[65], ntp_server[128];
    unsigned brightness;
    bool large_text;
    unsigned update_seconds, request_timeout_ms, stale_after_seconds, cache_write_seconds;
} weather_config_t;

bool weather_config_parse(const char *json, weather_config_t *out, char error_field[80]);
bool weather_config_load(const char *path, weather_config_t *out, char error_field[80]);

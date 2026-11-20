#pragma once
#include "weather_config.h"
#include "weather_ui.h"

typedef struct {
    bool valid;
    weather_snapshot_t snapshot;
    weather_ui_state_t state;
} desktop_result_t;

bool desktop_network_start(const weather_config_t *config, const char *cache_path);
void desktop_network_refresh(void *context);
bool desktop_network_receive(desktop_result_t *result);
void desktop_network_close(void);

#pragma once
#include "weather_config.h"
#include "weather_ui.h"

typedef struct {
    bool valid;
    weather_snapshot_t snapshot;
    weather_ui_state_t state;
} weather_result_t;

void weather_network_start(const weather_config_t *config);
void weather_network_refresh(void *context);
bool weather_network_receive(weather_result_t *result);

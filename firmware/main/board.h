#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

esp_err_t board_init(esp_lcd_panel_handle_t *panel);
bool board_touch_read(uint16_t *x, uint16_t *y);
esp_err_t board_expander_write(uint8_t mask, bool high);

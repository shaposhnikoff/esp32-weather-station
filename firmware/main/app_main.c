#include "board.h"
#include <assert.h>
#include <stdio.h>
#include "board_profile.h"
#include "weather_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "demo_fixture.h"
#include "weather_network.h"
#include "esp_littlefs.h"

static weather_snapshot_t demo_snapshot;
static weather_result_t incoming;
static weather_config_t config;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *colors)
{
    esp_lcd_panel_handle_t panel = driver->user_data;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
        area->x2 + 1, area->y2 + 1, colors));
    lv_disp_flush_ready(driver);
}

static void read_touch(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    uint16_t x, y;
    if (board_touch_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void tick(void *argument)
{
    (void)argument;
    lv_tick_inc(2);
}

void app_main(void)
{
    char config_error[80] = "file";
    const esp_vfs_littlefs_conf_t config_fs = {
        .base_path = "/config", .partition_label = "config",
        .read_only = true, .format_if_mount_failed = false,
    };
    const bool configured = esp_vfs_littlefs_register(&config_fs) == ESP_OK &&
        weather_config_load("/config/config.json", &config, config_error);
    bool valid_config = configured && weather_timezone_apply(&config.location);
    if (!valid_config) ESP_LOGE("weather", "Invalid external config.json: %s", config_error);
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(board_init(&panel));
    lv_init();
    lv_color_t *pixels = heap_caps_malloc(BOARD_WIDTH * 40 * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(pixels);
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, BOARD_WIDTH * 40);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = BOARD_WIDTH;
    display.ver_res = BOARD_HEIGHT;
    display.flush_cb = flush;
    display.draw_buf = &buffer;
    display.user_data = panel;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t input;
    lv_indev_drv_init(&input);
    input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = read_touch;
    lv_indev_drv_register(&input);
    const esp_timer_create_args_t timer = {.callback = tick, .name = "lvgl_tick"};
    esp_timer_handle_t handle;
    ESP_ERROR_CHECK(esp_timer_create(&timer, &handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(handle, 2000));
    const weather_ui_options_t options = {
        .brightness = valid_config ? config.brightness : 100,
        .large_text = valid_config && config.large_text,
        .refresh = weather_network_refresh,
    };
    weather_ui_create(&options);
    if (valid_config && !config.brightness)
        ESP_ERROR_CHECK(board_expander_write(BOARD_BACKLIGHT_BIT, false));
    weather_ui_state_t state = {
        .status = valid_config ? WEATHER_UI_LOADING : WEATHER_UI_CONFIG_ERROR,
        .now = time(NULL), .stale_after_seconds = config.stale_after_seconds,
    };
    if (!valid_config) snprintf(state.message, sizeof(state.message), "%s", config_error);
    weather_ui_apply(NULL, &state);
    if (valid_config && config.mode == WEATHER_MODE_DEMO) {
        demo_snapshot.location = config.location;
        ESP_ERROR_CHECK(weather_parse(demo_json, &demo_snapshot, 1784569500) ? ESP_OK : ESP_FAIL);
        demo_snapshot.demo = true;
        state.status = WEATHER_UI_READY;
        state.now = 1784569500;
        weather_ui_apply(&demo_snapshot, &state);
    } else if (valid_config && config.mode == WEATHER_MODE_DIAGNOSTICS) {
        weather_ui_diagnostics();
    } else if (valid_config) {
        const esp_vfs_littlefs_conf_t cache_fs = {
            .base_path = "/cache", .partition_label = "cache", .format_if_mount_failed = true,
        };
        ESP_ERROR_CHECK(esp_vfs_littlefs_register(&cache_fs));
        weather_network_start(&config);
    }
    ESP_LOGI("weather", "Weather UI ready: %s panel=%dx%d mode=%d",
        BOARD_PROFILE_NAME, BOARD_WIDTH, BOARD_HEIGHT, valid_config ? (int)config.mode : -1);
    ESP_LOGI("memory", "PSRAM total=%u free=%u internal free=%u largest=%u",
        (unsigned)esp_psram_get_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    while (true) {
        if (weather_network_receive(&incoming)) {
            state = incoming.state;
            weather_ui_apply(incoming.valid ? &incoming.snapshot : NULL, &state);
        }
        if (valid_config && config.mode == WEATHER_MODE_LIVE && time(NULL) / 60 != state.now / 60) {
            state.now = time(NULL);
            weather_ui_apply(NULL, &state);
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

#include "board.h"
#include "board_profile.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint8_t outputs = BOARD_TOUCH_RESET_BIT | BOARD_LCD_RESET_BIT |
                         BOARD_SD_CS_BIT | BOARD_USB_SELECT_BIT;

esp_err_t board_expander_write(uint8_t mask, bool high)
{
    const uint8_t next = high ? (outputs | mask) : (outputs & ~mask);
    const esp_err_t error = i2c_master_write_to_device(
        I2C_NUM_0, BOARD_EXPANDER_OUTPUT_ADDRESS, &next, 1, pdMS_TO_TICKS(50));
    if (error == ESP_OK) outputs = next;
    return error;
}

static esp_err_t reset_devices(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER, .sda_io_num = BOARD_SDA, .scl_io_num = BOARD_SCL,
        .sda_pullup_en = true, .scl_pullup_en = true, .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, config.mode, 0, 0, 0));
    const uint8_t mode = 1;
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_NUM_0,
        BOARD_EXPANDER_MODE_ADDRESS, &mode, 1, pdMS_TO_TICKS(50)));
    ESP_ERROR_CHECK(board_expander_write(BOARD_LCD_RESET_BIT, false));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(board_expander_write(BOARD_LCD_RESET_BIT, true));
    gpio_set_direction(BOARD_TOUCH_INT, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_TOUCH_INT, 0);
    ESP_ERROR_CHECK(board_expander_write(BOARD_TOUCH_RESET_BIT, false));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(board_expander_write(BOARD_TOUCH_RESET_BIT, true));
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_reset_pin(BOARD_TOUCH_INT);
    return ESP_OK;
}

esp_err_t board_init(esp_lcd_panel_handle_t *panel)
{
    ESP_ERROR_CHECK(reset_devices());
    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = BOARD_PIXEL_CLOCK_HZ, .h_res = BOARD_WIDTH, .v_res = BOARD_HEIGHT,
            .hsync_pulse_width = BOARD_HSYNC_PULSE, .hsync_back_porch = BOARD_HSYNC_BACK,
            .hsync_front_porch = BOARD_HSYNC_FRONT, .vsync_pulse_width = BOARD_VSYNC_PULSE,
            .vsync_back_porch = BOARD_VSYNC_BACK, .vsync_front_porch = BOARD_VSYNC_FRONT,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16, .bits_per_pixel = 16, .num_fbs = 1,
        .bounce_buffer_size_px = BOARD_WIDTH * 10, .dma_burst_size = 64,
        .hsync_gpio_num = BOARD_HSYNC, .vsync_gpio_num = BOARD_VSYNC,
        .de_gpio_num = BOARD_DE, .pclk_gpio_num = BOARD_PCLK, .disp_gpio_num = -1,
        .data_gpio_nums = BOARD_RGB_PINS, .flags.fb_in_psram = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&config, panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel));
    return board_expander_write(BOARD_BACKLIGHT_BIT, true);
}

static esp_err_t touch_register(uint16_t reg, uint8_t *data, size_t size)
{
    const uint8_t address[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_write_read_device(I2C_NUM_0, BOARD_TOUCH_ADDRESS,
        address, 2, data, size, pdMS_TO_TICKS(20));
}

bool board_touch_read(uint16_t *x, uint16_t *y)
{
    static bool contact;
    static uint16_t previous_x, previous_y;
    uint8_t status;
    if (touch_register(0x814e, &status, 1) != ESP_OK) {
        contact = false;
        return false;
    }
    if (!(status & 0x80)) {
        *x = previous_x;
        *y = previous_y;
        return contact;
    }
    uint8_t point[8];
    const bool pressed = (status & 0x0f) > 0 && (status & 0x0f) <= 5 &&
                         touch_register(0x814f, point, sizeof(point)) == ESP_OK;
    const uint8_t clear[] = {0x81, 0x4e, 0};
    const esp_err_t error = i2c_master_write_to_device(I2C_NUM_0,
        BOARD_TOUCH_ADDRESS, clear, sizeof(clear), pdMS_TO_TICKS(20));
    contact = false;
    if (!pressed || error != ESP_OK) return false;
    *x = point[1] | ((uint16_t)point[2] << 8);
    *y = point[3] | ((uint16_t)point[4] << 8);
    contact = *x < BOARD_WIDTH && *y < BOARD_HEIGHT;
    previous_x = *x;
    previous_y = *y;
    return contact;
}

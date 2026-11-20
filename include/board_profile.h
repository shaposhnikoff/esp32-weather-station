#pragma once

/* The attached factory firmware's LCD_CAM registers confirm 1024x600.
 * Both variants share pins; each has explicit RGB timing. */
#ifdef WEATHER_BOARD_LCD5B
#define BOARD_PROFILE_NAME "waveshare-esp32-s3-touch-lcd-5b-1024x600"
#define BOARD_WIDTH 1024
#define BOARD_HEIGHT 600
#define BOARD_PIXEL_CLOCK_HZ 21000000
#define BOARD_HSYNC_PULSE 30
#define BOARD_HSYNC_BACK 145
#define BOARD_HSYNC_FRONT 170
#define BOARD_VSYNC_PULSE 2
#define BOARD_VSYNC_BACK 23
#define BOARD_VSYNC_FRONT 12
#else
#define BOARD_PROFILE_NAME "waveshare-esp32-s3-touch-lcd-5-800x480"
#define BOARD_WIDTH 800
#define BOARD_HEIGHT 480
#define BOARD_PIXEL_CLOCK_HZ 16000000
#define BOARD_HSYNC_PULSE 4
#define BOARD_HSYNC_BACK 8
#define BOARD_HSYNC_FRONT 8
#define BOARD_VSYNC_PULSE 4
#define BOARD_VSYNC_BACK 8
#define BOARD_VSYNC_FRONT 8
#endif
#define BOARD_RGB_PINS {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40}
#define BOARD_PCLK 7
#define BOARD_HSYNC 46
#define BOARD_VSYNC 3
#define BOARD_DE 5
#define BOARD_SDA 8
#define BOARD_SCL 9
#define BOARD_TOUCH_INT 4
#define BOARD_TOUCH_ADDRESS 0x5d
#define BOARD_EXPANDER_MODE_ADDRESS 0x24
#define BOARD_EXPANDER_OUTPUT_ADDRESS 0x38
#define BOARD_TOUCH_RESET_BIT (1u << 1)
#define BOARD_BACKLIGHT_BIT (1u << 2)
#define BOARD_LCD_RESET_BIT (1u << 3)
#define BOARD_SD_CS_BIT (1u << 4)
#define BOARD_USB_SELECT_BIT (1u << 5)
#define BOARD_SD_MOSI 11
#define BOARD_SD_CLOCK 12
#define BOARD_SD_MISO 13

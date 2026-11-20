# Hardware profile

## Supported board

The default profile is `waveshare-lcd5b`, a 1024 x 600 landscape RGB565 LCD,
GT911 capacitive touch, CH422G expander and ESP32-S3 with 16 MB flash and
8 MB octal PSRAM. The attached chip reports revision v0.2. The physical PCB
revision remains unknown.

Before replacing the factory firmware, USB JTAG reads of LCD_CAM registers gave
`0x6004101c=0xce12b8ab` and `0x60041020=0x4b33ff15`. The active width/height
fields decode to 1024 x 600. The user subsequently confirmed that the shared
weather screen is visible with this profile. The separate 800 x 480
`waveshare-lcd5` profile remains available; timings are not interchangeable.

`include/board_profile.h` contains all board pins and RGB timings.

## Authoritative sources

- [Waveshare wiki and pin tables](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-5)
- [Espressif/Waveshare supported profile v1.0.0](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/v1.0.0/src/board/supported/waveshare/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_5.h)
- [ESP-IDF 5.4 RGB LCD API](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html)
- [PlatformIO espressif32 6.10.0 manifest](https://github.com/platformio/platform-espressif32/blob/v6.10.0/platform.json)

The 800 profile uses the vendor-supported 16 MHz PCLK, H/V pulse width 4 and
porches 8. The attached 1024 profile uses 21 MHz PCLK, horizontal
pulse/back/front 30/145/170 and vertical 2/23/12, with no mirror or axis swap.

| Signal | GPIO or expander |
| --- | --- |
| RGB D0-D15 | 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 |
| PCLK / HSYNC / VSYNC / DE | 7 / 46 / 3 / 5 |
| I2C SDA / SCL | 8 / 9, 400 kHz |
| GT911 interrupt / address | 4 / 0x5D |
| CH422G mode / output register addresses | 0x24 / 0x38 |
| Touch reset / backlight / LCD reset | EXIO1 / EXIO2 / EXIO3 |
| SD MOSI / SCLK / MISO | 11 / 12 / 13 |
| SD chip select | EXIO4 |

Backlight control is a digital switch on this board, not a native PWM pin.
Any intermediate brightness setting must use explicitly documented software
dimming; do not claim a hardware PWM capability.

## Software and memory

The greenfield project uses ESP-IDF 5.4.0, PlatformIO espressif32 6.10.0,
framework-espidf 3.50400.0 and LVGL 8.4.0 on both targets.
`dependencies.lock` records the actual IDF and LVGL versions and LVGL hash.

- RGB frame buffer: 1,228,800 bytes in PSRAM for 1024 x 600.
- LVGL draw buffer: 40 lines, 81,920 bytes in PSRAM.
- RGB DMA bounce buffers: two sets of 10 lines, 40,960 bytes in internal RAM.
- Flash and PSRAM clocks: conservative 80 MHz; CPU 240 MHz.
- LVGL owns a single UI context; only the tick increments from an ESP timer.
- `app_main` logs internal free/largest block and PSRAM total/free after UI init.

The PlatformIO-bundled CMake is x86_64-only and failed on this ARM Mac with
`OSError: [Errno 86] Bad CPU type in executable`. The project-local
`tools/native-cmake` package invokes the native UV wheel for CMake 3.30.5
with Python 3.12 and Ninja 1.11.1.3. Shared PlatformIO packages are unchanged.
This helper supports POSIX hosts with UV; Windows support is unverified.

## Verified baseline

On November 20, 2026:

- `pio run -e waveshare-lcd5`: exit 0 after the native-CMake correction.
- Static RAM: 48,800 / 327,680 bytes; app flash: 443,694 / 6,291,456 bytes.
- Firmware image: `.pio/build/waveshare-lcd5/firmware.bin`, 444,096 bytes.
- Desktop CMake build: exit 0.
- `SDL_VIDEODRIVER=dummy ./build-desktop/weather-preview --screenshot build-desktop/hardware-diagnostics.bmp`:
  exit 0. The 800 x 480 render was inspected: RGB fields and readable white
  counter/touch labels are present.

The baseline image sizes above precede weather integration.

## Device evidence and remaining checks

The board is attached at `/dev/cu.usbmodem21211201`. The stable demo boot reported
8,388,608 bytes of PSRAM, 7,074,916 free PSRAM bytes, 287,039 free internal bytes
and a largest internal allocation of 159,744 bytes. The display was confirmed
visible by the user. Touch at all four corners, intermediate software brightness
and long-duration stability remain unverified on hardware.

Configuration is `/config/config.json` on a dedicated read-only LittleFS
partition, provisioned as a whole image over USB. A separate LittleFS partition
stores the cache and supports the required fsync/rename operations. The TF
interface has not been enabled or tested.

The factory bootloader header and the SDK-generated flashing arguments both
select DIO. Forcing QIO in esptool caused ROM watchdog resets and a black screen;
restoring DIO corrected boot. Runtime QIO and octal PSRAM remain enabled.
`CONFIG_LV_CONF_SKIP=n` is required so the target uses the shared custom
allocator and float formatter rather than incompatible LVGL defaults.

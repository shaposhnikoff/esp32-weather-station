# UI render investigation

## Initial SDL render crash

Symptom: the first weather UI render terminated with macOS bus error 10.

Ranked hypotheses:

1. Display/input driver lifetime after moving initialization into a helper.
   Expected: LVGL retains pointers to automatic variables after the helper
   returns; making driver storage static removes the invalid access.
2. Custom icon draw callback corrupts drawing state.
   Expected: a sanitizer or debugger points to a custom draw callback.
3. Generated RGB565/font payload layout is incompatible.
   Expected: failure occurs while reading a particular generated asset.

Evidence: LVGL 8.4 `src/hal/lv_hal_disp.c:185` assigns
`disp->driver = driver`; `src/hal/lv_hal_indev.c:93` similarly retains the
input driver pointer. `desktop_display_init` incorrectly supplied automatic
structures, unlike the original `main`-lifetime implementation.

Experiment: changed both driver structures to static storage, preserving their
lifetime for the entire display session. Rebuild and render results are recorded
in the weather-ui handoff. No retry/guard was added around the invalid lifetime.

The render still failed after the lifetime correction. A separate ASan/UBSan
build then identified `_strnlen_s` in `lv_printf.c`, called by `ui_value` through
`lv_label_set_text_fmt`. LVGL's float formatter was disabled by default.
With a mixed `%.0f%s` format, the disabled float branch did not consume the
floating argument, and the following string conversion read an invalid pointer.
The shared `lv_conf.h` now explicitly enables `LV_SPRINTF_USE_FLOAT`.
This header change was first verified on desktop. Target configuration must
also permit the shared header, as the device investigation below established.

## Attached-device investigation

The factory application was `rtc_pcf85063`, version 1, built August 12, 2025.
ESP32-S3 revision v0.2, 16 MB flash and 8 MB PSRAM were read from the device.
Without writing flash, USB JTAG read active LCD_CAM registers
`0x6004101c=0xce12b8ab` and `0x60041020=0x4b33ff15`.
The SDK HAL decodes these as 1024 x 600. The physical PCB revision is unknown.

An initial manual flash command incorrectly forced the bootloader header to
QIO, although both the factory and generated bootloader headers specify DIO.
ROM repeatedly stopped at `ets_loader.c 78` and reset via TG0WDT.
Rewriting only the bootloader with DIO restored application startup.
The normal bootloader then enables QIO itself; do not override its DIO header.

The application subsequently reset in LVGL `_vsnprintf`, reached from
`ui_value` while applying the first snapshot. The target's generated SDK
configuration had `CONFIG_LV_CONF_SKIP=y`, so it ignored the shared header and
compiled LVGL with a 32 KiB static heap and float formatting disabled.
This was different from the desktop configuration despite identical UI sources.

The fix sets `CONFIG_LV_CONF_SKIP=n` in defaults and selects the fresh generated
`sdkconfig.weather` through PlatformIO's supported sdkconfig_path setting.
A public UI-header compile-time contract now rejects missing custom allocation,
float formatting or RGB565. A negative compile test demonstrates rejection of
the skipped-header configuration and acceptance of the shared header.

Local target builds pass with `pio run -e waveshare-lcd5b -j 1`.
Static RAM fell from 60,160 to 27,360 bytes, consistent with removing the
incorrect fixed heap. Parallel SCons 4.8.1 builds intermittently fail importing
the existing FortranCommon module; serial builds avoid that separate tool issue.
ASan/UBSan UI/data tests and the configuration-contract test pass locally.

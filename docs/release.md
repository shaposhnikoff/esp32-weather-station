# Release evidence — 2026-11-20

## Deliverables

- Firmware sources: `firmware/main/`, `components/`, `include/`.
- Shared LVGL desktop preview: `desktop/`, root `CMakeLists.txt`.
- Safe settings template: `config.example.json`.
- Configuration validator and USB image workflow:
  `tools/check-config.c`, `tools/provision-config.sh`.
- Built firmware: `.pio/build/waveshare-lcd5b/firmware.bin`.
- Locally assembled firmware bundle, excluded from Git: `dist/weather-station/`; SHA-256 values:
  `dist/weather-station/SHA256SUMS` and `manifest.json`.
- Preview screenshots: `docs/screenshots/`.
- Deployment procedure: [deployment.md](deployment.md).
- Portable board contract: [board-handoff.md](board-handoff.md).
- Requirement ledger: [acceptance.md](acceptance.md).
- Sequential stage evidence: [handoff.md](handoff.md).
- Runtime flow: [diagrams/station-flow.md](diagrams/station-flow.md).
- Future OTA work, not implemented: [../tasks/todo.md](../tasks/todo.md).

## Builds and checks actually executed

The default device profile is `waveshare-lcd5b`, 1024x600, 16 MB Flash,
8 MB octal PSRAM, ESP-IDF 5.4.0, LVGL 8.4.0, ESP LittleFS 1.19.1.

```sh
make clean-firmware PIO=/Users/max/.platformio/penv/bin/pio
make firmware PIO=/Users/max/.platformio/penv/bin/pio
```

Clean build: exit 0, 180.88 seconds. The final one-file UI correction was then
rebuilt successfully in 9.87 seconds. Final static RAM: 62,376 / 327,680 bytes;
app flash: 1,816,806 / 6,291,456 bytes. Final application binary: 1,817,200 bytes.

The ordinary `make desktop` build and its original nine tests passed.
Five added network-worker scenarios then passed. A fresh `build-release`
compiled all project objects with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 cmake \
  -S . -B build-release -DWEATHER_DESKTOP=ON -DWEATHER_PREVIEW_NATIVE=ON \
  -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=/Users/max/git/oled/weather/build-desktop/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_CJSON=/Users/max/git/oled/weather/build-desktop/_deps/cjson-src \
  "-DCMAKE_C_FLAGS=-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 cmake \
  --build build-release -j8
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 ctest \
  --test-dir build-release --output-on-failure
```

That full run passed all 14 tests registered at the time. Dependency source
directories were reused from the previously fetched pinned sources, not their
object files. After adding native-resolution pointer tests and the last
configuration-error correction, both affected UI suites passed again:

```sh
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 ctest \
  --test-dir build-release -R weather-ui --output-on-failure
```

There are now 15 registered tests, with passing evidence for all of them.
No sanitizer error was reported. The upstream cJSON CMake compatibility warning
remains visible; application targets use `-Wall -Wextra -Werror`.

## Controlled scenarios and observable UI

`tests/test_network.c` runs the production desktop worker with only libcurl
transport and monotonic time replaced. Its five groups cover success,
timeout, bad JSON, TLS failure, 429 with Retry-After, recovery, duplicate/manual
refresh gating, a delayed mailbox wake before the automatic interval, cached
offline startup, first startup offline, changed-location cache rejection and
the exact 65,536-byte body boundary. The transport mock asserts HTTPS,
certificate/hostname validation, disabled redirects and the configured timeout.
These tests do not establish actual ESP32 radio/TLS timeout behavior.

Separate preview processes loaded safe external JSON files without rebuilding:
explicit demo, changed location/brightness/large text, invalid password type,
first start offline, restart using real cached data and changed-location cache
rejection. Offline runs used an unreachable local HTTPS proxy. All exited 0
after saving screenshots, with no sanitizer error. Their 1024x600 screenshots
were visually inspected; the configuration-error screen was recaptured after
its final correction. No user credentials are in these QA configurations.

The original 800x480 day/night/stress renders were compared with the reference.
Real SDL pointer events verify cards, Back/Escape, refresh exclusion and selection
preservation at both resolutions. At 1024x600 the 800x480 layout scales to
1000x600 with intentional 12-pixel side margins.

`sh -n tools/provision-config.sh` and the pinned generator patch check passed.
The native generator successfully created the private image that was manually
uploaded for the live device test. The helper's final upload command was not
executed during final QA because the user had repurposed the board.

## Hardware evidence and limits

Before the user changed the installed project:

- USB JTAG established native 1024x600 from the factory LCD_CAM registers.
- The user confirmed the weather screen is visible.
- DIO boot, octal 8 MB PSRAM and a successful PSRAM test were logged.
- The board acquired Wi-Fi IP and received verified Open-Meteo HTTPS HTTP 200,
  temperature 7.3 C and 120 hourly records.
- Post-UI/Wi-Fi initialization: 7,007,120 free PSRAM bytes, 119,919 free internal
  bytes and largest internal block 63,488 bytes.

During initial QA the final UI-only corrections were compiled and checked on
the shared desktop UI while the board contained a user-installed different
project. Its observed 7 MiB factory partition lacked this station's `config`
and `cache`; that explained the configuration-partition error.

The user subsequently authorized installing the weather station again. The
final 1,817,200-byte application, its matching DIO bootloader/partition table
and the private configuration image were written successfully. esptool verified
all image hashes. The final device boot reported native 1024x600, a successful
8 MB PSRAM test, Wi-Fi IP acquisition and Open-Meteo HTTP 200 with 7.5 C and
120 hourly records. Free internal memory after initialization was 129,027 bytes;
largest internal block was 63,488 bytes, free PSRAM 7,007,120 bytes.

Not run on hardware: corner touch calibration, brightness levels, repeated
update cycles, AP interruption/recovery, TLS timeout/429 injection, sustained
heap measurements during HTTPS, and long-duration leak/flicker testing.
These are hardware verification limits, not claimed passes. The desktop worker
API is one start/close per process; reopening it inside one process is outside
the current application lifecycle.

## Using the firmware bundle

The files in `dist/weather-station/` are copied from the final successful build.
They contain compiled UI resources; a separate font/image filesystem is not
needed. `config.example.json` is a template, not a configured device image.

From that directory, with the installed PlatformIO Python/esptool and an
explicitly selected device:

```sh
"$HOME/.platformio/penv/bin/python" \
  "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" \
  --chip esp32s3 --port /dev/cu.usbmodemXXXX --baud 460800 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Then provision the whole private external configuration as described in the
deployment guide. Across different projects use a matching bootloader/table/app
set; an application-only update is valid only with compatible existing partitions.
Private config images, Wi-Fi values and flash backups are excluded from the
bundle. No Git commit or push was made during the original firmware-development
stage; repository publication was authorized separately afterward.

The final bundle audit checked 48 public files and 69 source/resource text
files against the private Wi-Fi values without printing them: zero matches.
All three binary SHA-256 values matched the manifest, the bootloader header
was DIO, and bundled Markdown links and code fences were valid.

## Repository publication

The project is prepared for
`shaposhnikoff/ESP32_S3_Weather_station_display` with a Russian README and a
captioned screenshot gallery. A fresh live preview returned HTTPS HTTP 200,
7.5 C; five native 1024x600 screenshots were generated and visually inspected.
The publication check ran all 15 registered CTest groups together: all passed
with the existing ASan/UBSan build and no sanitizer error.

The screenshot gallery explicitly separates live data, deterministic demo
states and offline/error tests. It contains desktop renders of the actual shared
LVGL UI, not photographs of the physical panel.

The initial Git whitespace check reports existing trailing whitespace/blank
lines in the upstream font license, generated font sources, the original user
plan and literal context lines in the generator patch. These files are retained
without unrelated formatting changes. The edited publication documentation is
checked separately.

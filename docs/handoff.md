# Stage handoffs

The sections are chronological. The final profile and storage decisions in
firmware-integration supersede the initial 800x480/TF assumptions.

## hardware-base

Date: 2026-11-20. Implemented and verified by the lead after delegated launches
stalled without producing files. The Anthropic launch was cancelled and is
prohibited by the user's provider instruction; subsequent launches select
ChatGPT subscription explicitly. No agent completion is claimed for cancelled
tasks. No Git repository existed, so worktree isolation was unavailable; all
stage work is sequential and no Git commits were made.

### Changed files

- Root `CMakeLists.txt`, `platformio.ini`, `sdkconfig.defaults`,
  `partitions.csv`, `dependencies.lock`, `Makefile`, `.gitignore`, `README.md`.
- `include/board_profile.h`, `include/lv_conf.h`.
- `firmware/main/{CMakeLists.txt,idf_component.yml,board.h,board.c,app_main.c}`.
- `components/weather_ui/{CMakeLists.txt,ui_base.c,include/weather_ui.h}`.
- `desktop/main.c`, `tools/native-cmake/{package.json,bin/cmake}`.
- `docs/hardware.md`, `docs/diagrams/station-flow.md`, this handoff.

### Commands and evidence

```sh
pio run -e waveshare-lcd5
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 cmake -S . -B build-desktop -DWEATHER_DESKTOP=ON -DCMAKE_PREFIX_PATH=/opt/homebrew
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 cmake --build build-desktop -j 8
SDL_VIDEODRIVER=dummy ./build-desktop/weather-preview --screenshot build-desktop/hardware-diagnostics.bmp
```

All final commands exited 0. The initial firmware attempt failed because
PlatformIO supplied x86_64-only CMake; the scoped native-CMake package fixed
that cause. Firmware uses 48,800 bytes static RAM and 443,694 bytes app flash.
The desktop diagnostic image was visually inspected.

### Decisions and downstream contract

ESP-IDF 5.4.0 and LVGL 8.4.0 are pinned. Board profile and sources are in
`docs/hardware.md`; physical PCB revision is unconfirmed. Only LCD-5 800 x 480,
not LCD-5B, is supported. UI component code is shared unchanged between targets.
The next stage should add an LVGL-independent weather component, fixtures and
desktop tests, retaining the existing build paths. Weather data, cache and
configuration must not embed secrets or require onboard settings. External
configuration will be a single TF-root `config.json`, applied on restart.

Stage transition gate: minimal firmware and desktop compile, sourced hardware
profile recorded, hardware-test status explicit. Physical LCD/touch/TF and
runtime heap checks were not performed because no board is attached.

## weather-data

Implemented by the lead after the stage agent stalled on its first read and
was cancelled without edits. Only the configured ChatGPT subscription route
was used for this stage's delegation.

Changed: `components/weather_data/**`, `fixtures/demo.json`,
`tests/test_weather.c`, desktop/firmware CMake wiring,
`docs/weather-data.md`, `docs/third-party-licenses.md`,
`docs/diagrams/weather-data-flow.md`.

Verified commands:

```sh
make desktop
uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 ctest --test-dir build-desktop --output-on-failure
pio run -e waveshare-lcd5
```

Desktop tests: six of six pass. Each group was also shown to fail for its
intended regression under a deliberate mutation; the original implementation
was restored and the final six-of-six run passed. Final target build exited 0
in 7.90 seconds, with 48,816 bytes static RAM and 443,742 bytes app flash.

The desktop build needed an explicit build-interface include path for cJSON;
the parser's helper was renamed to avoid POSIX's global `daylight` symbol.
These build failures are resolved. cJSON's upstream CMake compatibility warning
remains visible.

Downstream UI contract: load `fixtures/demo.json`, initialize snapshot.location
from `weather_default_location`, call `weather_timezone_apply` once at boot,
then `weather_parse` with frozen fixture time 1784569500 and explicitly mark the
snapshot `demo=true`. Use `weather_future_hours`, `weather_visible_days` and
`weather_today` rather than fixed indices. All missing numbers are NAN. Do not
put demo data in live mode or normal cache. UI must distinguish missing
precipitation from zero and preserve unknown daylight.

Transition gate met: typed offline fixture usable without LVGL/network, parser,
midnight, DST, unknown-code, stale-cache and recovery tests pass. Network
transport, UI and external-config loading remain subsequent stages.

## weather-ui

The sequential ChatGPT subscription UI adviser completed a tool-free contract
review. It identified timestamp-based selection, missing values, forecast
freshness, touch-target density and snapshot ownership as the main risks.
Its alternate geometry used undersized 26 px daily rows, so the lead retained
the reference layout with 54 px rows and a right-hand solar/lunar column.
Implementation and observable checks were performed by the lead.

Changed: `components/weather_ui/**`, `desktop/{display.h,display.c,main.c}`,
`assets/**`, `tools/generate-assets.sh`, `tools/generate-landscape.c`,
`tests/test_ui.c`, CMake wiring, `include/lv_conf.h`, `DESIGN.md`, `DEBUG.md`,
`docs/diagrams/weather-ui-flow.md`, and 16 PNGs under `docs/screenshots/`.

The same LVGL screen now renders current weather, seven hourly columns,
five daily rows, sunrise/sunset and lunar phase. Hour/day overlays and read-only
About Data use actual SDL pointer events and Escape/Back navigation. Snapshot
updates preserve selection by timestamp and keep an unavailable card open if
its record disappears. There is no settings screen or editing control.
Russian strings are centralized in `ui_strings.h` and `ui_strings.c`.

JetBrains Mono 2.304 is retained with its OFL license. Compiled fonts cover
Cyrillic, Latin-1, degree, minus, dash and ellipsis. The original generated
landscape assets occupy 436,800 bytes of flash; no reference pixels are reused.
Fog, rain, snow, thunder, day/night clear weather, unknown conditions and the
solar events have distinct drawing commands.

Checks:

- Desktop build: exit 0 with warnings treated as errors for application code.
- Six data CTest groups plus the SDL UI scenario: seven of seven passed.
- Latest UI scenario under ASan/UBSan: pass, with all 16 states rendered.
- Target `pio run -e waveshare-lcd5`: exit 0, 7.65 seconds.
- Day, night, offline, stale, long-location, large/three-digit, partial,
  About Data and error screenshots were visually inspected at 800 x 480.
- The missing closing guillemet was reproduced by a failing glyph test, then
  fixed by regenerating the font subset; the final test passed.

The first UI render exposed two actual causes: automatic display/input driver
structures whose pointers outlived initialization, and LVGL's disabled float
formatter mishandling a mixed float/string format. Static driver lifetime and
explicit float-format support fixed them; ASan/UBSan confirmed the corrected
render. Generated-font includes also needed the simple-LVGL-header definition
on the ESP-IDF component. See DEBUG.md.

Transition gate met: shared desktop UI launches, states and navigation pass,
screenshots saved and compared with the reference. This stage's standalone
preview is explicitly fixture/demo driven; the target entry point still runs
the diagnostic screen until firmware-integration connects configuration,
network workers and the shared UI. No live-network application behavior is
claimed yet, and no physical board tests were performed.

## firmware-integration

The ChatGPT subscription integration adviser reviewed worker/queue contracts.
A separate sequential configuration adviser supplied parser code; the lead
reviewed, split lexical validation and added the nesting bound before applying
it. Implementation remained with the lead; no failed child is counted as
completed implementation.

Changed: `components/weather_app/**`, `firmware/main/weather_network.*`,
`firmware/main/app_main.c`, `desktop/network.*`, `desktop/main.c`,
`config.example.json`, partition/CMake/SDK manifests and the USB provisioning
tools. The actual panel was identified by factory LCD_CAM registers as
1024x600; the default is now `waveshare-lcd5b`.

Configuration is one complete external file at `/config/config.json`,
read once from read-only LittleFS. Cache uses separate LittleFS with CRC,
schema/location identity and atomic persistence. Wi-Fi storage is RAM, not a
parallel persistent settings store. The worker performs Wi-Fi, bounded SNTP,
certificate-verified HTTPS, parsing and cache writes outside LVGL. By-value
delivery preserves previous data on failures.

The device flashed successfully, the user confirmed the screen, and serial
logs showed IP acquisition plus real HTTPS HTTP 200, 7.3 C and 120 hourly
records. Desktop real HTTPS also returned 7.3 C. The private configuration and
image remained outside the repository.

Independent source review found four concrete defects: an OOM path leaving a
deleted worker handle, a spurious desktop wake bypassing the automatic interval,
incomplete propagation of the HTTP deadline to header reads, and rejection of
an exact-limit body. The lead fixed the causes using a stable refresh semaphore,
an explicit due/manual predicate, remaining-budget headers and an extra-byte
overflow probe. A second independent review found no blocking defects in the
provided current sources.

Transition gate met: both targets build and consume the same strict schema,
external-file live startup succeeded, worker ownership is explicit, and no
onboard configuration interface exists. The SD interface is not used.

## qa-release

The sequential `qa-release` child ran on ChatGPT subscription, without editing.
Its final source/evidence review found no blocking defects and explicitly
retained physical-test and mocked-transport limits. The lead ran commands,
inspected images and assembled artifacts; the child did not claim those checks.

Changed: `tests/test_network.c`, native-resolution UI tests and CMake wiring,
configuration-error instructions and safe field propagation, current release
documentation, six external-workflow screenshots, `dist/weather-station/`
and the future-task list. The configuration-error notice now explains whole-file
replacement/restart and suppresses lunar phase when the selected timezone is
not validated.

Evidence:

- Clean firmware build: exit 0, 180.88 s; final UI rebuild: exit 0, 9.87 s.
- Final firmware: 1,817,200 bytes; static RAM 62,376 bytes; app flash 1,816,806 bytes.
- Fresh ASan/UBSan desktop build: full 14 registered checks passed.
- A native 1024x600 pointer suite was then added; both affected UI suites passed
  after the final correction. All 15 current registered checks have passing evidence.
- Five deterministic production-worker groups cover retry/429, timeout, bad
  body, TLS failure, duplicate refresh, spurious wake, cache restart/location
  and body-size boundary.
- Six actual external-file preview startups exited 0, including first offline,
  real cached offline and changed location; images were visually inspected.
- Provisioning shell syntax and native-generator patch checks passed.

The user subsequently installed another project on the physical board. Its
different 7 MiB factory partition table explains the observed missing-config
partition. Final QA performs no further board writes. The latest image's
UI-only corrections are validated through the shared desktop renderer and
device compilation, not claimed as physically tested.

Current procedures: `docs/deployment.md`; portable display settings:
`docs/board-handoff.md`; complete check/limitation ledger: `docs/release.md`
and `docs/acceptance.md`. The public binary bundle excludes secrets and includes
SHA-256 checksums and third-party license texts. OTA is future work only.

# Acceptance evidence

This ledger follows `plan.md` and the user's external-configuration correction.
An unexecuted check is not a pass. The board was tested, temporarily repurposed,
and then the final weather firmware was reinstalled with explicit user approval.
Full commands, test chronology and physical limits are recorded in `docs/release.md`.

| Requirement | Evidence or remaining check |
| --- | --- |
| Sourced LCD-5 profile, distinguish LCD-5B | `include/board_profile.h`, `docs/hardware.md` |
| Pinned ESP-IDF and same LVGL on both targets | `platformio.ini`, `firmware/main/idf_component.yml`, `dependencies.lock`, root `CMakeLists.txt` |
| Final firmware compiles | Clean `waveshare-lcd5b` build exit 0; final incremental UI rebuild exit 0; binary 1,817,200 bytes |
| Desktop compiles and renders | Ordinary build and fresh sanitized `build-release` passed; same shared UI and external schema |
| Physical orientation, colors, backlight and corner touch | User confirmed visible weather screen; corner calibration and controlled color/backlight tests remain unverified |
| Internal/PSRAM allocation documented | `docs/hardware.md`, `docs/board-handoff.md`, actual post-Wi-Fi figures in `docs/release.md`; peak HTTPS heap not measured |
| Forecast HTTPS query, correct units and pressure | `weather-fixture`; URL requests metric/m/s and `pressure_msl`; real device and desktop HTTP 200 |
| Typed model independent of LVGL | `components/weather_data/include/weather_data.h`, six data test groups |
| Six future hours and five daily rows | Fixture/time selection tests; inspected shared 800x480 and 1024x600 renders |
| Local-date/DST/midnight correctness | `weather-time`, `weather-fixture`; absolute-time ordering and selected-location TZ |
| Hourly day/night icons | Fixture `is_day` assertions and inspected day/night/icon-gallery renders |
| Null, missing, unknown WMO, mismatched arrays, malformed JSON | `weather-incomplete`, `weather-malformed`; worker bad-response scenario retains previous snapshot |
| Bound response size, request timeout and TLS validation | `weather-network-boundaries`, mocked transport option assertions, production read-loop review and historical real HTTPS success; hardware fault injection not run |
| Refresh interval, manual exclusion and retries including 429 | `weather-policy`, `weather-network-cycle`, both SDL refresh tests |
| Location/schema-bound cache and interrupted-write recovery | `weather-cache`; cached/offline and changed-location worker tests; atomic LittleFS fsync/rename backend |
| Cache write throttling and freshness indication | Policy tests, persisted worker snapshot, UI stale/offline states |
| Lunar phase method, local date and known-event tests | SunCalc method with retained license; `weather-time` known new/full moon checks; invalid config suppresses phase |
| Deterministic fixture, explicit demo label, no live/demo mixing | `fixtures/demo.json`; UI demo prefix; external explicit-demo process; live workers never load fixture |
| Russian UI, Cyrillic, degree, minus and units | Generated font glyph assertions in both UI suites, inspected stress renders |
| Reference composition, artwork provenance, theme and typography | `DESIGN.md`, `assets/README.md`, font OFL, original generated landscape, inspected screenshots |
| Current weather, 7 hourly cells, 5 daily rows, sun/moon | `docs/screenshots/day.png`, `native-1024x600.png`, `cached-offline-1024.png` |
| Hour/day detail cards and return navigation | Actual SDL pointer and Escape/Back scenarios at 800x480 and 1024x600 |
| Loading, updating, no-cache error, offline/cache, stale states | UI render tests plus external preview startup/offline processes |
| Main screen fits 800 x 480 | 800x480 UI captures; native UI scales to 1000x600 centered on 1024x600 |
| Long place name, negative/3-digit temperature, long condition, large font | Both UI stress suites and inspected captures |
| One UI context, in-place updates, selected card survives refresh | SDL ownership/selection tests; worker communicates by value, no LVGL calls |
| One external `config.json`, immutable until restart | Strict shared `weather_config` parser; both entry points read once; no per-setting overrides |
| Missing/invalid configuration shows actionable message | Strict parser cases, actual external invalid-type startup, safe field name and whole-file/restart notice |
| No settings screen, portal, BLE or settings command interface | UI/navigation and entry-point source review; About Data is read-only |
| Network/location/brightness change via file without recompilation | Private Wi-Fi file boot succeeded on hardware; separate preview processes applied changed name/brightness/large text; alternate AP and physical brightness remain unverified |
| Secrets absent from logs, source and published artifacts | Private settings/images outside repository; public bundle allowlist and credential-value scan recorded in release manifest audit |
| Network/time sync/JSON/cache work outside UI handlers | Firmware FreeRTOS worker/semaphore/result queue and desktop worker/mailbox; independent review |
| One owner per forecast snapshot; no old-location application | UI copies input; worker tests reject old-location cache on new-config startup |
| Wi-Fi reconnect, slow response keeps touch responsive | Worker architecture and desktop timeout/recovery/refresh tests; physical AP interruption/touch-under-TLS not run |
| First run offline, cache restart, disconnect, recovery, timeout, 429 | Five deterministic production desktop worker groups plus actual preview processes using an unreachable proxy; radio-specific behavior unverified |
| Firmware and preview build from same project | Clean device build and fresh sanitized desktop build, shared LVGL/model/config components |
| Day, night, offline and stress screenshots | `docs/screenshots/`, plus six external-config/native workflow captures |
| Clean build and full tests from documented commands | Clean firmware build passed; publication check ran all 15 CTest groups together with ASan/UBSan: all passed |
| Flashable artifacts, instructions, licenses and limitations | `dist/weather-station/`, `docs/deployment.md`, `docs/board-handoff.md`, `docs/release.md`, license bundle |

## Sequential agent execution

Delegation was attempted for hardware-base using real task mechanisms.
Those launches stalled without source changes; the lead completed the stage
and recorded this in `docs/handoff.md`. The failed Anthropic launch was cancelled.
Only ChatGPT subscription models are allowed for later agent work.
Stages must not overlap as active writing agents.

## Final release audit

Software checks and independently reviewed code are complete. The final image
was installed after fresh authorization and received live weather over HTTPS.
Long-duration physical testing and radio-specific fault injection remain
unverified, as detailed in `docs/release.md`.
OTA was requested as future work and is tracked in `tasks/todo.md`; no OTA
support is claimed for the current release.

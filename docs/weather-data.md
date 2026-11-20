# Weather data contract

`components/weather_data/include/weather_data.h` is independent of LVGL and
ESP-IDF. Missing numeric values are `NAN`, unknown daylight is `-1`, unknown WMO
codes map to `WEATHER_UNKNOWN`, missing instants are zero. A genuine 0% remains
numeric zero. Snapshots own their arrays and copy their location.

## Open-Meteo request

The HTTPS Forecast endpoint is `https://api.open-meteo.com/v1/forecast`.
The URL builder requests five days, Unix timestamps, metric temperatures and
wind in m/s. Five forecast days also cover six future hours when starting late
at night. Current temperature, apparent temperature, relative humidity, weather
code, `is_day` and **mean-sea-level pressure** (`pressure_msl`, hPa) are requested.
Hourly fields include temperature, code, precipitation probability, wind speed
and `is_day`; daily fields include minimum/maximum temperature, code, maximum
precipitation probability, sunrise and sunset.

The request uses the configured IANA timezone. The external configuration must
also supply its matching POSIX timezone rule for libc formatting on ESP32.
Default: `Europe/Kyiv` and `EET-2EEST,M3.5.0/3,M10.5.0/4`.
Both fields describe the selected location, not the desktop host timezone.
Changing coordinates to a different time zone requires updating both fields.
Future legal timezone-rule changes require an external-file update.

The process selects TZ once at boot, before workers start. All ordering uses
absolute Unix instants; each displayed instant goes through `localtime_r`,
including seasonal offset changes. The response's one `utc_offset_seconds`
value is not reused for the entire forecast. Hourly day/night uses that hour's
API `is_day`; missing daylight remains unknown.

## Parser and storage

JSON is bounded to 65,536 bytes. Missing fields, null and short companion arrays
become missing measurements. Invalid JSON, absent current timestamp, excessive
array counts and non-increasing timestamp arrays are rejected without changing
the last valid snapshot. Hour and day capacities are 144 and 7.

Cache files contain a versioned header and original JSON. CRC32 protects both
header metadata and body; location identity covers coordinates, name and both
timezone representations. Writes flush and fsync a temporary file, rotate the
previous record to `.bak`, then rename the new file. A truncated/corrupt current
record falls back to the previous valid record. Incomplete `.tmp` files are
ignored. Demo snapshots cannot enter this cache.

The integration layer must invoke the provided persist-interval gate before
writing. The cache is an operational record, never a settings source.

## Request policy and astronomy

The policy serializes refreshes, keeps at least 60 seconds between successful
manual requests, defaults automatic refresh to 900 seconds and backs temporary
failures off from 60 seconds to a maximum of 3,600 seconds. A later Retry-After
deadline for HTTP 429 wins. Both delta-seconds and IMF-fixdate are parsed without
changing the selected location timezone.

Lunar phase uses SunCalc 1.9.0's orbital approximation, based on Meeus and NASA
illumination formulas, evaluated at noon on the selected location's local date.
It is a phase category approximation, not an astronomical ephemeris. Tests
check the January 11, 2024 new moon and January 25, 2024 full moon, within 0.035
of a cycle. The phase is geocentric; the local date determines its displayed
day. See `docs/third-party-licenses.md` for the retained BSD-2-Clause notice.

## Verification

Six CTest groups cover fixture selection/freshness, incomplete and unknown data,
malformed input/size bounds, Kyiv spring/fall DST and midnight, lunar events,
refresh/backoff/429, flash-write throttling, cache location binding, CRC-protected
metadata, interrupted temporary writes and backup recovery.

All six passed after six deliberate independent production mutations were
shown to make each corresponding group fail. Mutations were then removed and
all six passed again. Firmware compilation also passed with ESP-IDF 5.4.0.

The desktop dependency cJSON 1.7.18 emits its upstream CMake compatibility
deprecation warning; it is not suppressed. HTTP/TLS transport and Wi-Fi belong
to firmware-integration and are not yet implemented by this component.

#include "weather_data.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float number(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) ?
           (float)item->valuedouble : NAN;
}

static float element(const cJSON *object, const char *key, size_t index)
{
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(object, key);
    const cJSON *item = cJSON_GetArrayItem(array, (int)index);
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) ?
           (float)item->valuedouble : NAN;
}

static time_t instant(const cJSON *item)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
        item->valuedouble < 946684800 || item->valuedouble > 4102444800.0 ||
        floor(item->valuedouble) != item->valuedouble) return 0;
    return (time_t)item->valuedouble;
}

static float probability(float value)
{
    return value >= 0 && value <= 100 ? value : NAN;
}

weather_condition_t weather_condition(int wmo)
{
    switch (wmo) {
    case 0: return WEATHER_CLEAR;
    case 1: case 2: return WEATHER_PARTLY_CLOUDY;
    case 3: return WEATHER_CLOUDY;
    case 45: case 48: return WEATHER_FOG;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: return WEATHER_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WEATHER_SNOW;
    case 95: case 96: case 99: return WEATHER_THUNDER;
    default: return WEATHER_UNKNOWN;
    }
}

static weather_condition_t condition(float code)
{
    return isfinite(code) && code >= 0 && code <= 999 && floorf(code) == code ?
           weather_condition((int)code) : WEATHER_UNKNOWN;
}

static int8_t day_flag(float flag)
{
    return flag == 0 ? 0 : flag == 1 ? 1 : -1;
}

bool weather_parse(const char *json, weather_snapshot_t *snapshot, time_t fetched_at)
{
    if (strnlen(json, WEATHER_MAX_RESPONSE + 1) > WEATHER_MAX_RESPONSE) return false;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithOpts(json, &end, true);
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return false; }
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const time_t current_time = instant(cJSON_GetObjectItemCaseSensitive(current, "time"));
    if (!current_time) { cJSON_Delete(root); return false; }
    weather_snapshot_t parsed = {.location = snapshot->location,
        .location_id = weather_location_id(&snapshot->location), .fetched_at = fetched_at};
    parsed.current = (weather_current_t){
        .time = current_time, .temperature = number(current, "temperature_2m"),
        .feels_like = number(current, "apparent_temperature"),
        .humidity = probability(number(current, "relative_humidity_2m")),
        .pressure = number(current, "pressure_msl"),
        .condition = condition(number(current, "weather_code")),
        .is_day = day_flag(number(current, "is_day")),
    };
    const cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    const cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    const int hours = cJSON_GetArraySize(times);
    if (hours > WEATHER_MAX_HOURS) { cJSON_Delete(root); return false; }
    for (int i = 0; i < hours; ++i) {
        const time_t when = instant(cJSON_GetArrayItem(times, i));
        if (!when || (i > 0 && when <= parsed.hours[i - 1].time)) {
            cJSON_Delete(root); return false;
        }
        parsed.hours[i] = (weather_hour_t){
            .time = when, .temperature = element(hourly, "temperature_2m", i),
            .precipitation = probability(element(hourly, "precipitation_probability", i)),
            .wind = element(hourly, "wind_speed_10m", i),
            .condition = condition(element(hourly, "weather_code", i)),
            .is_day = day_flag(element(hourly, "is_day", i)),
        };
        ++parsed.hour_count;
    }
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    times = cJSON_GetObjectItemCaseSensitive(daily, "time");
    const int days = cJSON_GetArraySize(times);
    if (days > WEATHER_MAX_DAYS) { cJSON_Delete(root); return false; }
    for (int i = 0; i < days; ++i) {
        const time_t when = instant(cJSON_GetArrayItem(times, i));
        if (!when || (i > 0 && when <= parsed.days[i - 1].time)) {
            cJSON_Delete(root); return false;
        }
        parsed.days[i] = (weather_day_t){
            .time = when,
            .sunrise = instant(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(daily, "sunrise"), i)),
            .sunset = instant(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(daily, "sunset"), i)),
            .minimum = element(daily, "temperature_2m_min", i),
            .maximum = element(daily, "temperature_2m_max", i),
            .precipitation = probability(element(daily, "precipitation_probability_max", i)),
            .condition = condition(element(daily, "weather_code", i)),
        };
        ++parsed.day_count;
    }
    *snapshot = parsed;
    cJSON_Delete(root);
    return true;
}

weather_location_t weather_default_location(void)
{
    return (weather_location_t){.name = "Киев", .latitude = 50.45, .longitude = 30.523,
        .timezone = "Europe/Kyiv", .posix_timezone = "EET-2EEST,M3.5.0/3,M10.5.0/4"};
}

uint64_t weather_location_id(const weather_location_t *location)
{
    char canonical[512];
    snprintf(canonical, sizeof(canonical), "%.6f,%.6f|%s|%s|%s",
        location->latitude, location->longitude, location->timezone,
        location->posix_timezone, location->name);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)canonical; *p; ++p) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

bool weather_build_url(const weather_location_t *location, char *url, size_t size)
{
    for (const char *p = location->timezone; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '/' || *p == '_' || *p == '+' || *p == '-'))
            return false;
    char zone[192];
    size_t used = 0;
    for (const char *p = location->timezone; *p; ++p) {
        if (*p == '+') { memcpy(zone + used, "%2B", 3); used += 3; }
        else zone[used++] = *p;
    }
    zone[used] = '\0';
    const int length = snprintf(url, size,
        "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,pressure_msl"
        "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,is_day"
        "&daily=weather_code,temperature_2m_min,temperature_2m_max,precipitation_probability_max,sunrise,sunset"
        "&forecast_days=5&timeformat=unixtime&wind_speed_unit=ms&timezone=%s",
        location->latitude, location->longitude, zone);
    return length > 0 && (size_t)length < size;
}

#include "weather_config.h"
#include "weather_config_internal.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int alpha(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static bool hexadecimal(unsigned char c)
{
    return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static const cJSON *get(const cJSON *object, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(object, key);
}
static bool shape(const cJSON *object, const char *const *keys, unsigned count)
{
    unsigned seen = 0;
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON *v = object->child; v; v = v->next) {
        unsigned i;
        for (i = 0; i < count; ++i)
            if (v->string && !strcmp(v->string, keys[i])) break;
        if (i == count || (seen & (1u << i))) return false;
        seen |= 1u << i;
    }
    return seen == (1u << count) - 1;
}
static bool text(const cJSON *object, const char *key, char *dst, size_t capacity, bool empty)
{
    const cJSON *v = get(object, key);
    if (!cJSON_IsString(v) || !v->valuestring) return false;
    const size_t n = strlen(v->valuestring);
    if (n >= capacity || (!empty && !n)) return false;
    memcpy(dst, v->valuestring, n + 1);
    return true;
}
static bool integer(const cJSON *v, unsigned low, unsigned high, unsigned *out)
{
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
        v->valuedouble < low || v->valuedouble > high ||
        v->valuedouble != (unsigned)v->valuedouble) return false;
    *out = (unsigned)v->valuedouble;
    return true;
}
static bool coordinate(const cJSON *v, double limit, double *out)
{
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
        v->valuedouble < -limit || v->valuedouble > limit) return false;
    *out = v->valuedouble;
    return true;
}
static bool hostname(const char *p)
{
    unsigned label = 0;
    char previous = 0;
    for (; *p; previous = *p++) {
        if (*p == '.') {
            if (!label || previous == '-') return false;
            label = 0;
        } else {
            if (!alpha(*p) && !digit(*p) && *p != '-') return false;
            if ((!label && *p == '-') || ++label > 63) return false;
        }
    }
    return previous && previous != '-';
}
static bool timezone_name(const char *p)
{
    bool slash = false, segment = false;
    if (!strcmp(p, "UTC")) return true;
    for (; *p; ++p) {
        if (*p == '/') {
            if (!segment) return false;
            slash = true; segment = false;
        } else {
            if (!alpha(*p) && !digit(*p) && !strchr("_+-", *p)) return false;
            segment = true;
        }
    }
    return slash && segment;
}
static bool number(const char **cursor, unsigned low, unsigned high)
{
    const char *p = *cursor;
    unsigned value = 0;
    if (!digit(*p)) return false;
    do {
        const unsigned d = (unsigned)(*p++ - '0');
        if (value > high / 10 || (value == high / 10 && d > high % 10)) return false;
        value = value * 10 + d;
    } while (digit(*p));
    if (value < low) return false;
    *cursor = p;
    return true;
}
static bool clock_value(const char **cursor, bool signed_value)
{
    const char *p = *cursor, *hours;
    if (signed_value && (*p == '+' || *p == '-')) ++p;
    hours = p;
    if (!number(&p, 0, 24)) return false;
    unsigned hour = 0;
    while (hours < p) hour = hour * 10 + (unsigned)(*hours++ - '0');
    for (unsigned i = 0; i < 2 && *p == ':'; ++i) {
        ++p;
        if (!number(&p, 0, hour == 24 ? 0 : 59)) return false;
    }
    *cursor = p;
    return true;
}
static bool abbreviation(const char **cursor)
{
    const char *p = *cursor;
    while (alpha(*p)) ++p;
    if (p - *cursor < 3) return false;
    *cursor = p;
    return true;
}
static bool rule(const char **cursor)
{
    const char *p = *cursor;
    if (*p++ != 'M' || !number(&p, 1, 12) || *p++ != '.' ||
        !number(&p, 1, 5) || *p++ != '.' || !number(&p, 0, 6)) return false;
    if (*p == '/') { ++p; if (!clock_value(&p, false)) return false; }
    *cursor = p;
    return true;
}
static bool posix_timezone(const char *p)
{
    if (!abbreviation(&p) || !clock_value(&p, true)) return false;
    if (!*p) return true;
    if (!abbreviation(&p)) return false;
    if (*p != ',' && !clock_value(&p, true)) return false;
    return *p++ == ',' && rule(&p) && *p++ == ',' && rule(&p) && !*p;
}
static bool password(const char *p)
{
    const size_t n = strlen(p);
    if (!n || (n >= 8 && n <= 63)) return true;
    if (n != 64) return false;
    for (; *p; ++p) if (!hexadecimal(*p)) return false;
    return true;
}

bool weather_config_parse(const char *json, weather_config_t *out, char error_field[80])
{
    static const char *const root_keys[] = {"schema_version","mode","wifi","location","display","weather"};
    static const char *const wifi_keys[] = {"ssid","password","ntp_server"};
    static const char *const location_keys[] = {"name","latitude","longitude","timezone","posix_timezone"};
    static const char *const display_keys[] = {"brightness","large_text"};
    static const char *const weather_keys[] = {"update_seconds","request_timeout_ms","stale_after_seconds","cache_write_seconds"};
    weather_config_t cfg = {0};
    cJSON *root = NULL;
    const char *bad = "json", *end = NULL;
    size_t length = 0;
    unsigned version;
#define REQUIRE(test, field) do { if (!(test)) { bad = field; goto done; } } while (0)
#define COPY(obj, key, dst, empty) REQUIRE(text(obj, key, dst, sizeof(dst), empty), #obj "." key)
    REQUIRE(json && out, "json");
    while (length <= 8192 && json[length]) ++length;
    REQUIRE(length && length <= 8192 && weather_config_lexical_valid(json), "json");
    root = cJSON_ParseWithLengthOpts(json, length + 1, &end, true);
    REQUIRE(root && end == json + length, "json");
    REQUIRE(shape(root, root_keys, 6), "json");
    REQUIRE(integer(get(root, "schema_version"), 1, 1, &version), "schema_version");
    const cJSON *mode = get(root, "mode");
    REQUIRE(cJSON_IsString(mode) && mode->valuestring, "mode");
    if (!strcmp(mode->valuestring, "weather")) cfg.mode = WEATHER_MODE_LIVE;
    else if (!strcmp(mode->valuestring, "demo")) cfg.mode = WEATHER_MODE_DEMO;
    else if (!strcmp(mode->valuestring, "diagnostics")) cfg.mode = WEATHER_MODE_DIAGNOSTICS;
    else REQUIRE(false, "mode");
    const cJSON *wifi = get(root, "wifi"), *location = get(root, "location");
    const cJSON *display = get(root, "display"), *weather = get(root, "weather");
    REQUIRE(shape(wifi, wifi_keys, 3), "wifi");
    REQUIRE(shape(location, location_keys, 5), "location");
    REQUIRE(shape(display, display_keys, 2), "display");
    REQUIRE(shape(weather, weather_keys, 4), "weather");
    COPY(wifi, "ssid", cfg.wifi_ssid, true);
    COPY(wifi, "password", cfg.wifi_password, true);
    COPY(wifi, "ntp_server", cfg.ntp_server, false);
    REQUIRE(cfg.mode != WEATHER_MODE_LIVE || cfg.wifi_ssid[0], "wifi.ssid");
    REQUIRE(password(cfg.wifi_password), "wifi.password");
    REQUIRE(hostname(cfg.ntp_server), "wifi.ntp_server");
    COPY(location, "name", cfg.location.name, false);
    COPY(location, "timezone", cfg.location.timezone, false);
    COPY(location, "posix_timezone", cfg.location.posix_timezone, false);
    REQUIRE(coordinate(get(location, "latitude"), 90, &cfg.location.latitude), "location.latitude");
    REQUIRE(coordinate(get(location, "longitude"), 180, &cfg.location.longitude), "location.longitude");
    REQUIRE(timezone_name(cfg.location.timezone), "location.timezone");
    REQUIRE(posix_timezone(cfg.location.posix_timezone), "location.posix_timezone");
    REQUIRE(integer(get(display, "brightness"), 0, 100, &cfg.brightness), "display.brightness");
    REQUIRE(cJSON_IsBool(get(display, "large_text")), "display.large_text");
    cfg.large_text = cJSON_IsTrue(get(display, "large_text"));
    REQUIRE(integer(get(weather, "update_seconds"), 60, 86400, &cfg.update_seconds), "weather.update_seconds");
    REQUIRE(integer(get(weather, "request_timeout_ms"), 1000, 60000, &cfg.request_timeout_ms), "weather.request_timeout_ms");
    REQUIRE(integer(get(weather, "stale_after_seconds"), cfg.update_seconds, 604800, &cfg.stale_after_seconds), "weather.stale_after_seconds");
    REQUIRE(integer(get(weather, "cache_write_seconds"), 60, 86400, &cfg.cache_write_seconds), "weather.cache_write_seconds");
    *out = cfg;
    bad = NULL;
done:
    cJSON_Delete(root);
    if (error_field) {
        if (bad) strcpy(error_field, bad);
        else error_field[0] = '\0';
    }
    return bad == NULL;
#undef COPY
#undef REQUIRE
}

bool weather_config_load(const char *path, weather_config_t *out, char error_field[80])
{
    FILE *file = fopen(path, "rb");
    if (!file) { strcpy(error_field, "file"); return false; }
    char json[8194];
    const size_t size = fread(json, 1, sizeof(json) - 1, file);
    const bool valid = !ferror(file) && size <= 8192;
    fclose(file);
    json[size] = '\0';
    if (!valid || strlen(json) != size) { strcpy(error_field, "json"); return false; }
    return weather_config_parse(json, out, error_field);
}

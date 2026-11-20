#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define WEATHER_MAX_RESPONSE 65536
#define WEATHER_MAX_HOURS 144
#define WEATHER_MAX_DAYS 7

typedef enum {
    WEATHER_CLEAR, WEATHER_PARTLY_CLOUDY, WEATHER_CLOUDY, WEATHER_FOG,
    WEATHER_RAIN, WEATHER_SNOW, WEATHER_THUNDER, WEATHER_UNKNOWN
} weather_condition_t;

typedef struct {
    char name[128];
    double latitude, longitude;
    char timezone[64];
    char posix_timezone[96];
} weather_location_t;

typedef struct {
    time_t time;
    float temperature, feels_like, humidity, pressure;
    weather_condition_t condition;
    int8_t is_day;
} weather_current_t;

typedef struct {
    time_t time;
    float temperature, precipitation, wind;
    weather_condition_t condition;
    int8_t is_day;
} weather_hour_t;

typedef struct {
    time_t time, sunrise, sunset;
    float minimum, maximum, precipitation;
    weather_condition_t condition;
} weather_day_t;

typedef struct {
    weather_location_t location;
    uint64_t location_id;
    time_t fetched_at;
    bool demo;
    weather_current_t current;
    size_t hour_count, day_count;
    weather_hour_t hours[WEATHER_MAX_HOURS];
    weather_day_t days[WEATHER_MAX_DAYS];
} weather_snapshot_t;

typedef struct {
    bool busy;
    unsigned failures, interval_seconds;
    time_t next_allowed, next_auto, last_persisted;
} weather_policy_t;

weather_location_t weather_default_location(void);
uint64_t weather_location_id(const weather_location_t *location);
bool weather_timezone_apply(const weather_location_t *location);
bool weather_parse(const char *json, weather_snapshot_t *snapshot, time_t fetched_at);
weather_condition_t weather_condition(int wmo);
size_t weather_future_hours(const weather_snapshot_t *snapshot, time_t now, size_t indices[6]);
size_t weather_visible_days(const weather_snapshot_t *snapshot, time_t now, size_t indices[5]);
const weather_day_t *weather_today(const weather_snapshot_t *snapshot, time_t now);
bool weather_is_stale(const weather_snapshot_t *snapshot, time_t now, unsigned maximum_age);
void weather_clock(time_t instant, char result[6]);
double weather_moon_phase(time_t selected_local_date);
bool weather_build_url(const weather_location_t *location, char *url, size_t size);
bool weather_cache_save(const char *path, const weather_snapshot_t *snapshot, const char *json);
bool weather_cache_load(const char *path, weather_snapshot_t *snapshot);
bool weather_policy_begin(weather_policy_t *policy, time_t now);
void weather_policy_finish(weather_policy_t *policy, time_t now, int http_status, time_t retry_after);
time_t weather_retry_after(const char *header, time_t now);
bool weather_policy_persist_due(const weather_policy_t *policy, time_t now, unsigned minimum_interval);

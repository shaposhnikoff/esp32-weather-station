#include "weather_data.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static char *read_fixture(const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    const long size = ftell(file);
    CHECK(size > 0 && size <= WEATHER_MAX_RESPONSE);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    CHECK(text);
    CHECK(fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    CHECK(fclose(file) == 0);
    return text;
}

static weather_snapshot_t snapshot(const char *json)
{
    weather_snapshot_t value = {.location = weather_default_location()};
    CHECK(weather_parse(json, &value, 1784569500));
    return value;
}

static void fixture_contract(const char *json)
{
    const weather_snapshot_t value = snapshot(json);
    CHECK(value.current.temperature == 23 && value.current.condition == WEATHER_CLEAR);
    CHECK(value.hour_count == 11 && value.day_count == 5);
    CHECK(value.days[0].precipitation == 0 && isnan(value.days[1].precipitation));
    CHECK(value.hours[1].is_day == 1 && value.hours[2].is_day == 0);
    size_t hours[6], days[5];
    CHECK(weather_future_hours(&value, value.current.time, hours) == 6);
    CHECK(hours[0] == 1 && hours[5] == 6);
    CHECK(weather_future_hours(&value, 1784580600, hours) == 6);
    CHECK(value.hours[hours[0]].time == 1784581200);
    CHECK(weather_future_hours(&value, 1784581200, hours) == 6);
    CHECK(value.hours[hours[0]].time == 1784584800);
    CHECK(weather_visible_days(&value, value.current.time, days) == 5);
    CHECK(weather_today(&value, value.current.time)->sunset == 1784570520);
    CHECK(!weather_is_stale(&value, value.fetched_at + 21600, 21600));
    CHECK(weather_is_stale(&value, value.fetched_at + 21601, 21600));
    CHECK(weather_future_hours(&value, 1784700000, hours) == 0);
}

static void incomplete_and_unknown(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
    cJSON *current = cJSON_GetObjectItem(root, "current");
    cJSON_ReplaceItemInObject(current, "weather_code", cJSON_CreateNumber(999));
    cJSON_DeleteItemFromObject(current, "apparent_temperature");
    cJSON_ReplaceItemInArray(cJSON_GetObjectItem(hourly, "temperature_2m"), 1, cJSON_CreateNull());
    cJSON_ReplaceItemInObject(hourly, "wind_speed_10m", cJSON_CreateArray());
    char *partial = cJSON_PrintUnformatted(root);
    const weather_snapshot_t value = snapshot(partial);
    CHECK(value.current.condition == WEATHER_UNKNOWN);
    CHECK(isnan(value.current.feels_like) && isnan(value.hours[1].temperature));
    CHECK(isnan(value.hours[10].wind));
    CHECK(value.hour_count == 11);
    free(partial);
    cJSON_Delete(root);
}

static void malformed_keeps_valid_snapshot(const char *json)
{
    weather_snapshot_t value = snapshot(json);
    const char *invalid[] = {"{", "{}", "[]", "{\"current\":{\"time\":null}}",
        "{\"current\":{\"time\":1784569500}} garbage"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(!weather_parse(invalid[i], &value, value.fetched_at));
        CHECK(value.current.temperature == 23);
    }
    char *large = malloc(WEATHER_MAX_RESPONSE + 2);
    CHECK(large);
    memset(large, ' ', WEATHER_MAX_RESPONSE + 1);
    large[WEATHER_MAX_RESPONSE + 1] = '\0';
    CHECK(!weather_parse(large, &value, value.fetched_at));
    free(large);
}

static void local_time_and_moon(void)
{
    const struct { time_t instant; const char *clock; } cases[] = {
        {1774742400, "02:00"}, {1774746000, "04:00"},
        {1792886400, "03:00"}, {1792890000, "03:00"},
        {1784581200, "00:00"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char clock[6];
        weather_clock(cases[i].instant, clock);
        CHECK(strcmp(clock, cases[i].clock) == 0);
    }
    const double new_moon = weather_moon_phase(1704967200);
    CHECK(new_moon < 0.035 || new_moon > 0.965);
    CHECK(fabs(weather_moon_phase(1706176800) - 0.5) < 0.035);
}

static void request_policy(void)
{
    weather_policy_t policy = {.interval_seconds = 900};
    CHECK(weather_policy_begin(&policy, 1000));
    CHECK(!weather_policy_begin(&policy, 1000));
    weather_policy_finish(&policy, 1000, 429, 1600);
    CHECK(!weather_policy_begin(&policy, 1599));
    CHECK(weather_policy_begin(&policy, 1600));
    weather_policy_finish(&policy, 1600, 0, 0);
    CHECK(policy.next_allowed == 1720);
    CHECK(weather_policy_begin(&policy, 1720));
    weather_policy_finish(&policy, 1720, 200, 0);
    CHECK(policy.next_allowed == 1780 && policy.next_auto == 2620 && !policy.busy);
    CHECK(weather_retry_after("120", 1000) == 1120);
    CHECK(weather_retry_after("Thu, 11 Jan 2024 10:00:00 GMT", 1000) == 1704967200);
    CHECK(weather_retry_after("invalid", 1000) == 0);
    policy.last_persisted = 1000;
    CHECK(!weather_policy_persist_due(&policy, 2799, 1800));
    CHECK(weather_policy_persist_due(&policy, 2800, 1800));
}

static void cache_recovery_and_location(const char *json)
{
    char directory[] = "/tmp/weather-tests-XXXXXX";
    CHECK(mkdtemp(directory));
    char path[256], backup[260], temporary[260];
    snprintf(path, sizeof(path), "%s/cache", directory);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    weather_snapshot_t source = snapshot(json), loaded = {.location = source.location};
    CHECK(weather_cache_save(path, &source, json));
    CHECK(weather_cache_load(path, &loaded));
    CHECK(loaded.fetched_at == source.fetched_at && loaded.current.temperature == 23);
    source.fetched_at += 900;
    CHECK(weather_cache_save(path, &source, json));
    FILE *file = fopen(temporary, "wb");
    CHECK(file && fputs("interrupted temporary write", file) >= 0 && fclose(file) == 0);
    CHECK(weather_cache_load(path, &loaded) && loaded.fetched_at == source.fetched_at);
    file = fopen(path, "r+b");
    CHECK(file && fseek(file, 24, SEEK_SET) == 0);
    const int original = fgetc(file);
    CHECK(original != EOF && fseek(file, 24, SEEK_SET) == 0);
    CHECK(fputc(original ^ 1, file) != EOF && fclose(file) == 0);
    CHECK(weather_cache_load(path, &loaded) && loaded.fetched_at == 1784569500);
    file = fopen(path, "wb");
    CHECK(file && fputs("interrupted write", file) >= 0 && fclose(file) == 0);
    CHECK(weather_cache_load(path, &loaded));
    CHECK(loaded.fetched_at == 1784569500);
    loaded.location.latitude = 48.85;
    CHECK(!weather_cache_load(path, &loaded));
    source.demo = true;
    CHECK(!weather_cache_save(path, &source, json));
    CHECK(unlink(path) == 0 && unlink(backup) == 0 && unlink(temporary) == 0);
    CHECK(rmdir(directory) == 0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2 || argc == 3);
    const weather_location_t location = weather_default_location();
    CHECK(weather_timezone_apply(&location));
    char *json = read_fixture(argv[1]);
    const char *group = argc == 3 ? argv[2] : "all";
    if (strcmp(group, "all") == 0 || strcmp(group, "fixture") == 0) fixture_contract(json);
    if (strcmp(group, "all") == 0 || strcmp(group, "incomplete") == 0) incomplete_and_unknown(json);
    if (strcmp(group, "all") == 0 || strcmp(group, "malformed") == 0) malformed_keeps_valid_snapshot(json);
    if (strcmp(group, "all") == 0 || strcmp(group, "time") == 0) local_time_and_moon();
    if (strcmp(group, "all") == 0 || strcmp(group, "policy") == 0) request_policy();
    if (strcmp(group, "all") == 0 || strcmp(group, "cache") == 0) cache_recovery_and_location(json);
    char url[1200];
    CHECK(weather_build_url(&location, url, sizeof(url)));
    CHECK(!weather_build_url(&location, url, 8));
    free(json);
    puts("PASS: parser, missing/unknown data, midnight/DST, moon, policy, cache recovery/location");
    return 0;
}

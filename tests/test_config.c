#include "weather_config.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rejected(const char *text)
{
    weather_config_t output, original;
    memset(&output, 0xa5, sizeof(output));
    original = output;
    char error[80];
    assert(!weather_config_parse(text, &output, error));
    assert(error[0] && !memcmp(&output, &original, sizeof(output)));
}

static void replace(cJSON *root, const char *object, const char *key, cJSON *value)
{
    cJSON *parent = object ? cJSON_GetObjectItemCaseSensitive(root, object) : root;
    assert(cJSON_ReplaceItemInObjectCaseSensitive(parent, key, value));
    char *text = cJSON_PrintUnformatted(root);
    assert(text);
    rejected(text);
    free(text);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char text[8194], error[80];
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    const size_t size = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[size] = '\0';
    weather_config_t config;
    assert(weather_config_parse(text, &config, error));
    assert(config.mode == WEATHER_MODE_LIVE && config.update_seconds == 900);
    assert(config.brightness == 100 && config.location.latitude == 50.45);
    assert(weather_config_load(argv[1], &config, error));
    const struct { const char *object, *key, *value; } cases[] = {
        {NULL, "schema_version", "2"},
        {NULL, "mode", "\"portal\""},
        {"wifi", "ssid", "\"\""},
        {"wifi", "password", "\"short\""},
        {"wifi", "ntp_server", "\"https://pool.ntp.org\""},
        {"location", "latitude", "91"},
        {"location", "longitude", "-181"},
        {"location", "timezone", "\"Europe//Kyiv\""},
        {"location", "posix_timezone", "\"EET-2EEST\""},
        {"display", "brightness", "101"},
        {"display", "brightness", "0.5"},
        {"display", "large_text", "1"},
        {"weather", "update_seconds", "59"},
        {"weather", "request_timeout_ms", "60001"},
        {"weather", "stale_after_seconds", "60"},
        {"weather", "cache_write_seconds", "-1"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        cJSON *root = cJSON_Parse(text);
        assert(root);
        replace(root, cases[i].object, cases[i].key, cJSON_Parse(cases[i].value));
        cJSON_Delete(root);
    }
    rejected("{\"schema_version\":01}");
    rejected("{\"schema_version\":1,\"schema_version\":1}");
    rejected("{\"unknown\":1}");
    rejected("{\"text\":\"\\u0000\"}");
    rejected("{\"text\":\"\xc0\x80\"}");
    rejected("[[[[[[[[[[0]]]]]]]]]]");
    rejected("{} trailing");
    puts("weather-config: valid external file and 23 rejection cases passed");
    return 0;
}

#include "weather_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    uint32_t magic, schema, length, crc;
    uint64_t location_id;
    int64_t fetched_at;
} cache_header_t;

static uint32_t crc_update(uint32_t crc, const unsigned char *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
    }
    return crc;
}

static uint32_t cache_crc(const cache_header_t *header, const char *json)
{
    cache_header_t checked = *header;
    checked.crc = 0;
    uint32_t crc = crc_update(UINT32_MAX, (const unsigned char *)&checked, sizeof(checked));
    return ~crc_update(crc, (const unsigned char *)json, header->length);
}

bool weather_cache_save(const char *path, const weather_snapshot_t *snapshot, const char *json)
{
    if (snapshot->demo) return false;
    const size_t length = strnlen(json, WEATHER_MAX_RESPONSE + 1);
    if (length > WEATHER_MAX_RESPONSE) return false;
    char temporary[320], backup[320];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary) ||
        snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup)) return false;
    cache_header_t header = {.magic = UINT32_C(0x57544852), .schema = 1,
        .length = (uint32_t)length,
        .location_id = snapshot->location_id, .fetched_at = snapshot->fetched_at};
    header.crc = cache_crc(&header, json);
    FILE *file = fopen(temporary, "wb");
    if (!file) return false;
    bool valid = fwrite(&header, sizeof(header), 1, file) == 1 &&
                 fwrite(json, 1, length, file) == length && fflush(file) == 0 &&
                 fsync(fileno(file)) == 0;
    if (fclose(file) != 0) valid = false;
    if (!valid) return false;
    if (rename(path, backup) != 0 && access(path, F_OK) == 0) return false;
    return rename(temporary, path) == 0;
}

static bool load_file(const char *path, weather_snapshot_t *snapshot)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    cache_header_t header;
    bool valid = fread(&header, sizeof(header), 1, file) == 1 &&
        header.magic == UINT32_C(0x57544852) && header.schema == 1 &&
        header.location_id == weather_location_id(&snapshot->location) &&
        header.length <= WEATHER_MAX_RESPONSE && header.fetched_at >= 946684800 &&
        header.fetched_at <= INT64_C(4102444800);
    char *json = valid ? malloc((size_t)header.length + 1) : NULL;
    if (json) {
        valid = fread(json, 1, header.length, file) == header.length && fgetc(file) == EOF &&
                cache_crc(&header, json) == header.crc;
        json[header.length] = '\0';
        if (valid) valid = weather_parse(json, snapshot, (time_t)header.fetched_at);
    } else valid = false;
    free(json);
    fclose(file);
    return valid;
}

bool weather_cache_load(const char *path, weather_snapshot_t *snapshot)
{
    if (load_file(path, snapshot)) return true;
    char backup[320];
    if (snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup)) return false;
    return load_file(backup, snapshot);
}

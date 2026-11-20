#include <SDL.h>
#include <curl/curl.h>
#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "weather_config.h"

static atomic_uint_fast64_t simulated_time;
static atomic_uint calls;
static atomic_int response;
static const char *fixture;
static size_t fixture_size;
static curl_write_callback body_callback, header_callback;
static void *body_context, *header_context;
static long status_code, peer_validation, hostname_validation, redirects, deadline;

static Uint64 fake_ticks(void) { return atomic_load(&simulated_time); }

static CURLcode fake_setopt(CURL *client, CURLoption option, ...)
{
    (void)client;
    va_list args;
    va_start(args, option);
    switch (option) {
    case CURLOPT_URL:
        assert(strncmp(va_arg(args, const char *), "https://api.open-meteo.com/", 27) == 0);
        break;
    case CURLOPT_TIMEOUT_MS: deadline = va_arg(args, long); break;
    case CURLOPT_SSL_VERIFYPEER: peer_validation = va_arg(args, long); break;
    case CURLOPT_SSL_VERIFYHOST: hostname_validation = va_arg(args, long); break;
    case CURLOPT_FOLLOWLOCATION: redirects = va_arg(args, long); break;
    case CURLOPT_NOSIGNAL: assert(va_arg(args, long) == 1); break;
    case CURLOPT_WRITEFUNCTION: body_callback = va_arg(args, curl_write_callback); break;
    case CURLOPT_HEADERFUNCTION: header_callback = va_arg(args, curl_write_callback); break;
    case CURLOPT_WRITEDATA: body_context = va_arg(args, void *); break;
    case CURLOPT_HEADERDATA: header_context = va_arg(args, void *); break;
    default: assert(false);
    }
    va_end(args);
    return CURLE_OK;
}

static CURLcode fake_perform(CURL *client)
{
    (void)client;
    atomic_fetch_add(&calls, 1);
    assert(peer_validation == 1 && hostname_validation == 2 && redirects == 0);
    assert(deadline == 15000);
    status_code = 0;
    switch (atomic_load(&response)) {
    case 1: return CURLE_COULDNT_CONNECT;
    case 2: return CURLE_OPERATION_TIMEDOUT;
    case 3: return CURLE_PEER_FAILED_VERIFICATION;
    case 4:
        status_code = 429;
        {
            char header[] = "rEtRy-AfTeR: 300\r\n";
            assert(header_callback(header, 1, strlen(header), header_context) == strlen(header));
        }
        return CURLE_OK;
    case 5:
        status_code = 200;
        assert(body_callback("{invalid", 1, 8, body_context) == 8);
        return CURLE_OK;
    default:
        status_code = 200;
        assert(body_callback((char *)fixture, 1, fixture_size, body_context) == fixture_size);
        return CURLE_OK;
    }
}

static CURLcode fake_getinfo(CURL *client, CURLINFO info, ...)
{
    (void)client;
    assert(info == CURLINFO_RESPONSE_CODE);
    va_list args;
    va_start(args, info);
    *va_arg(args, long *) = status_code;
    va_end(args);
    return CURLE_OK;
}

/* Run the production worker with deterministic transport and monotonic time. */
#undef curl_easy_setopt
#undef curl_easy_getinfo
#define curl_easy_setopt fake_setopt
#define curl_easy_perform fake_perform
#define curl_easy_getinfo fake_getinfo
#define SDL_GetTicks64 fake_ticks
#include "../desktop/network.c"
#undef curl_easy_setopt
#undef curl_easy_perform
#undef curl_easy_getinfo
#undef SDL_GetTicks64

static desktop_result_t wait_state(weather_ui_status_t wanted)
{
    const Uint64 stop = SDL_GetTicks64() + 3000;
    desktop_result_t result;
    while (SDL_GetTicks64() < stop) {
        if (desktop_network_receive(&result) && result.state.status == wanted) return result;
        SDL_Delay(1);
    }
    fprintf(stderr, "Timed out waiting for UI status %d\n", wanted);
    abort();
}

static void wait_pending_ready(void)
{
    const Uint64 stop = SDL_GetTicks64() + 3000;
    while (SDL_GetTicks64() < stop) {
        SDL_LockMutex(mutex);
        const bool ready = pending && mailbox.state.status == WEATHER_UI_READY;
        SDL_UnlockMutex(mutex);
        if (ready) return;
        SDL_Delay(1);
    }
    abort();
}

static void request_at(unsigned seconds, int scenario)
{
    atomic_store(&response, scenario);
    atomic_store(&simulated_time, (uint64_t)seconds * 1000);
    desktop_network_refresh(NULL);
}

static void unchanged(unsigned expected)
{
    SDL_Delay(30);
    assert(atomic_load(&calls) == expected);
}

static void boundaries(void)
{
    char *bytes = malloc(WEATHER_MAX_RESPONSE + 1);
    assert(bytes);
    memset(bytes, 'x', WEATHER_MAX_RESPONSE);
    transfer_t transfer = {.body = bytes};
    char input[1024];
    memset(input, 'a', sizeof(input));
    for (unsigned i = 0; i < WEATHER_MAX_RESPONSE / sizeof(input); ++i)
        assert(receive_body(input, 1, sizeof(input), &transfer) == sizeof(input));
    assert(transfer.used == WEATHER_MAX_RESPONSE && !bytes[WEATHER_MAX_RESPONSE]);
    assert(receive_body(input, 1, 1, &transfer) == 0);
    assert(transfer.used == WEATHER_MAX_RESPONSE);
    char header[] = "Retry-After:\t300\r\n";
    receive_header(header, 1, strlen(header), &transfer);
    assert(!strcmp(transfer.retry, "300"));
    free(bytes);
}

static void live_cycle(const weather_config_t *config, const char *path)
{
    assert(desktop_network_start(config, path));
    wait_state(WEATHER_UI_LOADING);
    wait_state(WEATHER_UI_LOADING);
    wait_pending_ready();
    atomic_store(&simulated_time, 61000);
    desktop_result_t result = wait_state(WEATHER_UI_READY);
    assert(result.valid && !result.snapshot.demo);
    unchanged(1); /* Consuming a late mailbox result must not trigger a request. */
    const float temperature = result.snapshot.current.temperature;
    weather_snapshot_t cached = {.location = config->location};
    assert(weather_cache_load(path, &cached));
    request_at(62, 2);
    request_at(62, 2);
    result = wait_state(WEATHER_UI_OFFLINE);
    assert(result.valid && result.snapshot.current.temperature == temperature);
    unchanged(2);
    request_at(90, 0);
    unchanged(2);
    request_at(123, 0);
    wait_state(WEATHER_UI_READY);
    request_at(184, 5);
    result = wait_state(WEATHER_UI_OFFLINE);
    assert(result.snapshot.current.temperature == temperature);
    request_at(245, 4);
    wait_state(WEATHER_UI_OFFLINE);
    const unsigned before = atomic_load(&calls);
    request_at(400, 0);
    unchanged(before);
    request_at(546, 3);
    result = wait_state(WEATHER_UI_OFFLINE);
    assert(result.valid);
    request_at(787, 0);
    wait_state(WEATHER_UI_READY);
    assert(atomic_load(&calls) == before + 2);
    desktop_network_close();
}

static void boot_error(weather_config_t config, const char *path, bool seed, bool moved)
{
    if (seed) {
        weather_snapshot_t snapshot = {.location = config.location};
        assert(weather_parse(fixture, &snapshot, time(NULL)));
        assert(weather_cache_save(path, &snapshot, fixture));
    }
    if (moved) { config.location.latitude += 1; strcpy(config.location.name, "Other city"); }
    atomic_store(&response, 1);
    assert(desktop_network_start(&config, path));
    if (seed && !moved) {
        desktop_result_t result = wait_state(WEATHER_UI_STALE);
        assert(result.valid);
        result = wait_state(WEATHER_UI_OFFLINE);
        assert(result.valid && result.snapshot.location_id == weather_location_id(&config.location));
    } else {
        desktop_result_t result = wait_state(WEATHER_UI_ERROR);
        assert(!result.valid);
    }
    desktop_network_close();
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    char *text = calloc(WEATHER_MAX_RESPONSE + 1, 1);
    assert(text);
    fixture_size = fread(text, 1, WEATHER_MAX_RESPONSE, file);
    fclose(file);
    fixture = text;
    weather_config_t config;
    char field[80];
    assert(weather_config_load(argv[2], &config, field));
    assert(weather_timezone_apply(&config.location));
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    char directory[] = "/tmp/weather-network-XXXXXX";
    assert(mkdtemp(directory));
    char path[320], backup[324];
    snprintf(path, sizeof(path), "%s/forecast.cache", directory);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    if (!strcmp(argv[3], "cycle")) live_cycle(&config, path);
    else if (!strcmp(argv[3], "cached")) boot_error(config, path, true, false);
    else if (!strcmp(argv[3], "first-offline")) boot_error(config, path, false, false);
    else if (!strcmp(argv[3], "location")) boot_error(config, path, true, true);
    else if (!strcmp(argv[3], "boundaries")) boundaries();
    else abort();
    remove(path); remove(backup); rmdir(directory);
    SDL_Quit();
    free(text);
    printf("weather-network-%s: passed\n", argv[3]);
    return 0;
}

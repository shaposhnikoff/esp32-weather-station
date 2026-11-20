#include "network.h"
#include <SDL.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static weather_config_t settings;
static char cache_file[1024];
static SDL_mutex *mutex;
static SDL_cond *changed;
static SDL_Thread *thread;
static bool stopping, requested, pending;
static desktop_result_t mailbox;

typedef struct {
    char *body;
    size_t used;
    char retry[96];
} transfer_t;

static size_t receive_body(char *bytes, size_t size, size_t count, void *context)
{
    transfer_t *transfer = context;
    if (size && count > (WEATHER_MAX_RESPONSE - transfer->used) / size) return 0;
    const size_t length = size * count;
    memcpy(transfer->body + transfer->used, bytes, length);
    transfer->used += length;
    transfer->body[transfer->used] = '\0';
    return length;
}

static size_t receive_header(char *bytes, size_t size, size_t count, void *context)
{
    transfer_t *transfer = context;
    const size_t length = size * count;
    if (length > 12 && !strncasecmp(bytes, "Retry-After:", 12)) {
        size_t begin = 12, end = length;
        while (begin < end && (bytes[begin] == ' ' || bytes[begin] == '\t')) ++begin;
        while (end > begin && (bytes[end - 1] == '\r' || bytes[end - 1] == '\n')) --end;
        if (end - begin < sizeof(transfer->retry)) {
            memcpy(transfer->retry, bytes + begin, end - begin);
            transfer->retry[end - begin] = '\0';
        }
    }
    return length;
}

static int fetch(transfer_t *transfer)
{
    char url[2048];
    if (!weather_build_url(&settings.location, url, sizeof(url))) return 0;
    CURL *client = curl_easy_init();
    if (!client) return 0;
    curl_easy_setopt(client, CURLOPT_URL, url);
    curl_easy_setopt(client, CURLOPT_TIMEOUT_MS, (long)settings.request_timeout_ms);
    curl_easy_setopt(client, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(client, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(client, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(client, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, receive_body);
    curl_easy_setopt(client, CURLOPT_WRITEDATA, transfer);
    curl_easy_setopt(client, CURLOPT_HEADERFUNCTION, receive_header);
    curl_easy_setopt(client, CURLOPT_HEADERDATA, transfer);
    const CURLcode error = curl_easy_perform(client);
    long status = 0;
    curl_easy_getinfo(client, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(client);
    if (error != CURLE_OK || strlen(transfer->body) != transfer->used) return 0;
    return (int)status;
}

static bool publish(const desktop_result_t *result)
{
    SDL_LockMutex(mutex);
    while (pending && !stopping) SDL_CondWait(changed, mutex);
    const bool running = !stopping;
    if (running) { mailbox = *result; pending = true; }
    SDL_UnlockMutex(mutex);
    return running;
}

static int run(void *argument)
{
    (void)argument;
    desktop_result_t *result = calloc(1, sizeof(*result));
    weather_snapshot_t *candidate = malloc(sizeof(*candidate));
    char *body = calloc(WEATHER_MAX_RESPONSE + 1, 1);
    if (!result || !candidate || !body) {
        desktop_result_t failed = {.state = {.status = WEATHER_UI_ERROR, .now = time(NULL)}};
        publish(&failed);
        free(result); free(candidate); free(body);
        return 1;
    }
    result->snapshot.location = settings.location;
    result->valid = weather_cache_load(cache_file, &result->snapshot);
    result->state.status = result->valid ? WEATHER_UI_STALE : WEATHER_UI_LOADING;
    result->state.now = time(NULL);
    result->state.stale_after_seconds = settings.stale_after_seconds;
    weather_policy_t policy = {.interval_seconds = settings.update_seconds};
    bool running = publish(result);
    bool manual = false;
    while (running) {
        const time_t mono = (time_t)(SDL_GetTicks64() / 1000);
        if ((manual || mono >= policy.next_auto) && weather_policy_begin(&policy, mono)) {
            result->state.status = result->valid ? WEATHER_UI_UPDATING : WEATHER_UI_LOADING;
            if (!publish(result)) break;
            transfer_t transfer = {.body = body};
            body[0] = '\0';
            int status = fetch(&transfer);
            candidate->location = settings.location;
            if (status == 200 && !weather_parse(body, candidate, time(NULL))) status = 0;
            if (status == 200) {
                candidate->demo = false;
                result->snapshot = *candidate;
                result->valid = true;
                if (weather_policy_persist_due(&policy, time(NULL), settings.cache_write_seconds) &&
                    weather_cache_save(cache_file, candidate, body))
                    policy.last_persisted = time(NULL);
                fprintf(stderr, "LIVE weather: HTTP 200, %.1f C\n", (double)candidate->current.temperature);
            } else fprintf(stderr, "Weather request failed: HTTP %d\n", status);
            const time_t now = time(NULL), finished = (time_t)(SDL_GetTicks64() / 1000);
            const time_t retry = weather_retry_after(transfer.retry, now);
            weather_policy_finish(&policy, finished, status, retry > now ? finished + retry - now : 0);
            result->state.status = status == 200 ? WEATHER_UI_READY :
                result->valid ? WEATHER_UI_OFFLINE : WEATHER_UI_ERROR;
            result->state.now = now;
            if (!publish(result)) break;
        }
        SDL_LockMutex(mutex);
        while (!stopping && !requested) {
            const time_t wait = policy.next_auto - (time_t)(SDL_GetTicks64() / 1000);
            if (wait <= 0) break;
            SDL_CondWaitTimeout(changed, mutex, (Uint32)wait * 1000);
        }
        manual = requested;
        requested = false;
        running = !stopping;
        SDL_UnlockMutex(mutex);
    }
    free(result); free(candidate); free(body);
    return 0;
}

bool desktop_network_start(const weather_config_t *config, const char *path)
{
    if (strlen(path) >= sizeof(cache_file) || curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return false;
    settings = *config;
    strcpy(cache_file, path);
    mutex = SDL_CreateMutex();
    changed = SDL_CreateCond();
    if (!mutex || !changed) { desktop_network_close(); return false; }
    thread = SDL_CreateThread(run, "weather_network", NULL);
    if (!thread) { desktop_network_close(); return false; }
    return true;
}

void desktop_network_refresh(void *context)
{
    (void)context;
    if (!mutex) return;
    SDL_LockMutex(mutex);
    requested = true;
    SDL_CondBroadcast(changed);
    SDL_UnlockMutex(mutex);
}

bool desktop_network_receive(desktop_result_t *result)
{
    if (!mutex) return false;
    SDL_LockMutex(mutex);
    const bool available = pending;
    if (available) { *result = mailbox; pending = false; SDL_CondBroadcast(changed); }
    SDL_UnlockMutex(mutex);
    return available;
}

void desktop_network_close(void)
{
    if (mutex) {
        SDL_LockMutex(mutex);
        stopping = true;
        if (changed) SDL_CondBroadcast(changed);
        SDL_UnlockMutex(mutex);
    }
    if (thread) SDL_WaitThread(thread, NULL);
    if (changed) SDL_DestroyCond(changed);
    if (mutex) SDL_DestroyMutex(mutex);
    changed = NULL; mutex = NULL; thread = NULL;
    curl_global_cleanup();
}

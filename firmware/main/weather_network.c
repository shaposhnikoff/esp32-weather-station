#include "weather_network.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static weather_config_t settings;
static EventGroupHandle_t network_events;
static QueueHandle_t results;
static SemaphoreHandle_t refresh_signal;
static weather_result_t result;
static weather_policy_t policy;
static char retry_header[96];

static time_t monotonic_seconds(void)
{
    return (time_t)(esp_timer_get_time() / 1000000);
}

static void publish(weather_ui_status_t status)
{
    result.state.status = status;
    result.state.now = time(NULL);
    result.state.stale_after_seconds = settings.stale_after_seconds;
    xQueueSend(results, &result, portMAX_DELAY);
}

static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(network_events, BIT0);
        ESP_LOGI("weather", "Wi-Fi connected; IP acquired");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(network_events, BIT0);
        ESP_LOGW("weather", "Wi-Fi disconnected");
    }
}

static bool connect_network(void)
{
    if (xEventGroupGetBits(network_events) & BIT0) return true;
    esp_err_t error = esp_wifi_connect();
    if (error != ESP_OK) return false;
    return (xEventGroupWaitBits(network_events, BIT0, pdFALSE, pdFALSE,
                               pdMS_TO_TICKS(20000)) & BIT0) != 0;
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        !strcasecmp(event->header_key, "Retry-After")) {
        snprintf(retry_header, sizeof(retry_header), "%s", event->header_value);
    }
    return ESP_OK;
}

static int fetch(char *body)
{
    char url[2048];
    if (!weather_build_url(&settings.location, url, sizeof(url))) return 0;
    retry_header[0] = '\0';
    const esp_http_client_config_t config = {
        .url = url, .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = (int)settings.request_timeout_ms,
        .disable_auto_redirect = true, .event_handler = http_event,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return 0;
    const int64_t deadline = esp_timer_get_time() + settings.request_timeout_ms * 1000LL;
    int status = 0;
    if (esp_http_client_open(client, 0) != ESP_OK) goto done;
    int64_t header_budget = (deadline - esp_timer_get_time()) / 1000;
    if (header_budget <= 0) goto done;
    esp_http_client_set_timeout_ms(client, (int)header_budget);
    if (esp_http_client_fetch_headers(client) < 0) goto done;
    status = esp_http_client_get_status_code(client);
    if (status != 200) goto done;
    size_t used = 0;
    while (true) {
        int64_t remaining = (deadline - esp_timer_get_time()) / 1000;
        if (remaining <= 0) { status = 0; break; }
        esp_http_client_set_timeout_ms(client, (int)remaining);
        const int count = esp_http_client_read(client, body + used, WEATHER_MAX_RESPONSE + 1 - used);
        if (count < 0) { status = 0; break; }
        if (!count) {
            if (!esp_http_client_is_complete_data_received(client)) status = 0;
            break;
        }
        used += (size_t)count;
        if (used > WEATHER_MAX_RESPONSE) { status = 0; break; }
    }
    body[used <= WEATHER_MAX_RESPONSE ? used : 0] = '\0';
    if (used > WEATHER_MAX_RESPONSE || strlen(body) != used) status = 0;
done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return status;
}

static void network_worker(void *argument)
{
    (void)argument;
    char *body = malloc(WEATHER_MAX_RESPONSE + 1);
    weather_snapshot_t *candidate = malloc(sizeof(*candidate));
    if (!body || !candidate) {
        publish(WEATHER_UI_ERROR);
        free(body);
        free(candidate);
        vTaskDelete(NULL);
        return;
    }
    result.snapshot.location = settings.location;
    result.valid = weather_cache_load("/cache/weather.cache", &result.snapshot);
    publish(result.valid ? WEATHER_UI_STALE : WEATHER_UI_LOADING);
    policy.interval_seconds = settings.update_seconds;
    bool sntp_started = false;
    while (true) {
        const time_t now = monotonic_seconds();
        if (weather_policy_begin(&policy, now)) {
            retry_header[0] = '\0';
            publish(result.valid ? WEATHER_UI_UPDATING : WEATHER_UI_LOADING);
            int status = 0;
            if (connect_network()) {
                if (!sntp_started) {
                    esp_sntp_config_t ntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(settings.ntp_server);
                    sntp_started = esp_netif_sntp_init(&ntp) == ESP_OK;
                }
                if (sntp_started && time(NULL) < 1700000000)
                    esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
                if (time(NULL) >= 1700000000) status = fetch(body);
            }
            candidate->location = settings.location;
            if (status == 200 && !weather_parse(body, candidate, time(NULL))) status = 0;
            if (status == 200) {
                candidate->demo = false;
                result.snapshot = *candidate;
                result.valid = true;
                if (weather_policy_persist_due(&policy, time(NULL), settings.cache_write_seconds)) {
                    if (weather_cache_save("/cache/weather.cache", candidate, body))
                        policy.last_persisted = time(NULL);
                    else ESP_LOGW("weather", "Cache write failed");
                }
                ESP_LOGI("weather", "LIVE weather received: HTTP 200, temperature %.1f C, hours=%u",
                         (double)candidate->current.temperature, (unsigned)candidate->hour_count);
            } else {
                ESP_LOGW("weather", "Weather request failed: HTTP %d", status);
            }
            const time_t wall = time(NULL), mono = monotonic_seconds();
            const time_t retry = weather_retry_after(retry_header, wall);
            weather_policy_finish(&policy, mono, status, retry > wall ? mono + retry - wall : 0);
            publish(status == 200 ? WEATHER_UI_READY :
                    (result.valid ? WEATHER_UI_OFFLINE : WEATHER_UI_ERROR));
        }
        time_t wait = policy.next_auto - monotonic_seconds();
        if (wait < 1) wait = 1;
        xSemaphoreTake(refresh_signal, pdMS_TO_TICKS((uint32_t)wait * 1000));
    }
}

void weather_network_start(const weather_config_t *config)
{
    settings = *config;
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    network_events = xEventGroupCreate();
    results = xQueueCreate(2, sizeof(weather_result_t));
    refresh_signal = xSemaphoreCreateBinary();
    assert(network_events && results && refresh_signal);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL));
    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, settings.wifi_ssid, strlen(settings.wifi_ssid));
    memcpy(wifi.sta.password, settings.wifi_password, strlen(settings.wifi_password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    assert(xTaskCreate(network_worker, "weather_network", 24576, NULL, 4, NULL) == pdPASS);
}

void weather_network_refresh(void *context)
{
    (void)context;
    if (refresh_signal) xSemaphoreGive(refresh_signal);
}

bool weather_network_receive(weather_result_t *out)
{
    return results && xQueueReceive(results, out, 0) == pdTRUE;
}

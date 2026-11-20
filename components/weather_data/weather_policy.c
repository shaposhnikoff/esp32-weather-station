#define _XOPEN_SOURCE 700
#include "weather_data.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool weather_policy_begin(weather_policy_t *policy, time_t now)
{
    if (policy->busy || now < policy->next_allowed) return false;
    policy->busy = true;
    return true;
}

void weather_policy_finish(weather_policy_t *policy, time_t now, int http_status, time_t retry_after)
{
    policy->busy = false;
    const unsigned interval = policy->interval_seconds ? policy->interval_seconds : 900;
    if (http_status == 200) {
        policy->failures = 0;
        policy->next_allowed = now + 60;
        policy->next_auto = now + interval;
        return;
    }
    if (policy->failures < 7) ++policy->failures;
    unsigned delay = 60u << (policy->failures - 1);
    if (delay > 3600) delay = 3600;
    policy->next_allowed = now + delay;
    if (http_status == 429 && retry_after > policy->next_allowed)
        policy->next_allowed = retry_after;
    policy->next_auto = policy->next_allowed;
}

time_t weather_retry_after(const char *header, time_t now)
{
    if (!header || !*header) return 0;
    bool digits = true;
    for (const char *p = header; *p; ++p)
        if (!isdigit((unsigned char)*p)) digits = false;
    if (digits) {
        char *end;
        const unsigned long seconds = strtoul(header, &end, 10);
        if (*end || seconds > INT_MAX) return 0;
        return now + (time_t)seconds;
    }
    struct tm date = {0};
    const char *end = strptime(header, "%a, %d %b %Y %H:%M:%S GMT", &date);
    if (!end || *end) return 0;
    /* Compute UTC without changing the process's selected-location timezone. */
    const int year = date.tm_year + 1900;
    if (year < 2000 || year > 2100 || date.tm_mon < 0 || date.tm_mon > 11) return 0;
    int64_t days = 0;
    for (int y = 1970; y < year; ++y)
        days += 365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int m = 0; m < date.tm_mon; ++m)
        days += lengths[m] + (m == 1 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    return (time_t)((days + date.tm_mday - 1) * 86400 +
           date.tm_hour * 3600 + date.tm_min * 60 + date.tm_sec);
}

bool weather_policy_persist_due(const weather_policy_t *policy, time_t now, unsigned minimum_interval)
{
    return !policy->last_persisted || now - policy->last_persisted >= minimum_interval;
}

#include "weather_data.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool weather_timezone_apply(const weather_location_t *location)
{
    if (setenv("TZ", location->posix_timezone, 1) != 0) return false;
    tzset();
    return true;
}

void weather_clock(time_t instant, char result[6])
{
    if (!instant) { memcpy(result, "--:--", 6); return; }
    struct tm local;
    localtime_r(&instant, &local);
    strftime(result, 6, "%H:%M", &local);
}

static bool same_date(time_t left, time_t right)
{
    struct tm a, b;
    localtime_r(&left, &a);
    localtime_r(&right, &b);
    return a.tm_year == b.tm_year && a.tm_yday == b.tm_yday;
}

size_t weather_future_hours(const weather_snapshot_t *snapshot, time_t now, size_t indices[6])
{
    size_t count = 0;
    for (size_t i = 0; i < snapshot->hour_count && count < 6; ++i)
        if (snapshot->hours[i].time > now) indices[count++] = i;
    return count;
}

size_t weather_visible_days(const weather_snapshot_t *snapshot, time_t now, size_t indices[5])
{
    size_t count = 0;
    for (size_t i = 0; i < snapshot->day_count && count < 5; ++i)
        if (same_date(snapshot->days[i].time, now) || snapshot->days[i].time > now)
            indices[count++] = i;
    return count;
}

const weather_day_t *weather_today(const weather_snapshot_t *snapshot, time_t now)
{
    for (size_t i = 0; i < snapshot->day_count; ++i)
        if (same_date(snapshot->days[i].time, now)) return &snapshot->days[i];
    return NULL;
}

bool weather_is_stale(const weather_snapshot_t *snapshot, time_t now, unsigned maximum_age)
{
    return now < snapshot->fetched_at || now - snapshot->fetched_at > maximum_age;
}

/* Orbital approximation from SunCalc's BSD-2-Clause getMoonIllumination.
 * Evaluate at local noon to associate the phase with the selected local date. */
double weather_moon_phase(time_t selected_local_date)
{
    struct tm local;
    localtime_r(&selected_local_date, &local);
    local.tm_hour = 12; local.tm_min = 0; local.tm_sec = 0; local.tm_isdst = -1;
    const time_t noon = mktime(&local);
    if (noon == (time_t)-1) return NAN;
    const double pi = 3.14159265358979323846, rad = pi / 180;
    const double d = (double)noon / 86400 + 2440587.5 - 2451545.0;
    const double e = rad * 23.4397;
    const double sm = rad * (357.5291 + 0.98560028 * d);
    const double sl = sm + rad * (1.9148 * sin(sm) + 0.02 * sin(2 * sm) +
                      0.0003 * sin(3 * sm) + 102.9372) + pi;
    const double sra = atan2(sin(sl) * cos(e), cos(sl));
    const double sdec = asin(sin(e) * sin(sl));
    const double mm = rad * (134.963 + 13.064993 * d);
    const double ml = rad * (218.316 + 13.176396 * d) + rad * 6.289 * sin(mm);
    const double mb = rad * 5.128 * sin(rad * (93.272 + 13.229350 * d));
    const double md = 385001 - 20905 * cos(mm);
    const double mra = atan2(sin(ml) * cos(e) - tan(mb) * sin(e), cos(ml));
    const double mdec = asin(sin(mb) * cos(e) + cos(mb) * sin(e) * sin(ml));
    const double phi = acos(sin(sdec) * sin(mdec) + cos(sdec) * cos(mdec) * cos(sra - mra));
    const double inc = atan2(149598000 * sin(phi), md - 149598000 * cos(phi));
    const double angle = atan2(cos(sdec) * sin(sra - mra),
        sin(sdec) * cos(mdec) - cos(sdec) * sin(mdec) * cos(sra - mra));
    return 0.5 + 0.5 * inc * (angle < 0 ? -1 : 1) / pi;
}

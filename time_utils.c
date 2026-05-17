#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

time_t now_time(void) {
    return time(NULL);
}

void format_time(time_t t, char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (t == 0) {
        snprintf(buf, size, "-");
        return;
    }
    struct tm *tm_ptr = localtime(&t);
    if (!tm_ptr) {
        snprintf(buf, size, "-");
        return;
    }
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_ptr);
}

int seconds_to_minutes_ceil(time_t start, time_t end) {
    if (end <= start) return 1;
    long diff = (long)difftime(end, start);
    int minutes = (int)((diff + 59) / 60);
    if (minutes < 1) minutes = 1;
    return minutes;
}

int parse_datetime(const char *s, time_t *out) {
    int y, mo, d, h, mi, sec = 0;
    char tail;
    struct tm tmv;
    time_t result;
    char buf[64];
    if (!s || !out) return 0;
    snprintf(buf, sizeof(buf), "%s", s);
    for (char *p = buf; *p; p++) {
        if (*p == 'T') *p = ' ';
    }
    int matched = sscanf(buf, "%d-%d-%d %d:%d:%d%c", &y, &mo, &d, &h, &mi, &sec, &tail);
    if (matched != 6) {
        matched = sscanf(buf, "%d-%d-%d %d:%d%c", &y, &mo, &d, &h, &mi, &tail);
        if (matched != 5) return 0;
        sec = 0;
    }
    if (y < 1970 || y > 2099) return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 59) return 0;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = mo - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = h;
    tmv.tm_min = mi;
    tmv.tm_sec = sec;
    tmv.tm_isdst = -1;
    result = mktime(&tmv);
    if (result == (time_t)-1) return 0;
    struct tm *check = localtime(&result);
    if (!check) return 0;
    if (check->tm_year != y - 1900 || check->tm_mon != mo - 1 || check->tm_mday != d ||
        check->tm_hour != h || check->tm_min != mi || check->tm_sec != sec) return 0;
    *out = result;
    return 1;
}

int charged_minutes_for_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    if (actual_minutes <= BASE_MINUTES) return BASE_MINUTES;
    int extra = actual_minutes - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    return BASE_MINUTES + units * EXTRA_UNIT_MINUTES;
}

void format_duration_minutes(int minutes, char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (minutes < 0) minutes = 0;
    int days = minutes / 1440;
    int rem = minutes % 1440;
    int hours = rem / 60;
    int mins = rem % 60;
    if (days > 0) snprintf(buf, size, "%d일 %d시간 %d분", days, hours, mins);
    else snprintf(buf, size, "%d시간 %d분", hours, mins);
}

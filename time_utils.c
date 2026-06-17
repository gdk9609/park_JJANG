#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

// 현재 시스템 시간을 time_t 형식으로 반환하는 함수
time_t now_time(void) {
    return time(NULL);
}

// time_t 형식의 시간을 "YYYY-MM-DD HH:MM:SS" 문자열 형식으로 변환하는 함수
// 시간이 없거나 변환에 실패하면 "-"를 반환
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

// 두 시간 사이의 차이를 분 단위로 계산하는 함수
// 초 단위는 올림 처리하며 최소 1분을 반환
int seconds_to_minutes_ceil(time_t start, time_t end) {
    if (end <= start) return 1;
    long diff = (long)difftime(end, start);
    int minutes = (int)((diff + 59) / 60);
    if (minutes < 1) minutes = 1;
    return minutes;
}

// 문자열 형태의 날짜/시간을 time_t 형식으로 변환하는 함수
// YYYY-MM-DD HH:MM 또는 YYYY-MM-DD HH:MM:SS 형식을 지원
// 날짜 및 시간 범위의 유효성을 검사
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

// 실제 주차 시간을 요금 계산 기준 시간으로 변환하는 함수
// 기본 시간 이후의 추가 시간은 10분 단위로 올림 계산
int charged_minutes_for_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    if (actual_minutes <= BASE_MINUTES) return BASE_MINUTES;
    int extra = actual_minutes - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    return BASE_MINUTES + units * EXTRA_UNIT_MINUTES;
}

// 분 단위 시간을 "n일 n시간 n분" 형식의 문자열로 변환하는 함수
// 주차 이용 시간을 사용자에게 표시할 때 사용
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

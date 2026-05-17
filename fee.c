#include <stdio.h>
#include "parking.h"

int calculate_parking_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    if (actual_minutes <= BASE_MINUTES) return BASE_FEE;
    int extra = actual_minutes - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    return BASE_FEE + units * EXTRA_UNIT_FEE;
}

int api_calculate_fee(const char *entry_time_text, const char *exit_time_text,
                      int *total_minutes, int *charged_minutes, int *fee,
                      char *err, size_t err_size) {
    time_t entry_t, exit_t;
    if (!entry_time_text || !*entry_time_text || !exit_time_text || !*exit_time_text) {
        snprintf(err, err_size, "입차/출차 일시를 모두 입력해주세요.");
        return 0;
    }
    if (!parse_datetime(entry_time_text, &entry_t)) {
        snprintf(err, err_size, "입차 일시 형식이 올바르지 않습니다. YYYY-MM-DD HH:MM:SS 형식으로 입력하세요.");
        return 0;
    }
    if (!parse_datetime(exit_time_text, &exit_t)) {
        snprintf(err, err_size, "출차 일시 형식이 올바르지 않습니다. YYYY-MM-DD HH:MM:SS 형식으로 입력하세요.");
        return 0;
    }
    if (exit_t <= entry_t) {
        snprintf(err, err_size, "출차 일시는 입차 일시보다 늦어야 합니다.");
        return 0;
    }
    int actual = seconds_to_minutes_ceil(entry_t, exit_t);
    int charged = charged_minutes_for_fee(actual);
    int amount = calculate_parking_fee(actual);
    if (total_minutes) *total_minutes = actual;
    if (charged_minutes) *charged_minutes = charged;
    if (fee) *fee = amount;
    return 1;
}

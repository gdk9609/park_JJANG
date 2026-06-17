#include <stdio.h>
#include "parking.h"

// 실제 주차 시간을 기준으로 주차요금을 계산하는 함수
// 기본 1시간 요금(BASE_FEE)을 적용하고 초과 시간은 10분 단위(EXTRA_UNIT_MINUTES)로 올림 계산하여 추가 요금을 부과함

int calculate_parking_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    if (actual_minutes <= BASE_MINUTES) return BASE_FEE;
    int extra = actual_minutes - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    return BASE_FEE + units * EXTRA_UNIT_FEE;
}



// 입차/출차 시간을 입력받아 예상 주차요금을 계산하는 API 함수
// 날짜 형식 검증 및 입차·출차 시간 비교를 수행하고 실제 주차 시간, 요금 적용 시간, 최종 주차요금을 계산하여 반환함


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

/*
- 관리자 페이지에서 입차 중인 차량의 입차시간을 수정하는 함수
- 예약번호로 차량을 조회한 뒤 새로운 입차시간으로 변경
- 날짜 형식 검증, 미래 시간 여부 검사, 파일 저장
*/

#include <stdio.h>
#include "parking.h"

int api_update_entry_time(const char *reservation_code, const char *new_entry_time_text,
                          ParkingRecord *out, char *err, size_t err_size) {

    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }
    if (!new_entry_time_text || !*new_entry_time_text) {
        snprintf(err, err_size, "새 입차시간이 비어 있습니다.");
        return 0;
    }

    ParkingRecord p; 
    int index; 


    if (!find_parking_by_code(reservation_code, &p, &index)) {
        snprintf(err, err_size, "현재 입차 중인 차량에서 해당 예약번호를 찾을 수 없습니다.");
        return 0;
    }

    time_t new_entry;
    if (!parse_datetime(new_entry_time_text, &new_entry)) {
        snprintf(err, err_size, "날짜 형식이 올바르지 않습니다. 예: 2026-05-08 13:30");
        return 0;
    }
    if (new_entry > now_time()) {
        snprintf(err, err_size, "입차시간은 현재 시각보다 미래일 수 없습니다.");
        return 0;
    }

    p.entry_time = new_entry;
    if (update_record_at(PARKING_FILE, index, &p, sizeof(ParkingRecord)) < 0) {
        snprintf(err, err_size, "입차시간 수정 저장에 실패했습니다.");
        return 0;
    }

    if (out) *out = p;
    return 1;
}

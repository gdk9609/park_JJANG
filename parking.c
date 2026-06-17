#include <stdio.h>
#include <string.h>
#include "parking.h"

// 주차 데이터 파일에서 가장 큰 ID를 찾아 다음 주차 ID를 생성하는 함수
int next_parking_id(void) {
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && p.id > max_id) max_id = p.id;
    }
    return max_id + 1;
}

// 새로운 입차 차량 정보를 주차 데이터 파일에 추가하는 함수
int append_parking_record(const ParkingRecord *parking) {
    if (!parking) return -1;
    return append_record(PARKING_FILE, parking, sizeof(ParkingRecord));
}

// 예약번호를 이용해 현재 입차 중인 차량 정보를 검색하는 함수
// 차량 정보와 파일 내 위치(index)를 반환
int find_parking_by_code(const char *code, ParkingRecord *parking, int *index_out) {
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            if (strcmp(p.reservation_code, code) == 0) {
                if (parking) *parking = p;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

// 주차 데이터 파일에서 특정 index의 입차 기록을 삭제하는 함수
int delete_parking_record(int index) {
    return delete_record_at(PARKING_FILE, index, sizeof(ParkingRecord));
}

// 예약 완료된 차량을 실제 입차 처리하는 API 함수
// 예약번호 및 차량번호를 검증하고 입차 정보를 parking.dat에 저장한 뒤 예약 정보를 삭제
int api_entry_car(const char *reservation_code, const char *car_number, ParkingRecord *out,
                  char *err, size_t err_size) {
    expire_old_reservations();
    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }
    if (!car_number || !*car_number) {
        snprintf(err, err_size, "차량번호가 비어 있습니다.");
        return 0;
    }

    Reservation r;
    int index;
    if (!find_reservation_by_code(reservation_code, &r, &index)) {
        ParkingRecord existing;
        if (find_parking_by_code(reservation_code, &existing, NULL)) {
            snprintf(err, err_size, "이미 입차 처리된 차량입니다.");
            return 0;
        }
        snprintf(err, err_size, "해당 예약번호가 없습니다.");
        return 0;
    }

    if (strcmp(r.car_number, car_number) != 0) {
        snprintf(err, err_size, "예약번호와 차량번호가 일치하지 않습니다.");
        return 0;
    }
    if (r.status != RES_RESERVED) {
        snprintf(err, err_size, "예약 완료 상태인 예약만 입차할 수 있습니다.");
        return 0;
    }

    time_t now = now_time();
    if (difftime(now, r.reserved_at) > RESERVATION_TIMEOUT_SECONDS) {
        delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation));
        snprintf(err, err_size, "예약 후 20분이 지나 입차할 수 없습니다.");
        return 0;
    }

    if (increase_tower_current(r.tower_id, r.car_type) < 0) {
        snprintf(err, err_size, "주차타워 상태 갱신에 실패했습니다.");
        return 0;
    }

    ParkingRecord p;
    memset(&p, 0, sizeof(p));
    p.id = next_parking_id();
    snprintf(p.reservation_code, sizeof(p.reservation_code), "%s", r.code);
    snprintf(p.car_number, sizeof(p.car_number), "%s", r.car_number);
    snprintf(p.phone, sizeof(p.phone), "%s", r.phone);
    p.tower_id = r.tower_id;
    p.car_type = r.car_type;
    p.reserved_at = r.reserved_at;
    p.entry_time = now;
    p.deposit = r.deposit;

    if (append_parking_record(&p) < 0) {
        decrease_tower_current(r.tower_id, r.car_type);
        snprintf(err, err_size, "입차 정보 저장에 실패했습니다.");
        return 0;
    }

    if (delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation)) < 0) {
        int parking_index;
        if (find_parking_by_code(p.reservation_code, NULL, &parking_index)) delete_parking_record(parking_index);
        decrease_tower_current(r.tower_id, r.car_type);
        snprintf(err, err_size, "예약목록 삭제에 실패하여 입차를 취소했습니다.");
        return 0;
    }

    if (out) *out = p;
    return 1;
}

// 입차 기록을 기반으로 결제 정보를 생성하는 내부 함수
// 실제 이용 시간과 요금 적용 시간을 계산하고 총 주차요금을 계산
// 예약 보증금 차감 금액과 최종 결제 금액을 계산하여 Payment 구조체에 저장
static void fill_payment_from_parking(const ParkingRecord *pking, const char *method, Payment *p) {
    time_t exit_time = now_time();
    int actual_minutes = seconds_to_minutes_ceil(pking->entry_time, exit_time);
    int charged_minutes = charged_minutes_for_fee(actual_minutes);
    int parking_fee = calculate_parking_fee(actual_minutes);
    int deposit_used = parking_fee < RESERVATION_DEPOSIT ? parking_fee : RESERVATION_DEPOSIT;
    int final_fee = parking_fee - deposit_used;
    if (final_fee < 0) final_fee = 0;

    memset(p, 0, sizeof(*p));
    p->id = count_records(PAYMENTS_FILE, sizeof(Payment)) + 1;
    snprintf(p->reservation_code, sizeof(p->reservation_code), "%s", pking->reservation_code);
    snprintf(p->car_number, sizeof(p->car_number), "%s", pking->car_number);
    snprintf(p->phone, sizeof(p->phone), "%s", pking->phone);
    p->tower_id = pking->tower_id;
    p->car_type = pking->car_type;
    p->entry_time = pking->entry_time;
    p->exit_time = exit_time;
    p->total_minutes = actual_minutes;
    p->charged_minutes = charged_minutes;
    p->parking_fee = parking_fee;
    p->deposit_used = deposit_used;
    p->final_fee = final_fee;
    snprintf(p->method, sizeof(p->method), "%s", (method && *method) ? method : "카드");
}

// 현재 입차 중인 차량의 예상 출차 요금을 미리 계산하는 API 함수
// 실제 출차 처리 없이 예상 결제 정보를 반환
int api_preview_exit_fee(const char *reservation_code, Payment *out, char *err, size_t err_size) {
    expire_old_reservations();
    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }
    ParkingRecord pking;
    if (!find_parking_by_code(reservation_code, &pking, NULL)) {
        snprintf(err, err_size, "현재 입차 중인 차량에서 해당 예약번호를 찾을 수 없습니다.");
        return 0;
    }
    if (out) fill_payment_from_parking(&pking, "", out);
    return 1;
}

// 차량 출차 및 결제를 처리하는 API 함수
// 결제 정보를 생성하고 영수증을 저장하고 입차 기록 삭제 및 주차타워 상태를 갱신
int api_exit_car(const char *reservation_code, const char *method, Payment *out, char *err, size_t err_size) {
    expire_old_reservations();
    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }

    ParkingRecord pking;
    int parking_index;
    if (!find_parking_by_code(reservation_code, &pking, &parking_index)) {
        snprintf(err, err_size, "현재 입차 중인 차량에서 해당 예약번호를 찾을 수 없습니다.");
        return 0;
    }

    Payment p;
    fill_payment_from_parking(&pking, method, &p);
    make_receipt_number(p.receipt_no, sizeof(p.receipt_no));

    if (append_payment(&p) < 0) {
        snprintf(err, err_size, "영수증 기록 저장에 실패했습니다.");
        return 0;
    }
    save_receipt_text(&p);

    if (delete_parking_record(parking_index) < 0) {
        snprintf(err, err_size, "입차 데이터 삭제에 실패했습니다. 영수증 데이터는 저장되었습니다.");
        return 0;
    }
    decrease_tower_current(pking.tower_id, pking.car_type);


    if (out) *out = p;
    return 1;
}

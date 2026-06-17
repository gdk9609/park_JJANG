/*
 * reservation.c
 *
 * 주차 예약 기능을 처리하는 파일이다.
 * 예약 생성, 취소, 변경, 조회 기능을 담당하며,
 * 예약번호 중복 검사, 예약 만료 처리, 예약 메시지 저장 기능도 포함한다.
 *
 * 주요 기능:
 * - 새 예약 생성
 * - 예약 취소
 * - 예약 차종/타워 변경
 * - 예약번호로 상태 조회
 * - 차량번호와 전화번호로 상태 조회
 * - 만료된 예약 자동 삭제
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

// 결제 내역 파일에서 해당 예약번호가 이미 사용된 적 있는지 확인하는 함수
static int parking_code_exists_in_payments(const char *code) {
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    for (int i = 0; i < count; i++) {
        Payment p;
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0 && strcmp(p.reservation_code, code) == 0) return 1;
    }
    return 0;
}

// 예약, 주차, 결제 내역 전체에서 예약번호가 중복되는지 확인하는 함수
static int reservation_code_exists_anywhere(const char *code) {
    if (find_reservation_by_code(code, NULL, NULL)) return 1;
    if (find_parking_by_code(code, NULL, NULL)) return 1;
    if (parking_code_exists_in_payments(code)) return 1;
    return 0;
}

// 대문자와 소문자가 포함된 5자리 랜덤 예약번호를 생성하는 함수
static void make_random_reservation_code(char *out, size_t size) {
    const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int char_count = 62;
    if (!out || size < 6) return;

    for (int attempt = 0; attempt < 10000; attempt++) {
        int has_upper = 0;
        int has_lower = 0;
        for (int i = 0; i < 5; i++) {
            char c = chars[rand() % char_count];
            out[i] = c;
            if (isupper((unsigned char)c)) has_upper = 1;
            if (islower((unsigned char)c)) has_lower = 1;
        }
        out[5] = '\0';
        if (has_upper && has_lower && !reservation_code_exists_anywhere(out)) return;
    }

    snprintf(out, size, "A%04d", rand() % 10000);
}

// 해당 차량이 현재 예약 중이거나 주차 중인지 확인하는 함수
static int has_active_reservation_for_car(const char *car_number) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    time_t now = now_time();
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.car_number, car_number) == 0 && r.status == RES_RESERVED &&
                difftime(now, r.reserved_at) <= RESERVATION_TIMEOUT_SECONDS) return 1;
        }
    }

    count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && strcmp(p.car_number, car_number) == 0) return 1;
    }
    return 0;
}

// 예약 완료 정보를 메시지 파일에 저장하는 함수
static void send_reservation_message(const Reservation *r) {
    char timebuf[64];
    char line[512];
    format_time(now_time(), timebuf, sizeof(timebuf));
    snprintf(line, sizeof(line), "[%s] TO %s: 예약번호 %s / 차량번호 %s / %s / %c Tower",
             timebuf, r->phone, r->code, r->car_number, car_type_name(r->car_type), 'A' + r->tower_id - 1);
    append_text_line(MESSAGES_FILE, line);
}

// 예약 만료까지 남은 시간을 초 단위로 계산하는 함수
static int remaining_seconds_for_reservation(const Reservation *r) {
    if (!r || r->status != RES_RESERVED) return 0;
    int elapsed = (int)difftime(now_time(), r->reserved_at);
    int remaining = RESERVATION_TIMEOUT_SECONDS - elapsed;
    return remaining > 0 ? remaining : 0;
}

// 기존 예약 중 가장 큰 ID를 찾아 다음 예약 ID를 생성하는 함수
int next_reservation_id(void) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0 && r.id > max_id) max_id = r.id;
    }
    return max_id + 1;
}

// 제한 시간이 지난 예약을 자동으로 만료 처리하고 삭제하는 함수
void expire_old_reservations(void) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    time_t now = now_time();
    int i = 0;
    while (i < count) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (r.status == RES_RESERVED && difftime(now, r.reserved_at) > RESERVATION_TIMEOUT_SECONDS) {
                delete_record_at(RESERVATIONS_FILE, i, sizeof(Reservation));
                count--;
                continue;
            }
        }
        i++;
    }
}

// 특정 타워와 차종에 대해 현재 유효한 예약 개수를 계산하는 함수
int count_active_reservations(int tower_id, int car_type) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    int active = 0;
    time_t now = now_time();
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (r.tower_id == tower_id && r.car_type == car_type && r.status == RES_RESERVED) {
                if (difftime(now, r.reserved_at) <= RESERVATION_TIMEOUT_SECONDS) active++;
            }
        }
    }
    return active;
}

// 예약번호를 이용해 예약 정보를 검색하는 함수
int find_reservation_by_code(const char *code, Reservation *res, int *index_out) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.code, code) == 0) {
                if (res) *res = r;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

// 특정 위치의 예약 정보를 새 예약 정보로 갱신하는 함수
int update_reservation(const Reservation *res, int index) {
    if (!res) return -1;
    return update_record_at(RESERVATIONS_FILE, index, res, sizeof(Reservation));
}

// 웹에서 입력받은 차량번호, 전화번호, 차종, 타워 정보를 이용해 새 예약을 생성하는 함수
int api_create_reservation(const char *car_number, const char *phone, int car_type, int tower_id,
                           Reservation *out, char *err, size_t err_size) {
    expire_old_reservations();

    if (!car_number || !*car_number) {
        snprintf(err, err_size, "차량번호가 비어 있습니다.");
        return 0;
    }
    if (!phone || !*phone) {
        snprintf(err, err_size, "전화번호가 비어 있습니다.");
        return 0;
    }
    if (!validate_car_number(car_number)) {
        snprintf(err, err_size, "차량번호 형식이 올바르지 않습니다.");
        return 0;
    }
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) {
        snprintf(err, err_size, "차종 값이 올바르지 않습니다.");
        return 0;
    }

    char formatted_phone[MAX_PHONE];
    if (!format_phone_number(phone, formatted_phone, sizeof(formatted_phone))) {
        snprintf(err, err_size, "전화번호 형식이 올바르지 않습니다.");
        return 0;
    }

    if (has_active_reservation_for_car(car_number)) {
        snprintf(err, err_size, "이미 예약 중이거나 주차 중인 차량입니다.");
        return 0;
    }

    if (tower_id == 0) tower_id = recommend_tower_for_car_type(car_type);
    if (tower_id < 1 || tower_id > 3 || !get_tower_by_id(tower_id, NULL, NULL)) {
        snprintf(err, err_size, "주차타워 값이 올바르지 않습니다.");
        return 0;
    }
    if (get_available_slots(tower_id, car_type) <= 0) {
        snprintf(err, err_size, "선택한 타워의 해당 차종 예약 가능 대수가 없습니다.");
        return 0;
    }

    Reservation r;
    memset(&r, 0, sizeof(r));
    r.id = next_reservation_id();
    make_random_reservation_code(r.code, sizeof(r.code));
    snprintf(r.car_number, sizeof(r.car_number), "%s", car_number);
    snprintf(r.phone, sizeof(r.phone), "%s", formatted_phone);
    r.car_type = car_type;
    r.tower_id = tower_id;
    r.reserved_at = now_time();
    r.entry_time = 0;
    r.exit_time = 0;
    r.status = RES_RESERVED;
    r.deposit = RESERVATION_DEPOSIT;
    r.deposit_forfeited = 0;
    r.total_fee = 0;
    r.final_paid = 0;

    if (append_record(RESERVATIONS_FILE, &r, sizeof(r)) < 0) {
        snprintf(err, err_size, "예약 저장에 실패했습니다.");
        return 0;
    }

    send_reservation_message(&r);

    if (out) *out = r;
    return 1;
}

// 예약번호를 이용해 아직 입차하지 않은 예약을 취소하는 함수
int api_cancel_reservation(const char *reservation_code, char *err, size_t err_size) {
    expire_old_reservations();
    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }

    Reservation r;
    int index;
    if (!find_reservation_by_code(reservation_code, &r, &index)) {
        snprintf(err, err_size, "취소 가능한 예약을 찾을 수 없습니다. 이미 입차, 만료 또는 출차 처리되었을 수 있습니다.");
        return 0;
    }
    if (r.status != RES_RESERVED) {
        snprintf(err, err_size, "예약 완료 상태인 건만 취소할 수 있습니다.");
        return 0;
    }
    if (delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation)) < 0) {
        snprintf(err, err_size, "예약 취소 저장에 실패했습니다.");
        return 0;
    }
   
    return 1;
}

// 예약번호를 이용해 예약된 차종과 주차타워를 변경하는 함수
int api_change_reservation(const char *reservation_code, int car_type, int tower_id,
                           Reservation *out, char *err, size_t err_size) {
    expire_old_reservations();
    if (!reservation_code || !*reservation_code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) {
        snprintf(err, err_size, "차종 값이 올바르지 않습니다.");
        return 0;
    }
    if (tower_id < 1 || tower_id > 3 || !get_tower_by_id(tower_id, NULL, NULL)) {
        snprintf(err, err_size, "주차타워 값이 올바르지 않습니다.");
        return 0;
    }

    Reservation r;
    int index;
    if (!find_reservation_by_code(reservation_code, &r, &index)) {
        snprintf(err, err_size, "변경 가능한 예약을 찾을 수 없습니다. 이미 입차, 만료 또는 출차 처리되었을 수 있습니다.");
        return 0;
    }
    if (r.status != RES_RESERVED) {
        snprintf(err, err_size, "예약 완료 상태인 건만 변경할 수 있습니다.");
        return 0;
    }

    int available = get_available_slots(tower_id, car_type);
    if (r.tower_id == tower_id && r.car_type == car_type) available += 1;
    if (available <= 0) {
        snprintf(err, err_size, "선택한 타워의 해당 차종 자리가 만차입니다.");
        return 0;
    }

    r.car_type = car_type;
    r.tower_id = tower_id;
    if (update_record_at(RESERVATIONS_FILE, index, &r, sizeof(Reservation)) < 0) {
        snprintf(err, err_size, "예약 변경 저장에 실패했습니다.");
        return 0;
    }

    send_reservation_message(&r);

    if (out) *out = r;
    return 1;
}

// 예약번호로 현재 활성 예약 또는 입차 상태를 조회하는 함수
int api_get_active_status_by_code(const char *code, Reservation *res_out, ParkingRecord *parking_out,
                                  int *status_out, int *remaining_seconds_out, char *err, size_t err_size) {
    expire_old_reservations();
    if (!code || !*code) {
        snprintf(err, err_size, "예약번호가 비어 있습니다.");
        return 0;
    }

    Reservation r;
    int r_index;
    if (find_reservation_by_code(code, &r, &r_index)) {
        if (res_out) *res_out = r;
        if (parking_out) memset(parking_out, 0, sizeof(ParkingRecord));
        if (status_out) *status_out = RES_RESERVED;
        if (remaining_seconds_out) *remaining_seconds_out = remaining_seconds_for_reservation(&r);
        return 1;
    }

    ParkingRecord p;
    if (find_parking_by_code(code, &p, NULL)) {
        if (parking_out) *parking_out = p;
        if (res_out) memset(res_out, 0, sizeof(Reservation));
        if (status_out) *status_out = RES_ENTERED;
        if (remaining_seconds_out) *remaining_seconds_out = 0;
        return 1;
    }

    snprintf(err, err_size, "해당 예약번호의 활성 예약 또는 입차 내역이 없습니다.");
    return 0;
}

// 차량번호와 전화번호를 이용해 활성 예약 또는 입차 상태를 조회하는 함수
int api_find_active_by_identity(const char *car_number, const char *phone, Reservation *res_out,
                                ParkingRecord *parking_out, int *status_out, int *remaining_seconds_out,
                                char *err, size_t err_size) {
    expire_old_reservations();
    if (!car_number || !*car_number || !phone || !*phone) {
        snprintf(err, err_size, "차량번호와 전화번호를 입력해주세요.");
        return 0;
    }

    char formatted_phone[MAX_PHONE];
    if (!format_phone_number(phone, formatted_phone, sizeof(formatted_phone))) {
        snprintf(err, err_size, "전화번호 형식이 올바르지 않습니다.");
        return 0;
    }

    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.car_number, car_number) == 0 && strcmp(r.phone, formatted_phone) == 0 && r.status == RES_RESERVED) {
                if (res_out) *res_out = r;
                if (parking_out) memset(parking_out, 0, sizeof(ParkingRecord));
                if (status_out) *status_out = RES_RESERVED;
                if (remaining_seconds_out) *remaining_seconds_out = remaining_seconds_for_reservation(&r);
                return 1;
            }
        }
    }

    count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            if (strcmp(p.car_number, car_number) == 0 && strcmp(p.phone, formatted_phone) == 0) {
                if (parking_out) *parking_out = p;
                if (res_out) memset(res_out, 0, sizeof(Reservation));
                if (status_out) *status_out = RES_ENTERED;
                if (remaining_seconds_out) *remaining_seconds_out = 0;
                return 1;
            }
        }
    }

    snprintf(err, err_size, "차량번호와 전화번호가 일치하는 활성 예약 또는 입차 내역이 없습니다.");
    return 0;
}

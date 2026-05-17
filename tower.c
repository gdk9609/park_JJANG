#include <stdio.h>
#include <string.h>
#include "parking.h"

// 주차타워 구조체를 초기화하는 내부 함수
// 타워 이름, 전체 수용량, 차종별 수용 가능 대수, 운영 시간 등의 기본 정보 설정
static void make_tower(ParkingTower *t, int id, const char *name, int total,
                       int compact, int midsize, int suv, int large, int ev) {
    memset(t, 0, sizeof(*t));
    t->id = id;
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->total_capacity = total;
    t->capacity[CAR_COMPACT] = compact;
    t->capacity[CAR_MIDSIZE] = midsize;
    t->capacity[CAR_SUV] = suv;
    t->capacity[CAR_LARGE] = large;
    t->capacity[CAR_EV] = ev;
    snprintf(t->operating_hours, sizeof(t->operating_hours), "%s", DEFAULT_OPERATING_HOURS);
    t->active = 1;
}

// 기본 주차타워(A/B/C Tower) 데이터를 생성하는 함수
// 최초 실행 시 타워 데이터 파일에 기본 타워 정보를 저장
int init_default_towers(void) {
    if (count_records(TOWERS_FILE, sizeof(ParkingTower)) > 0) return 0;
    ParkingTower t;
    make_tower(&t, 1, "A Tower", 30, 6, 10, 6, 4, 4);
    if (append_record(TOWERS_FILE, &t, sizeof(t)) < 0) return -1;
    make_tower(&t, 2, "B Tower", 40, 8, 14, 8, 5, 5);
    if (append_record(TOWERS_FILE, &t, sizeof(t)) < 0) return -1;
    make_tower(&t, 3, "C Tower", 50, 10, 18, 10, 6, 6);
    if (append_record(TOWERS_FILE, &t, sizeof(t)) < 0) return -1;
    return 0;
}

// 타워 ID를 이용해 주차타워 정보를 조회하는 함수
// 조회된 타워 정보와 파일 내 위치(index)를 반환
int get_tower_by_id(int tower_id, ParkingTower *tower, int *index_out) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0) {
            if (t.id == tower_id && t.active) {
                if (tower) *tower = t;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

// 수정된 주차타워 정보를 파일에 저장하는 함수
int update_tower(const ParkingTower *tower, int index) {
    if (!tower) return -1;
    return update_record_at(TOWERS_FILE, index, tower, sizeof(ParkingTower));
}

// 특정 타워와 차종의 남은 주차 가능 대수를 계산하는 함수
// 현재 입차 차량 수와 예약 차량 수를 제외한 남은 공간을 반환
int get_available_slots(int tower_id, int car_type) {
    ParkingTower t;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return 0;
    if (!get_tower_by_id(tower_id, &t, NULL)) return 0;
    int reserved = count_active_reservations(tower_id, car_type);
    int available = t.capacity[car_type] - t.current[car_type] - reserved;
    return available > 0 ? available : 0;
}

// 차량 입차 시 해당 차종의 현재 주차 차량 수를 증가시키는 함수
// 수용 가능 대수를 초과하면 실패
int increase_tower_current(int tower_id, int car_type) {
    ParkingTower t;
    int index;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return -1;
    if (!get_tower_by_id(tower_id, &t, &index)) return -1;
    if (t.current[car_type] >= t.capacity[car_type]) return -1;
    t.current[car_type]++;
    return update_tower(&t, index);
}

// 차량 출차 시 해당 차종의 현재 주차 차량 수를 감소시키는 함수
int decrease_tower_current(int tower_id, int car_type) {
    ParkingTower t;
    int index;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return -1;
    if (!get_tower_by_id(tower_id, &t, &index)) return -1;
    if (t.current[car_type] > 0) t.current[car_type]--;
    return update_tower(&t, index);
}

// 현재 입차 중인 차량 데이터를 기준으로 각 주차타워의 현재 주차 차량 수를 다시 계산하는 함수
int rebuild_tower_current_from_reservations(void) {
    int tower_count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    for (int i = 0; i < tower_count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) != 0) continue;
        for (int type = 1; type <= CAR_TYPE_COUNT; type++) t.current[type] = 0;
        update_record_at(TOWERS_FILE, i, &t, sizeof(t));
    }

    int parking_count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < parking_count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) increase_tower_current(p.tower_id, p.car_type);
    }
    return 0;
}

// 특정 차종에 대해 가장 많은 빈 자리를 가진 주차타워를 추천하는 함수
// 남은 자리가 없는 경우 0을 반환
int recommend_tower_for_car_type(int car_type) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    int best_id = 0;
    int best_available = -1;
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            int available = get_available_slots(t.id, car_type);
            if (available > best_available) {
                best_available = available;
                best_id = t.id;
            }
        }
    }
    return best_available > 0 ? best_id : 0;
}

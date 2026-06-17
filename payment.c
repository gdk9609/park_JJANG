#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parking.h"

// 영수증 데이터에서 매출로 반영할 금액을 반환하는 내부 함수
// 총 주차요금을 그대로 매출 금액으로 사용
static int receipt_sales_amount(const Payment *p) {
    if (!p) return 0;
    return p->parking_fee;
}

// 결제 완료된 Payment 데이터를 결제 파일에 저장하는 함수
int append_payment(const Payment *payment) {
    if (!payment) return -1;
    return append_record(PAYMENTS_FILE, payment, sizeof(Payment));
}

// 현재 시각과 난수를 조합하여 고유한 영수증 번호를 생성하는 함수
// 예: E202605171530001234
char *make_receipt_number(char *buf, size_t size) {
    char timebuf[32];
    time_t t = now_time();
    struct tm *tm_ptr = localtime(&t);
    if (!buf || size == 0) return buf;
    if (tm_ptr) strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S", tm_ptr);
    else snprintf(timebuf, sizeof(timebuf), "00000000000000");
    snprintf(buf, size, "E%s%04d", timebuf, rand() % 10000);
    return buf;
}

// 결제 완료된 영수증 정보를 문자열 형태로 저장하는 함수
// 입차시간, 출차시간, 주차요금, 보증금 차감 금액, 최종 결제 금액 등을 receipts.txt 파일에 기록
void save_receipt_text(const Payment *p) {
    if (!p) return;
    char entry[64], exit_time[64], line[1024];
    format_time(p->entry_time, entry, sizeof(entry));
    format_time(p->exit_time, exit_time, sizeof(exit_time));
    snprintf(line, sizeof(line),
             "영수증번호 %s | 예약 %s | 차량 %s | 전화 %s | %c Tower | %s | 입차 %s | 출차 %s | 주차요금 %d원 | 보증금차감 %d원 | 추가결제 %d원 | 매출반영 %d원 | %s",
             p->receipt_no, p->reservation_code, p->car_number, p->phone,
             'A' + p->tower_id - 1, car_type_name(p->car_type), entry, exit_time,
             p->parking_fee, p->deposit_used, p->final_fee, receipt_sales_amount(p), p->method);
    append_text_line(RECEIPTS_FILE, line);
}

// 전체 주차장 매출 통계를 계산하는 함수
// 모든 결제 데이터를 조회하여 전체 매출과 타워 별 매출(A/B/C Tower)을 계산
int api_get_sales_summary(int *total_sales, int tower_sales[4]) {
    int total = 0;
    int towers[4] = {0, 0, 0, 0};
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    for (int i = 0; i < count; i++) {
        Payment p;
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0) {
            int sales = receipt_sales_amount(&p);
            total += sales;
            if (p.tower_id >= 1 && p.tower_id <= 3) towers[p.tower_id] += sales;
        }
    }
    if (total_sales) *total_sales = total;
    if (tower_sales) {
        for (int i = 0; i < 4; i++) tower_sales[i] = towers[i];
    }
    return 0;
}

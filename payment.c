#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parking.h"

static int receipt_sales_amount(const Payment *p) {
    if (!p) return 0;
    return p->parking_fee;
}

int append_payment(const Payment *payment) {
    if (!payment) return -1;
    return append_record(PAYMENTS_FILE, payment, sizeof(Payment));
}

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

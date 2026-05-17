#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "parking.h"

static _Thread_local char *request_data = NULL;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} ResponseBuffer;

static _Thread_local ResponseBuffer response_buffer = {NULL, 0, 0};

static void response_reset(void) {
    response_buffer.len = 0;
    if (response_buffer.data && response_buffer.cap > 0) response_buffer.data[0] = '\0';
}

static int response_reserve(size_t extra) {
    size_t need = response_buffer.len + extra + 1;
    if (need <= response_buffer.cap) return 1;
    size_t new_cap = response_buffer.cap ? response_buffer.cap : 4096;
    while (new_cap < need) new_cap *= 2;
    char *new_data = realloc(response_buffer.data, new_cap);
    if (!new_data) return 0;
    response_buffer.data = new_data;
    response_buffer.cap = new_cap;
    response_buffer.data[response_buffer.len] = '\0';
    return 1;
}

static int response_append_bytes(const char *data, size_t len) {
    if (!response_reserve(len)) return -1;
    memcpy(response_buffer.data + response_buffer.len, data, len);
    response_buffer.len += len;
    response_buffer.data[response_buffer.len] = '\0';
    return (int)len;
}

static int response_putchar(int c) {
    char ch = (char)c;
    return response_append_bytes(&ch, 1) == 1 ? c : EOF;
}

static int response_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(ap);
        return needed;
    }
    if (!response_reserve((size_t)needed)) {
        va_end(ap);
        return -1;
    }
    int written = vsnprintf(response_buffer.data + response_buffer.len,
                            response_buffer.cap - response_buffer.len, fmt, ap);
    va_end(ap);
    if (written > 0) response_buffer.len += (size_t)written;
    return written;
}

#define printf response_printf
#define putchar response_putchar

static void json_escape_print(const char *s) {
    putchar('"');
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
                case '"': printf("\\\""); break;
                case '\\': printf("\\\\"); break;
                case '\b': printf("\\b"); break;
                case '\f': printf("\\f"); break;
                case '\n': printf("\\n"); break;
                case '\r': printf("\\r"); break;
                case '\t': printf("\\t"); break;
                default:
                    if (*p < 0x20) printf("\\u%04x", *p);
                    else putchar(*p);
            }
        }
    }
    putchar('"');
}

static void print_error(const char *message) {
    printf("{\"success\":false,\"message\":");
    json_escape_print(message ? message : "요청 처리 중 오류가 발생했습니다.");
    printf("}");
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void url_decode(char *s) {
    char *src = s;
    char *dst = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static const char *get_param(const char *name) {
    static char value[2048];
    value[0] = '\0';
    if (!request_data || !name) return value;
    size_t name_len = strlen(name);
    const char *p = request_data;
    while (*p) {
        const char *key_start = p;
        const char *eq = strchr(key_start, '=');
        const char *amp = strchr(key_start, '&');
        if (!amp) amp = key_start + strlen(key_start);
        if (eq && eq < amp && (size_t)(eq - key_start) == name_len && strncmp(key_start, name, name_len) == 0) {
            size_t len = (size_t)(amp - eq - 1);
            if (len >= sizeof(value)) len = sizeof(value) - 1;
            memcpy(value, eq + 1, len);
            value[len] = '\0';
            url_decode(value);
            return value;
        }
        if (*amp == '&') p = amp + 1;
        else break;
    }
    return value;
}

static void copy_param(char *dst, size_t dst_size, const char *name) {
    if (!dst || dst_size == 0) return;
    const char *v = get_param(name);
    snprintf(dst, dst_size, "%s", v ? v : "");
}

static int parse_tower_id(const char *s) {
    if (!s || !*s) return 0;
    if (strcmp(s, "1") == 0 || s[0] == 'A' || s[0] == 'a') return 1;
    if (strcmp(s, "2") == 0 || s[0] == 'B' || s[0] == 'b') return 2;
    if (strcmp(s, "3") == 0 || s[0] == 'C' || s[0] == 'c') return 3;
    return atoi(s);
}

static int parse_car_type(const char *s) {
    if (!s || !*s) return 0;
    if (strcmp(s, "1") == 0 || strcmp(s, "경차") == 0) return CAR_COMPACT;
    if (strcmp(s, "2") == 0 || strcmp(s, "중형차") == 0) return CAR_MIDSIZE;
    if (strcmp(s, "3") == 0 || strcmp(s, "SUV") == 0 || strcmp(s, "suv") == 0) return CAR_SUV;
    if (strcmp(s, "4") == 0 || strcmp(s, "대형차") == 0) return CAR_LARGE;
    if (strcmp(s, "5") == 0 || strcmp(s, "전기차") == 0) return CAR_EV;
    return atoi(s);
}

static const char *tower_key(int tower_id) {
    if (tower_id == 1) return "A";
    if (tower_id == 2) return "B";
    if (tower_id == 3) return "C";
    return "?";
}

static void format_money(int value, char *buf, size_t size) {
    snprintf(buf, size, "%d원", value);
}

static void print_time_field(const char *name, time_t t) {
    char buf[64];
    format_time(t, buf, sizeof(buf));
    printf("\"%s\":", name);
    json_escape_print(buf);
}

static int count_parking_for_tower(int tower_id) {
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    int n = 0;
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && p.tower_id == tower_id) n++;
    }
    return n;
}

static int count_reserved_for_tower(int tower_id) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    int n = 0;
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0 && r.tower_id == tower_id && r.status == RES_RESERVED) n++;
    }
    return n;
}

static void tower_status_text(int used, int capacity, const char **status, const char **status_class) {
    if (capacity <= 0 || used >= capacity) { *status = "만차"; *status_class = "full"; return; }
    double ratio = capacity > 0 ? (double)used / (double)capacity : 1.0;
    if (ratio < 0.4) { *status = "여유"; *status_class = "good"; }
    else if (ratio < 0.8) { *status = "보통"; *status_class = "normal"; }
    else { *status = "혼잡"; *status_class = "busy"; }
}

static void print_reservation_json_fields(const Reservation *r) {
    char towername[32];
    snprintf(towername, sizeof(towername), "%c Tower", 'A' + r->tower_id - 1);
    printf(",\"reservationNo\":"); json_escape_print(r->code);
    printf(",\"carNumber\":"); json_escape_print(r->car_number);
    printf(",\"phoneNumber\":"); json_escape_print(r->phone);
    printf(",\"carType\":"); json_escape_print(car_type_name(r->car_type));
    printf(",\"carTypeId\":%d", r->car_type);
    printf(",\"towerId\":%d", r->tower_id);
    printf(",\"parkingTower\":"); json_escape_print(towername);
}

static void action_reserve(void) {
    char err[256] = "";
    char car_number[MAX_NAME], phone[MAX_PHONE], car_type_text[64], tower_text[64];
    Reservation r;
    copy_param(car_number, sizeof(car_number), "carNumber");
    copy_param(phone, sizeof(phone), "phoneNumber");
    copy_param(car_type_text, sizeof(car_type_text), "carType");
    copy_param(tower_text, sizeof(tower_text), "towerId");
    if (!*tower_text) copy_param(tower_text, sizeof(tower_text), "tower");
    int car_type = parse_car_type(car_type_text);
    int tower_id = parse_tower_id(tower_text);

    if (!api_create_reservation(car_number, phone, car_type, tower_id, &r, err, sizeof(err))) {
        print_error(err);
        return;
    }

    char money[32];
    format_money(r.deposit, money, sizeof(money));
    printf("{\"success\":true");
    print_reservation_json_fields(&r);
    printf(",\"operatingHours\":"); json_escape_print(DEFAULT_OPERATING_HOURS);
    printf(","); print_time_field("reserveTime", r.reserved_at);
    printf(",\"limitTime\":\"20분\"");
    printf(",\"deposit\":"); json_escape_print(money);
    printf("}");
}

static void action_entry(void) {
    char err[256] = "";
    char code[MAX_CODE], car_number[MAX_NAME];
    ParkingRecord p;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    copy_param(car_number, sizeof(car_number), "carNumber");
    if (!api_entry_car(code, car_number, &p, err, sizeof(err))) {
        print_error(err);
        return;
    }
    char towername[32];
    snprintf(towername, sizeof(towername), "%c Tower", 'A' + p.tower_id - 1);
    printf("{\"success\":true");
    printf(",\"reservationNo\":"); json_escape_print(p.reservation_code);
    printf(",\"carNumber\":"); json_escape_print(p.car_number);
    printf(",\"phoneNumber\":"); json_escape_print(p.phone);
    printf(",\"carType\":"); json_escape_print(car_type_name(p.car_type));
    printf(",\"carTypeId\":%d", p.car_type);
    printf(",\"towerId\":%d", p.tower_id);
    printf(",\"parkingTower\":"); json_escape_print(towername);
    printf(","); print_time_field("entryTime", p.entry_time);
    printf("}");
}

static void print_status_json(int status, const Reservation *r, const ParkingRecord *p, int remaining) {
    printf("{\"success\":true");
    if (status == RES_RESERVED && r) {
        printf(",\"status\":\"reserved\",\"statusText\":\"예약 완료\"");
        print_reservation_json_fields(r);
        printf(","); print_time_field("reserveTime", r->reserved_at);
        printf(",\"remainingSeconds\":%d", remaining);
        char rem[64]; snprintf(rem, sizeof(rem), "%d분 %d초", remaining / 60, remaining % 60);
        printf(",\"remainingText\":"); json_escape_print(rem);
    } else if (status == RES_ENTERED && p) {
        int elapsed = seconds_to_minutes_ceil(p->entry_time, now_time());
        int current_fee = calculate_parking_fee(elapsed);
        char dur[64], feeText[32], towername[32];
        format_duration_minutes(elapsed, dur, sizeof(dur));
        format_money(current_fee, feeText, sizeof(feeText));
        snprintf(towername, sizeof(towername), "%c Tower", 'A' + p->tower_id - 1);
        printf(",\"status\":\"entered\",\"statusText\":\"입차 완료\"");
        printf(",\"reservationNo\":"); json_escape_print(p->reservation_code);
        printf(",\"carNumber\":"); json_escape_print(p->car_number);
        printf(",\"phoneNumber\":"); json_escape_print(p->phone);
        printf(",\"carType\":"); json_escape_print(car_type_name(p->car_type));
        printf(",\"carTypeId\":%d", p->car_type);
        printf(",\"towerId\":%d", p->tower_id);
        printf(",\"parkingTower\":"); json_escape_print(towername);
        printf(","); print_time_field("entryTime", p->entry_time);
        printf(",\"elapsedText\":"); json_escape_print(dur);
        printf(",\"currentFee\":%d", current_fee);
        printf(",\"currentFeeText\":"); json_escape_print(feeText);
    }
    printf("}");
}

static void action_status(void) {
    char err[256] = "";
    char code[MAX_CODE];
    Reservation r;
    ParkingRecord p;
    int status = 0, remaining = 0;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    if (!api_get_active_status_by_code(code, &r, &p, &status, &remaining, err, sizeof(err))) {
        print_error(err);
        return;
    }
    print_status_json(status, &r, &p, remaining);
}

static void action_find(void) {
    char err[256] = "";
    char car_number[MAX_NAME], phone[MAX_PHONE];
    Reservation r;
    ParkingRecord p;
    int status = 0, remaining = 0;
    copy_param(car_number, sizeof(car_number), "carNumber");
    copy_param(phone, sizeof(phone), "phoneNumber");
    if (!api_find_active_by_identity(car_number, phone, &r, &p, &status, &remaining, err, sizeof(err))) {
        print_error(err);
        return;
    }
    print_status_json(status, &r, &p, remaining);
}

static void print_payment_json(const Payment *p, int completed) {
    char duration[64], money[32];
    format_duration_minutes(p->total_minutes, duration, sizeof(duration));
    printf("{\"success\":true");
    if (completed) printf(",\"completed\":true");
    printf(",\"reservationNo\":"); json_escape_print(p->reservation_code);
    printf(",\"receiptNo\":"); json_escape_print(p->receipt_no);
    printf(",\"carNumber\":"); json_escape_print(p->car_number);
    printf(",\"phoneNumber\":"); json_escape_print(p->phone);
    printf(",\"carType\":"); json_escape_print(car_type_name(p->car_type));
    printf(",\"towerId\":%d", p->tower_id);
    printf(",\"parkingTower\":"); char towername[32]; snprintf(towername, sizeof(towername), "%c Tower", 'A' + p->tower_id - 1); json_escape_print(towername);
    printf(","); print_time_field("entryTime", p->entry_time);
    printf(","); print_time_field("exitTime", p->exit_time);
    printf(",\"durationText\":"); json_escape_print(duration);
    printf(",\"totalMinutes\":%d", p->total_minutes);
    printf(",\"chargedMinutes\":%d", p->charged_minutes);
    printf(",\"parkingFee\":%d", p->parking_fee);
    printf(",\"depositUsed\":%d", p->deposit_used);
    printf(",\"finalFee\":%d", p->final_fee);
    format_money(p->parking_fee, money, sizeof(money)); printf(",\"parkingFeeText\":"); json_escape_print(money);
    format_money(p->deposit_used, money, sizeof(money)); printf(",\"depositUsedText\":"); json_escape_print(money);
    format_money(p->final_fee, money, sizeof(money)); printf(",\"finalFeeText\":"); json_escape_print(money);
    printf(",\"method\":"); json_escape_print(p->method);
    printf("}");
}

static void action_settle_preview(void) {
    char err[256] = "";
    char code[MAX_CODE];
    Payment p;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    if (!api_preview_exit_fee(code, &p, err, sizeof(err))) {
        print_error(err);
        return;
    }
    print_payment_json(&p, 0);
}

static void action_exit(void) {
    char err[256] = "";
    char code[MAX_CODE], method[MAX_METHOD];
    Payment p;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    copy_param(method, sizeof(method), "method");
    if (!api_exit_car(code, method, &p, err, sizeof(err))) {
        print_error(err);
        return;
    }
    print_payment_json(&p, 1);
}

static void action_fee_calc(void) {
    char err[256] = "";
    char entry[64], exit_time[64];
    int total = 0, charged = 0, fee = 0;
    copy_param(entry, sizeof(entry), "entryTime");
    copy_param(exit_time, sizeof(exit_time), "exitTime");
    if (!api_calculate_fee(entry, exit_time, &total, &charged, &fee, err, sizeof(err))) {
        print_error(err);
        return;
    }
    char duration[64], charged_text[64], money[32];
    format_duration_minutes(total, duration, sizeof(duration));
    format_duration_minutes(charged, charged_text, sizeof(charged_text));
    printf("{\"success\":true");
    printf(",\"totalMinutes\":%d", total);
    printf(",\"chargedMinutes\":%d", charged);
    printf(",\"durationText\":"); json_escape_print(duration);
    printf(",\"chargedText\":"); json_escape_print(charged_text);
    printf(",\"fee\":%d", fee);
    format_money(fee, money, sizeof(money)); printf(",\"feeText\":"); json_escape_print(money);
    printf(",\"baseFee\":%d", BASE_FEE);
    printf(",\"extraFee\":%d", fee > BASE_FEE ? fee - BASE_FEE : 0);
    printf("}");
}

static void action_tower_overview(void) {
    expire_old_reservations();
    rebuild_tower_current_from_reservations();
    printf("{\"success\":true,\"baseFee\":%d,\"extraUnitMinutes\":%d,\"extraUnitFee\":%d,\"towers\":[", BASE_FEE, EXTRA_UNIT_MINUTES, EXTRA_UNIT_FEE);
    int printed = 0;
    for (int id = 1; id <= 3; id++) {
        ParkingTower t;
        if (!get_tower_by_id(id, &t, NULL)) continue;
        int entered = count_parking_for_tower(id);
        int reserved = count_reserved_for_tower(id);
        int available_total = 0;
        for (int type = 1; type <= CAR_TYPE_COUNT; type++) available_total += get_available_slots(id, type);
        const char *status = "보통", *status_class = "normal";
        tower_status_text(entered + reserved, t.total_capacity, &status, &status_class);
        if (printed++) printf(",");
        printf("{\"key\":"); json_escape_print(tower_key(id));
        printf(",\"id\":%d,\"name\":", id); json_escape_print(t.name);
        printf(",\"capacity\":%d", t.total_capacity);
        printf(",\"usedTotal\":%d", entered + reserved);
        printf(",\"availableTotal\":%d", available_total);
        printf(",\"enteredCount\":%d", entered);
        printf(",\"reservedCount\":%d", reserved);
        printf(",\"operatingHours\":"); json_escape_print(t.operating_hours);
        printf(",\"status\":"); json_escape_print(status);
        printf(",\"statusClass\":"); json_escape_print(status_class);
        printf(",\"carTypes\":[");
        for (int type = 1; type <= CAR_TYPE_COUNT; type++) {
            if (type > 1) printf(",");
            int reserved_type = count_active_reservations(id, type);
            int entered_type = t.current[type];
            int used = entered_type + reserved_type;
            int available = get_available_slots(id, type);
            printf("{\"id\":%d,\"name\":", type); json_escape_print(car_type_name(type));
            printf(",\"capacity\":%d,\"entered\":%d,\"reserved\":%d,\"used\":%d,\"available\":%d}",
                   t.capacity[type], entered_type, reserved_type, used, available);
        }
        printf("]}");
    }
    printf("]}");
}

static void action_admin_summary(void) {
    expire_old_reservations();
    rebuild_tower_current_from_reservations();
    int total_sales = 0;
    int tower_sales[4] = {0};
    api_get_sales_summary(&total_sales, tower_sales);
    int total_entered = count_records(PARKING_FILE, sizeof(ParkingRecord));
    int total_reserved = count_records(RESERVATIONS_FILE, sizeof(Reservation));

    printf("{\"success\":true,\"totalEntered\":%d,\"totalReserved\":%d,\"totalSales\":%d,\"towers\":[", total_entered, total_reserved, total_sales);
    for (int id = 1; id <= 3; id++) {
        ParkingTower t;
        if (!get_tower_by_id(id, &t, NULL)) continue;
        int entered = count_parking_for_tower(id);
        int reserved = count_reserved_for_tower(id);
        const char *status = "보통", *status_class = "normal";
        tower_status_text(entered + reserved, t.total_capacity, &status, &status_class);
        if (id > 1) printf(",");
        printf("{\"key\":"); json_escape_print(tower_key(id));
        printf(",\"id\":%d,\"name\":", id); json_escape_print(t.name);
        printf(",\"capacity\":%d,\"usedTotal\":%d,\"enteredCount\":%d,\"reservedCount\":%d,\"sales\":%d",
               t.total_capacity, entered + reserved, entered, reserved, tower_sales[id]);
        printf(",\"status\":"); json_escape_print(status);
        printf(",\"statusClass\":"); json_escape_print(status_class);
        printf("}");
    }
    printf("]}");
}

static void action_admin_tower(void) {
    expire_old_reservations();
    rebuild_tower_current_from_reservations();
    char tower_text[64];
    copy_param(tower_text, sizeof(tower_text), "towerId");
    if (!*tower_text) copy_param(tower_text, sizeof(tower_text), "tower");
    int tower_id = parse_tower_id(tower_text);
    ParkingTower t;
    if (!get_tower_by_id(tower_id, &t, NULL)) {
        print_error("존재하지 않는 주차타워입니다.");
        return;
    }
    int total_sales = 0, tower_sales[4] = {0};
    api_get_sales_summary(&total_sales, tower_sales);
    int entered = count_parking_for_tower(tower_id);
    int reserved = count_reserved_for_tower(tower_id);
    const char *status = "보통", *status_class = "normal";
    tower_status_text(entered + reserved, t.total_capacity, &status, &status_class);

    printf("{\"success\":true,\"tower\":{");
    printf("\"key\":"); json_escape_print(tower_key(tower_id));
    printf(",\"id\":%d,\"name\":", tower_id); json_escape_print(t.name);
    printf(",\"capacity\":%d,\"usedTotal\":%d,\"sales\":%d,\"status\":", t.total_capacity, entered + reserved, tower_sales[tower_id]); json_escape_print(status);
    printf(",\"statusClass\":"); json_escape_print(status_class);
    printf("},\"entered\":[");

    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    int first = 1;
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && p.tower_id == tower_id) {
            int elapsed = seconds_to_minutes_ceil(p.entry_time, now_time());
            int fee = calculate_parking_fee(elapsed);
            char dur[64]; format_duration_minutes(elapsed, dur, sizeof(dur));
            if (!first) printf(",");
            first = 0;
            printf("{\"reservationNo\":"); json_escape_print(p.reservation_code);
            printf(",\"carNumber\":"); json_escape_print(p.car_number);
            printf(",\"phoneNumber\":"); json_escape_print(p.phone);
            printf(",\"carType\":"); json_escape_print(car_type_name(p.car_type));
            printf(","); print_time_field("entryTime", p.entry_time);
            printf(",\"elapsedText\":"); json_escape_print(dur);
            printf(",\"currentFee\":%d}", fee);
        }
    }
    printf("],\"reserved\":[");
    count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    first = 1;
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0 && r.tower_id == tower_id && r.status == RES_RESERVED) {
            int rem = RESERVATION_TIMEOUT_SECONDS - (int)difftime(now_time(), r.reserved_at);
            if (rem < 0) rem = 0;
            if (!first) printf(",");
            first = 0;
            printf("{\"reservationNo\":"); json_escape_print(r.code);
            printf(",\"carNumber\":"); json_escape_print(r.car_number);
            printf(",\"phoneNumber\":"); json_escape_print(r.phone);
            printf(",\"carType\":"); json_escape_print(car_type_name(r.car_type));
            printf(","); print_time_field("reserveTime", r.reserved_at);
            printf(",\"limitTime\":"); char rembuf[64]; snprintf(rembuf, sizeof(rembuf), "%d분 %d초", rem / 60, rem % 60); json_escape_print(rembuf);
            printf("}");
        }
    }
    printf("]}");
}

static void action_update_entry_time(void) {
    char err[256] = "";
    char code[MAX_CODE], new_time[64];
    ParkingRecord p;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    copy_param(new_time, sizeof(new_time), "newEntryTime");
    if (!api_update_entry_time(code, new_time, &p, err, sizeof(err))) {
        print_error(err);
        return;
    }
    printf("{\"success\":true,\"reservationNo\":"); json_escape_print(p.reservation_code);
    printf(",\"carNumber\":"); json_escape_print(p.car_number);
    printf(",\"phoneNumber\":"); json_escape_print(p.phone);
    printf(","); print_time_field("entryTime", p.entry_time);
    printf("}");
}

static void action_cancel_reservation(void) {
    char err[256] = "";
    char code[MAX_CODE];
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    if (!api_cancel_reservation(code, err, sizeof(err))) {
        print_error(err);
        return;
    }
    printf("{\"success\":true,\"message\":\"예약이 취소되었습니다.\"}");
}

static void action_change_reservation(void) {
    char err[256] = "";
    char code[MAX_CODE], car_type_text[64], tower_text[64];
    Reservation r;
    copy_param(code, sizeof(code), "reservationNo");
    if (!*code) copy_param(code, sizeof(code), "code");
    copy_param(car_type_text, sizeof(car_type_text), "carType");
    copy_param(tower_text, sizeof(tower_text), "towerId");
    int car_type = parse_car_type(car_type_text);
    int tower_id = parse_tower_id(tower_text);
    if (!api_change_reservation(code, car_type, tower_id, &r, err, sizeof(err))) {
        print_error(err);
        return;
    }
    char money[32]; format_money(r.deposit, money, sizeof(money));
    printf("{\"success\":true");
    print_reservation_json_fields(&r);
    printf(","); print_time_field("reserveTime", r.reserved_at);
    printf(",\"limitTime\":\"20분\",\"deposit\":"); json_escape_print(money);
    printf("}");
}

static void run_api_action(void) {
    if (ensure_data_files() < 0) {
        print_error("데이터 파일을 준비하지 못했습니다.");
        return;
    }
    init_default_towers();
    srand((unsigned int)(now_time() ^ (time_t)(uintptr_t)pthread_self()));

    const char *action = get_param("action");
    if (!*action) print_error("action 값이 없습니다.");
    else if (strcmp(action, "reserve") == 0) action_reserve();
    else if (strcmp(action, "entry") == 0) action_entry();
    else if (strcmp(action, "status") == 0) action_status();
    else if (strcmp(action, "find") == 0) action_find();
    else if (strcmp(action, "settle_preview") == 0) action_settle_preview();
    else if (strcmp(action, "exit") == 0) action_exit();
    else if (strcmp(action, "fee_calc") == 0) action_fee_calc();
    else if (strcmp(action, "tower_overview") == 0) action_tower_overview();
    else if (strcmp(action, "admin_summary") == 0) action_admin_summary();
    else if (strcmp(action, "admin_tower") == 0) action_admin_tower();
    else if (strcmp(action, "update_entry_time") == 0) action_update_entry_time();
    else if (strcmp(action, "cancel_reservation") == 0) action_cancel_reservation();
    else if (strcmp(action, "change_reservation") == 0) action_change_reservation();
    else print_error("지원하지 않는 action입니다.");
}

#undef printf
#undef putchar

#define SERVER_PORT 8080
#define MAX_HTTP_REQUEST 1048576

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static void send_http_response(int fd, const char *status, const char *content_type,
                               const void *body, size_t body_len) {
    char header[1024];
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.1 %s\r\n"
                     "Server: parking-c-socket-server\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: Content-Type\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     status, content_type, body_len);
    if (n > 0) send_all(fd, header, (size_t)n);
    if (body && body_len > 0) send_all(fd, body, body_len);
}

static void send_text_response(int fd, const char *status, const char *content_type, const char *body) {
    send_http_response(fd, status, content_type, body, body ? strlen(body) : 0);
}

static void send_json_error(int fd, const char *message) {
    char body[512];
    snprintf(body, sizeof(body), "{\"success\":false,\"message\":\"%s\"}", message ? message : "요청 처리 실패");
    send_text_response(fd, "400 Bad Request", "application/json; charset=utf-8", body);
}

static const char *content_type_for_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

static int is_safe_static_path(const char *path) {
    if (!path || path[0] == '\0') return 0;
    if (path[0] == '/') path++;
    if (strstr(path, "..")) return 0;
    return 1;
}

static void serve_file(int fd, const char *url_path) {
    char file_path[512];
    if (strcmp(url_path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "index.html");
    } else {
        const char *p = url_path[0] == '/' ? url_path + 1 : url_path;
        if (!is_safe_static_path(p)) {
            send_text_response(fd, "403 Forbidden", "text/plain; charset=utf-8", "Forbidden");
            return;
        }
        snprintf(file_path, sizeof(file_path), "%s", p);
    }

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        send_text_response(fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
        return;
    }

    struct stat st;
    if (fstat(file_fd, &st) < 0 || st.st_size < 0) {
        close(file_fd);
        send_text_response(fd, "500 Internal Server Error", "text/plain; charset=utf-8", "File error");
        return;
    }

    char header[1024];
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK\r\n"
                     "Server: parking-c-socket-server\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     content_type_for_path(file_path), (long long)st.st_size);
    if (n > 0) send_all(fd, header, (size_t)n);

    char buf[8192];
    ssize_t r;
    while ((r = read(file_fd, buf, sizeof(buf))) > 0) {
        if (send_all(fd, buf, (size_t)r) < 0) break;
    }
    close(file_fd);
}

static char *find_header_end(char *request, size_t len) {
    for (size_t i = 3; i < len; i++) {
        if (request[i - 3] == '\r' && request[i - 2] == '\n' && request[i - 1] == '\r' && request[i] == '\n') {
            return request + i + 1;
        }
    }
    return NULL;
}

static int parse_content_length(const char *headers) {
    const char *p = headers;
    while ((p = strcasestr(p, "Content-Length:")) != NULL) {
        if (p == headers || p[-1] == '\n') {
            p += strlen("Content-Length:");
            while (*p == ' ' || *p == '\t') p++;
            long value = strtol(p, NULL, 10);
            if (value < 0) return 0;
            if (value > MAX_HTTP_REQUEST) return MAX_HTTP_REQUEST;
            return (int)value;
        }
        p += 15;
    }
    return 0;
}

static char *read_http_request(int fd, size_t *out_len) {
    size_t cap = 8192;
    size_t len = 0;
    char *buf = malloc(cap + 1);
    if (!buf) return NULL;

    char *header_end = NULL;
    int content_length = 0;
    while (len < MAX_HTTP_REQUEST) {
        if (len == cap) {
            cap *= 2;
            if (cap > MAX_HTTP_REQUEST) cap = MAX_HTTP_REQUEST;
            char *new_buf = realloc(buf, cap + 1);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }
        ssize_t n = recv(fd, buf + len, cap - len, 0);
        if (n <= 0) break;
        len += (size_t)n;
        buf[len] = '\0';

        if (!header_end) {
            header_end = find_header_end(buf, len);
            if (header_end) content_length = parse_content_length(buf);
        }
        if (header_end) {
            size_t header_len = (size_t)(header_end - buf);
            if (len >= header_len + (size_t)content_length) break;
        }
    }

    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static int acquire_data_file_lock(void) {
    mkdir(DATA_DIR, 0755);
    int fd = open(DATA_DIR "/data.lock", O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void release_data_file_lock(int fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

static void handle_api_request(int fd, const char *param_data) {
    response_reset();
    request_data = strdup(param_data ? param_data : "");
    if (!request_data) {
        send_json_error(fd, "요청 메모리 할당에 실패했습니다.");
        return;
    }

    pthread_mutex_lock(&data_mutex);
    int lock_fd = acquire_data_file_lock();
    if (lock_fd < 0) {
        pthread_mutex_unlock(&data_mutex);
        free(request_data);
        request_data = NULL;
        send_json_error(fd, "데이터 파일 잠금에 실패했습니다.");
        return;
    }

    run_api_action();

    release_data_file_lock(lock_fd);
    pthread_mutex_unlock(&data_mutex);

    if (!response_buffer.data || response_buffer.len == 0) {
        send_json_error(fd, "빈 응답입니다.");
    } else {
        send_http_response(fd, "200 OK", "application/json; charset=utf-8", response_buffer.data, response_buffer.len);
    }

    free(request_data);
    request_data = NULL;
}

static void split_target(char *target, char **path_out, char **query_out) {
    char *q = strchr(target, '?');
    if (q) {
        *q = '\0';
        *query_out = q + 1;
    } else {
        *query_out = "";
    }
    *path_out = target;
}

static void *client_thread(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    size_t req_len = 0;
    char *request = read_http_request(fd, &req_len);
    if (!request) {
        close(fd);
        return NULL;
    }

    char method[16] = "";
    char target[2048] = "";
    if (sscanf(request, "%15s %2047s", method, target) != 2) {
        send_text_response(fd, "400 Bad Request", "text/plain; charset=utf-8", "Bad Request");
        free(request);
        close(fd);
        return NULL;
    }

    char *path = NULL;
    char *query = NULL;
    split_target(target, &path, &query);

    if (strcmp(method, "OPTIONS") == 0) {
        send_text_response(fd, "204 No Content", "text/plain; charset=utf-8", "");
    } else if ((strcmp(path, "/api") == 0 || strcmp(path, "/parking_api.cgi") == 0 || strcmp(path, "/cgi-bin/parking_api.cgi") == 0) &&
               (strcmp(method, "POST") == 0 || strcmp(method, "GET") == 0)) {
        char *header_end = find_header_end(request, req_len);
        const char *body = "";
        if (strcmp(method, "POST") == 0 && header_end) body = header_end;
        else if (strcmp(method, "GET") == 0) body = query;
        handle_api_request(fd, body);
    } else if (strcmp(method, "GET") == 0) {
        serve_file(fd, path);
    } else {
        send_text_response(fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed");
    }

    free(request);
    close(fd);
    return NULL;
}

static void chdir_to_executable_dir(const char *argv0) {
    (void)argv0;
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) return;
    path[len] = '\0';
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = '\0';
        chdir(path);
    }
}

int main(int argc, char **argv) {
    chdir_to_executable_dir(argc > 0 ? argv[0] : NULL);

    int port = SERVER_PORT;
    if (argc >= 2) {
        int parsed = atoi(argv[1]);
        if (parsed > 0 && parsed < 65536) port = parsed;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 64) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    pthread_mutex_lock(&data_mutex);
    int lock_fd = acquire_data_file_lock();
    if (lock_fd >= 0) {
        ensure_data_files();
        init_default_towers();
        release_data_file_lock(lock_fd);
    }
    pthread_mutex_unlock(&data_mutex);

    printf("Parking C socket server started: http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop.\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        int *client_ptr = malloc(sizeof(int));
        if (!client_ptr) {
            close(client_fd);
            continue;
        }
        *client_ptr = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_ptr) != 0) {
            close(client_fd);
            free(client_ptr);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}

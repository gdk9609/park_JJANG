#ifndef PARKING_H
#define PARKING_H

#include <time.h>
#include <stddef.h>

#define DATA_DIR "data"
#define TOWERS_FILE "data/towers.dat"
#define RESERVATIONS_FILE "data/reservations.dat"
#define PARKING_FILE "data/parking.dat"
#define PAYMENTS_FILE "data/receipt_records.dat"
#define LOG_FILE "data/log.txt"
#define MESSAGES_FILE "data/messages.txt"
#define RECEIPTS_FILE "data/receipts.txt"
#define SALES_REPORT_FILE "data/sales_report.csv"

#define MAX_NAME 64
#define MAX_PHONE 32
#define MAX_CODE 24
#define MAX_METHOD 32
#define MAX_RECEIPT 32
#define MAX_BUFFER 256

#define RESERVATION_TIMEOUT_SECONDS 1200
#define RESERVATION_DEPOSIT 2000

#define BASE_MINUTES 60
#define BASE_FEE 2000
#define EXTRA_UNIT_MINUTES 10
#define EXTRA_UNIT_FEE 300
#define DAILY_MAX_FEE 0

#define ADMIN_ID "JJANG"
#define DEFAULT_OPERATING_HOURS "24시간 운영"

typedef enum {
    CAR_NONE = 0,
    CAR_COMPACT = 1,
    CAR_MIDSIZE = 2,
    CAR_SUV = 3,
    CAR_LARGE = 4,
    CAR_EV = 5,
    CAR_TYPE_COUNT = 5
} CarType;

typedef enum {
    RES_CANCELLED = 0,
    RES_RESERVED = 1,
    RES_ENTERED = 2,
    RES_PAID = 3,
    RES_EXPIRED = 4
} ReservationStatus;

typedef struct {
    int id;
    char name[MAX_NAME];
    int total_capacity;
    int capacity[CAR_TYPE_COUNT + 1];
    int current[CAR_TYPE_COUNT + 1];
    char operating_hours[32];
    int active;
} ParkingTower;

typedef struct {
    int id;
    char code[MAX_CODE];
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int tower_id;
    int car_type;
    time_t reserved_at;
    time_t entry_time;
    time_t exit_time;
    int status;
    int deposit;
    int deposit_forfeited;
    int total_fee;
    int final_paid;
} Reservation;

typedef struct {
    int id;
    char reservation_code[MAX_CODE];
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int tower_id;
    int car_type;
    time_t reserved_at;
    time_t entry_time;
    int deposit;
} ParkingRecord;

typedef struct {
    int id;
    char reservation_code[MAX_CODE];
    char receipt_no[MAX_RECEIPT];
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int tower_id;
    int car_type;
    time_t entry_time;
    time_t exit_time;
    int total_minutes;
    int charged_minutes;
    int parking_fee;
    int deposit_used;
    int final_fee;
    char method[MAX_METHOD];
} Payment;

const char *car_type_name(int type);
const char *reservation_status_name(int status);
int format_phone_number(const char *input, char *out, size_t out_size);
int validate_car_number(const char *input);

void format_time(time_t t, char *buf, size_t size);
time_t now_time(void);
int seconds_to_minutes_ceil(time_t start, time_t end);
int parse_datetime(const char *s, time_t *out);
int charged_minutes_for_fee(int actual_minutes);
void format_duration_minutes(int minutes, char *buf, size_t size);

int calculate_parking_fee(int actual_minutes);
int api_calculate_fee(const char *entry_time_text, const char *exit_time_text,
                      int *total_minutes, int *charged_minutes, int *fee,
                      char *err, size_t err_size);

int ensure_data_files(void);
int append_record(const char *path, const void *record, size_t size);
int read_record_at(const char *path, int index, void *record, size_t size);
int update_record_at(const char *path, int index, const void *record, size_t size);
int delete_record_at(const char *path, int index, size_t size);
int count_records(const char *path, size_t size);
int append_text_line(const char *path, const char *line);
int delete_text_lines_matching(const char *path, const char *keyword1, const char *keyword2);

void write_log_msg(const char *action);

int init_default_towers(void);
int get_tower_by_id(int tower_id, ParkingTower *tower, int *index_out);
int update_tower(const ParkingTower *tower, int index);
int get_available_slots(int tower_id, int car_type);
int increase_tower_current(int tower_id, int car_type);
int decrease_tower_current(int tower_id, int car_type);
int recommend_tower_for_car_type(int car_type);
int rebuild_tower_current_from_reservations(void);

void expire_old_reservations(void);
int count_active_reservations(int tower_id, int car_type);
int find_reservation_by_code(const char *code, Reservation *res, int *index_out);
int update_reservation(const Reservation *res, int index);
int next_reservation_id(void);

int next_parking_id(void);
int append_parking_record(const ParkingRecord *parking);
int find_parking_by_code(const char *code, ParkingRecord *parking, int *index_out);
int delete_parking_record(int index);

int append_payment(const Payment *payment);
char *make_receipt_number(char *buf, size_t size);
void save_receipt_text(const Payment *p);
int api_get_sales_summary(int *total_sales, int tower_sales[4]);

int api_create_reservation(const char *car_number, const char *phone, int car_type, int tower_id,
                           Reservation *out, char *err, size_t err_size);
int api_cancel_reservation(const char *reservation_code, char *err, size_t err_size);
int api_change_reservation(const char *reservation_code, int car_type, int tower_id,
                           Reservation *out, char *err, size_t err_size);
int api_get_active_status_by_code(const char *code, Reservation *res_out, ParkingRecord *parking_out,
                                  int *status_out, int *remaining_seconds_out, char *err, size_t err_size);
int api_find_active_by_identity(const char *car_number, const char *phone, Reservation *res_out,
                                ParkingRecord *parking_out, int *status_out, int *remaining_seconds_out,
                                char *err, size_t err_size);
int api_entry_car(const char *reservation_code, const char *car_number, ParkingRecord *out,
                  char *err, size_t err_size);
int api_preview_exit_fee(const char *reservation_code, Payment *out, char *err, size_t err_size);
int api_exit_car(const char *reservation_code, const char *method, Payment *out, char *err, size_t err_size);
int api_update_entry_time(const char *reservation_code, const char *new_entry_time_text,
                          ParkingRecord *out, char *err, size_t err_size);

#endif

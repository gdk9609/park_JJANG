/*
 * filedb.c
 *
 * 주차 시스템에서 사용하는 파일 기반 데이터베이스 관리 기능을 담당한다.
 * 데이터 저장 폴더와 필요한 파일들을 생성하고,
 * 구조체 단위의 바이너리 데이터 파일을 추가, 읽기, 수정, 삭제할 수 있도록 한다.
 *
 * 주요 기능:
 * - 데이터 폴더 및 기본 파일 생성
 * - 손상된 레코드 파일 초기화
 * - 구조체 데이터 append/read/update/delete 처리
 * - 파일 내 레코드 개수 계산
 * - 로그, 메시지 등 텍스트 파일 줄 단위 저장 및 삭제
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "parking.h"

// 지정한 경로의 파일이 존재하지 않으면 새로 생성하는 함수
static int ensure_file_exists(const char *path) {
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

// 레코드 파일의 크기가 구조체 크기의 배수가 아니면 손상된 파일로 판단하고 초기화하는 함수
static int truncate_if_broken_record_file(const char *path, size_t size)
{
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (size == 0) return 0;

    if (st.st_size > 0 && st.st_size % (off_t)size != 0) {
        int fd = open(path, O_WRONLY | O_TRUNC);
        if (fd < 0) return -1;
        close(fd);
    }
    return 0;
}

// 주차 시스템 실행에 필요한 데이터 폴더와 파일들을 준비하는 함수
int ensure_data_files(void) {
    struct stat st;
    if (stat(DATA_DIR, &st) < 0) {
        if (mkdir(DATA_DIR, 0755) < 0) return -1;
    }

    if (ensure_file_exists(TOWERS_FILE) < 0) return -1;
    if (ensure_file_exists(RESERVATIONS_FILE) < 0) return -1;
    if (ensure_file_exists(PARKING_FILE) < 0) return -1;
    if (ensure_file_exists(PAYMENTS_FILE) < 0) return -1;
    if (ensure_file_exists(MESSAGES_FILE) < 0) return -1;
    if (ensure_file_exists(RECEIPTS_FILE) < 0) return -1;
    if (ensure_file_exists(SALES_REPORT_FILE) < 0) return -1;

    if (truncate_if_broken_record_file(TOWERS_FILE, sizeof(ParkingTower)) < 0) return -1;
    if (truncate_if_broken_record_file(RESERVATIONS_FILE, sizeof(Reservation)) < 0) return -1;
    if (truncate_if_broken_record_file(PARKING_FILE, sizeof(ParkingRecord)) < 0) return -1;
    if (truncate_if_broken_record_file(PAYMENTS_FILE, sizeof(Payment)) < 0) return -1;

    if (init_default_towers() < 0) return -1;
    rebuild_tower_current_from_reservations();
    return 0;
}

// 구조체 데이터를 파일 끝에 추가 저장하는 함수
int append_record(const char *path, const void *record, size_t size) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;
    ssize_t written = write(fd, record, size);
    close(fd);
    return written == (ssize_t)size ? 0 : -1;
}

// 파일에서 특정 인덱스 위치의 구조체 레코드를 읽어오는 함수
int read_record_at(const char *path, int index, void *record, size_t size) {
    if (index < 0) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    off_t offset = (off_t)index * (off_t)size;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    ssize_t r = read(fd, record, size);
    close(fd);
    return r == (ssize_t)size ? 0 : -1;
}

// 파일에서 특정 인덱스 위치의 구조체 레코드를 수정하는 함수
int update_record_at(const char *path, int index, const void *record, size_t size) {
    if (index < 0) return -1;
    int fd = open(path, O_RDWR);
    if (fd < 0) return -1;
    off_t offset = (off_t)index * (off_t)size;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    ssize_t w = write(fd, record, size);
    close(fd);
    return w == (ssize_t)size ? 0 : -1;
}

// 파일에서 특정 인덱스의 구조체 레코드를 삭제하는 함수
int delete_record_at(const char *path, int index, size_t size) {
    if (index < 0 || size == 0) return -1;
    int count = count_records(path, size);
    if (index >= count) return -1;

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    int src = open(path, O_RDONLY);
    if (src < 0) return -1;
    int dst = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        close(src);
        return -1;
    }

    char buffer[512];
    if (size > sizeof(buffer)) {
        close(src);
        close(dst);
        unlink(tmp_path);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        ssize_t r = read(src, buffer, size);
        if (r != (ssize_t)size) {
            close(src);
            close(dst);
            unlink(tmp_path);
            return -1;
        }
        if (i == index) continue;
        if (write(dst, buffer, size) != (ssize_t)size) {
            close(src);
            close(dst);
            unlink(tmp_path);
            return -1;
        }
    }

    close(src);
    close(dst);
    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

// 파일에 저장된 구조체 레코드 개수를 계산하는 함수
int count_records(const char *path, size_t size) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (size == 0) return 0;
    return (int)(st.st_size / (off_t)size);
}

// 텍스트 파일 끝에 한 줄을 추가하는 함수
int append_text_line(const char *path, const char *line) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;
    size_t len = strlen(line);
    if (write(fd, line, len) != (ssize_t)len) {
        close(fd);
        return -1;
    }
    if (len == 0 || line[len - 1] != '\n') {
        if (write(fd, "\n", 1) != 1) {
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

// 특정 키워드가 포함된 텍스트 줄을 삭제하는 함수
int delete_text_lines_matching(const char *path, const char *keyword1, const char *keyword2) {
    if (!path) return -1;

    FILE *src = fopen(path, "r");
    if (!src) return -1;

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *dst = fopen(tmp_path, "w");
    if (!dst) {
        fclose(src);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), src)) {
        int matched = 0;
        if (keyword1 && keyword1[0] && strstr(line, keyword1)) matched = 1;
        if (keyword2 && keyword2[0] && strstr(line, keyword2)) matched = 1;
        if (!matched) fputs(line, dst);
    }

    fclose(src);
    fclose(dst);

    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

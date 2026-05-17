#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "parking.h"

// 차량 종류 enum 값을 문자열 형태의 차량 종류명으로 변환하는 함수
const char *car_type_name(int type) {
    switch (type) {
        case CAR_COMPACT: return "경차";
        case CAR_MIDSIZE: return "중형차";
        case CAR_SUV: return "SUV";
        case CAR_LARGE: return "대형차";
        case CAR_EV: return "전기차";
        default: return "알 수 없음";
    }
}

// 예약 상태 enum 값을 문자열 형태의 상태명으로 변환하는 함수
const char *reservation_status_name(int status) {
    switch (status) {
        case RES_CANCELLED: return "취소됨";
        case RES_RESERVED: return "예약 완료";
        case RES_ENTERED: return "입차 완료";
        case RES_PAID: return "정산 완료";
        case RES_EXPIRED: return "시간 초과 취소";
        default: return "알 수 없음";
    }
}

// 전화번호 문자열을 010-0000-0000 형식으로 변환 및 검증하는 함수
// 숫자 외 문자를 제거한 뒤 휴대전화 형식인지 검사
int format_phone_number(const char *input, char *out, size_t out_size) {
    char digits[16];
    int n = 0;
    if (!input || !out || out_size < 14) return 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (isdigit((unsigned char)input[i])) {
            if (n >= 15) return 0;
            digits[n++] = input[i];
        } else if (input[i] == '-' || input[i] == ' ' || input[i] == '\t') {
            continue;
        } else {
            return 0;
        }
    }
    digits[n] = '\0';
    if (n != 11) return 0;
    if (digits[0] != '0' || digits[1] != '1' || digits[2] != '0') return 0;
    snprintf(out, out_size, "%.3s-%.4s-%.4s", digits, digits + 3, digits + 7);
    return 1;
}

// UTF-8 문자열에서 한 글자를 읽어 유니코드 코드포인트로 변환하는 내부 함수
// 읽은 문자 길이(byte 수)도 함께 반환
static int decode_utf8_one(const char *s, unsigned int *codepoint, int *consumed) {
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) return 0;
    if ((c0 & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x1F) << 6) | (unsigned int)(c1 & 0x3F);
        *consumed = 2;
        return 1;
    }
    if ((c0 & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x0F) << 12) | ((unsigned int)(c1 & 0x3F) << 6) | (unsigned int)(c2 & 0x3F);
        *consumed = 3;
        return 1;
    }
    if ((c0 & 0xF8) == 0xF0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        unsigned char c3 = (unsigned char)s[3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x07) << 18) | ((unsigned int)(c1 & 0x3F) << 12) |
                     ((unsigned int)(c2 & 0x3F) << 6) | (unsigned int)(c3 & 0x3F);
        *consumed = 4;
        return 1;
    }
    return 0;
}

// 유니코드 코드포인트가 제대로 된 한글인지 확인하는 함수
static int is_hangul_syllable_codepoint(unsigned int cp) {
    return cp >= 0xAC00 && cp <= 0xD7A3;
}

// 차량번호 형식이 올바른지 검증하는 함수
// 앞자리 숫자(2~3자리), 한글 1자, 뒤 숫자 4자리 형식인지 검사
int validate_car_number(const char *input) {
    if (!input) return 0;
    size_t len = strlen(input);
    if (len < 9) return 0;

    int pos = 0;
    int digit_prefix = 0;
    while (isdigit((unsigned char)input[pos]) && digit_prefix < 3) {
        pos++;
        digit_prefix++;
    }
    if (digit_prefix != 2 && digit_prefix != 3) return 0;

    unsigned int cp = 0;
    int consumed = 0;
    if (!decode_utf8_one(input + pos, &cp, &consumed)) return 0;
    if (!is_hangul_syllable_codepoint(cp)) return 0;
    pos += consumed;

    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)input[pos + i])) return 0;
    }
    pos += 4;
    return input[pos] == '\0';
}

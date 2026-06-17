# Linux-Based Unmanned Parking Reservation and Management System

이 프로젝트는 C 언어와 POSIX-level API를 기반으로 구현한 리눅스 기반 무인 주차 예약 및 관리 시스템이다. 사용자는 웹 브라우저를 통해 주차 예약, 입차 처리, 예약 조회, 예약 변경 및 취소, 출차 정산을 수행할 수 있다. 서버는 별도의 웹 프레임워크를 사용하지 않고 TCP 소켓 기반 HTTP 서버로 동작한다.

## 주요 기능

### 주차 예약

차량번호, 전화번호, 차종, 주차타워 정보를 입력하면 주차 예약이 생성된다. 예약이 완료되면 5자리 랜덤 예약번호가 발급된다. 예약 시 보증금 2,000원이 적용되며, 예약 후 20분 이내 입차하지 않으면 예약은 자동으로 만료된다.

### 입차 처리

입차 처리는 예약번호와 차량번호를 기준으로 이루어진다. 입력된 정보가 기존 예약 정보와 일치하면 예약 데이터는 입차 데이터로 이동한다. 입차가 완료되면 해당 주차타워의 현재 주차 대수가 자동으로 갱신된다.

### 예약 조회 및 변경

예약번호를 이용해 현재 예약 상태 또는 입차 상태를 조회할 수 있다. 차량번호와 전화번호를 이용해 예약번호를 찾을 수 있다. 예약 상태에서는 차종과 주차타워를 변경할 수 있으며, 필요에 따라 예약을 취소할 수 있다.

### 출차 및 정산

출차 정산은 입차 시간을 기준으로 주차 시간을 계산하여 이루어진다. 기본 요금은 1시간 2,000원이며, 추가 요금은 10분당 300원으로 계산된다. 최종 결제 금액은 총 주차요금에서 예약 보증금을 차감한 금액이다. 결제가 완료되면 영수증 번호가 생성되고 결제 기록이 저장된다.

### 관리자 기능

관리자 기능은 전체 입차 차량 수, 예약 수, 매출 정보를 조회하는 기능을 제공한다. 또한 A Tower, B Tower, C Tower별 입차 및 예약 현황을 확인할 수 있다. 관리자는 입차 중인 차량의 입차 시간을 수정할 수 있다.

### 파일 기반 데이터 저장

이 시스템은 파일 기반 데이터 저장 방식을 사용한다. 예약, 입차, 결제, 타워 정보는 `.dat` 파일에 구조체 단위로 저장된다. 메시지와 영수증 로그는 `.txt` 파일에 저장된다. 파일 잠금 기능을 사용하여 여러 요청이 동시에 발생할 때 데이터 충돌을 방지한다.

## 실행 환경

이 프로젝트는 Linux 또는 WSL 환경에서 실행된다. 빌드에는 GCC와 Make가 사용된다. 클라이언트 요청 처리를 위해 POSIX thread 라이브러리를 사용하며, 웹 UI는 일반 웹 브라우저에서 접근할 수 있다.

## 빌드 방법

프로젝트 루트 디렉토리에서 `make` 명령어를 사용해 프로젝트를 빌드한다.

```bash
make
```

빌드가 완료되면 `parking_server` 실행 파일이 생성된다.

## 실행 방법

서버의 기본 포트는 `8080`이다.

```bash
./parking_server
```

서버 실행 후 웹 브라우저에서 다음 주소로 접속한다.

```text
http://localhost:8080
```

포트를 직접 지정하는 경우 실행 시 포트 번호를 인자로 전달한다.

```bash
./parking_server 9090
```

이 경우 접속 주소는 다음과 같다.

```text
http://localhost:9090
```

서버는 터미널에서 `Ctrl + C`를 입력하면 종료된다.

## 데이터 초기화

저장된 데이터를 초기화할 때는 `make reset-data` 명령어를 사용한다.

```bash
make reset-data
```

이 명령은 `data` 디렉토리를 삭제한 뒤 다시 생성한다. 이후 서버를 실행하면 필요한 데이터 파일과 기본 타워 데이터가 다시 생성된다.

## 프로젝트 구조

```text
park_JJANG-Final/
├── Makefile
├── server.c            # TCP socket 기반 HTTP 서버 및 API 라우팅
├── parking.h           # 공통 상수, 구조체, 함수 선언
├── reservation.c       # 예약 생성, 조회, 변경, 취소, 만료 처리
├── parking.c           # 입차, 출차, 주차 기록 관리
├── payment.c           # 결제 기록, 영수증, 매출 계산
├── fee.c               # 주차 요금 계산
├── tower.c             # 주차타워 정보 및 잔여 대수 관리
├── admin.c             # 관리자 입차시간 수정 기능
├── filedb.c            # 파일 기반 데이터베이스 처리
├── utils.c             # 차량번호, 전화번호, 문자열 검증 유틸리티
├── time_utils.c        # 시간 변환 및 주차 시간 계산 유틸리티
├── index.html          # 웹 UI
├── logo.png            # 웹 UI 로고 이미지
└── data/
    ├── towers.dat
    ├── reservations.dat
    ├── parking.dat
    ├── receipt_records.dat
    ├── messages.txt
    ├── receipts.txt
    ├── sales_report.csv
    └── data.lock
```

## 데이터 파일 설명

| 파일 | 설명 |
|---|---|
| `data/towers.dat` | A/B/C Tower의 수용량, 차종별 가능 대수, 현재 주차 대수를 저장한다. |
| `data/reservations.dat` | 아직 입차하지 않은 예약 정보를 저장한다. |
| `data/parking.dat` | 현재 입차 중인 차량 정보를 저장한다. |
| `data/receipt_records.dat` | 결제 완료된 영수증 구조체 데이터를 저장한다. |
| `data/messages.txt` | 예약 완료 시 발송되는 메시지 기록을 저장한다. |
| `data/receipts.txt` | 결제 완료 영수증 텍스트 기록을 저장한다. |
| `data/sales_report.csv` | 매출 보고서용 CSV 데이터를 저장한다. |
| `data/data.lock` | 데이터 파일 동시 접근 방지를 위한 잠금 파일이다. |

## 주차타워 기본 정보

최초 실행 시 기본적으로 3개의 주차타워가 생성된다.

| 타워 | 총 수용량 | 경차 | 중형차 | SUV | 대형차 | 전기차 |
|---|---:|---:|---:|---:|---:|---:|
| A Tower | 30 | 6 | 10 | 6 | 4 | 4 |
| B Tower | 40 | 8 | 14 | 8 | 5 | 5 |
| C Tower | 50 | 10 | 18 | 10 | 6 | 6 |

## 요금 정책

| 항목 | 금액 |
|---|---:|
| 예약 보증금 | 2,000원 |
| 기본 요금 | 1시간 2,000원 |
| 추가 요금 | 10분당 300원 |
| 일 최대 요금 | 적용하지 않음 |

정산 시 총 주차요금에서 예약 보증금이 차감된다. 최종 결제 금액은 `총 주차요금 - 예약 보증금`으로 계산된다.

## API 개요

서버는 `/api`, `/parking_api.cgi`, `/cgi-bin/parking_api.cgi` 경로를 지원한다. 요청 방식은 `GET`과 `POST`를 모두 지원한다. 각 기능은 `action` 파라미터 값에 따라 실행된다.

| Action | 기능 | 주요 파라미터 |
|---|---|---|
| `reserve` | 예약 생성 | `carNumber`, `phoneNumber`, `carType`, `towerId` |
| `entry` | 입차 처리 | `reservationNo` 또는 `code`, `carNumber` |
| `status` | 예약번호로 상태 조회 | `reservationNo` 또는 `code` |
| `find` | 차량번호/전화번호로 예약 조회 | `carNumber`, `phoneNumber` |
| `settle_preview` | 출차 전 예상 정산 금액 조회 | `reservationNo` 또는 `code` |
| `exit` | 출차 및 결제 완료 | `reservationNo` 또는 `code`, `method` |
| `fee_calc` | 입차/출차 시간 기준 예상 요금 계산 | `entryTime`, `exitTime` |
| `tower_overview` | 전체 타워 현황 조회 | 없음 |
| `admin_summary` | 관리자 전체 요약 조회 | 없음 |
| `admin_tower` | 특정 타워 상세 조회 | `towerId` 또는 `tower` |
| `update_entry_time` | 입차시간 수정 | `reservationNo` 또는 `code`, `newEntryTime` |
| `cancel_reservation` | 예약 취소 | `reservationNo` 또는 `code` |
| `change_reservation` | 예약 차종/타워 변경 | `reservationNo` 또는 `code`, `carType`, `towerId` |

## API 요청 예시

타워 현황은 다음 요청으로 조회된다.

```bash
curl "http://localhost:8080/api?action=tower_overview"
```

예상 요금은 다음 요청으로 계산된다.

```bash
curl -X POST "http://localhost:8080/api" \
  -d "action=fee_calc&entryTime=2026-05-08 13:30:00&exitTime=2026-05-08 15:10:00"
```

출차 전 정산 금액은 다음 요청으로 조회된다.

```bash
curl -X POST "http://localhost:8080/api" \
  -d "action=settle_preview&code=예약번호"
```

## 사용한 주요 POSIX-level API

| 분류 | 사용 API | 적용 내용 |
|---|---|---|
| File I/O & Data Persistence | `open`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `mkdir` | 예약, 입차, 결제, 영수증 데이터를 파일로 저장하고 조회한다. |
| File Update & Management | `rename`, `unlink`, `flock` | 임시 파일 기반 레코드 삭제와 파일 잠금을 통한 동시 접근 방지를 구현한다. |
| Socket Programming | `socket`, `bind`, `listen`, `accept`, `send`, `recv`, `setsockopt` | TCP 기반 HTTP 서버를 구현한다. |
| Execution Environment Handling | `readlink`, `chdir` | 실행 파일 위치를 기준으로 정적 파일과 데이터 파일을 탐색한다. |

## 동시성 처리

이 프로젝트는 클라이언트 요청마다 별도의 스레드를 생성하는 방식으로 동작한다. `pthread_create()`는 클라이언트별 스레드를 생성하는 데 사용된다. `pthread_detach()`는 종료된 스레드의 자원을 자동으로 회수하는 데 사용된다. `pthread_mutex_lock()`과 `pthread_mutex_unlock()`은 프로세스 내부 데이터 처리를 보호하는 데 사용된다. `flock()`은 데이터 파일 접근을 보호하여 동시 쓰기 충돌을 방지한다.

## 웹 UI 사용 흐름

사용자는 메인 화면에 접속한 뒤 `주차장 이용` 메뉴에서 차량을 예약한다. 예약이 완료되면 예약번호가 발급되며, 사용자는 해당 예약번호로 20분 이내 입차 처리를 진행한다. `조회 및 변경` 메뉴에서는 예약 상태 또는 입차 상태를 확인할 수 있다. 예약 상태에서는 예약 변경 또는 취소가 가능하다. 출차 시에는 `출차 및 정산` 메뉴에서 요금을 확인하고 결제를 완료한다. 결제가 완료되면 영수증 정보가 제공된다.

관리자 화면은 예약번호 조회 입력칸에 `admin`을 입력하면 접근할 수 있다.

## 컴파일 정리

빌드 결과물과 오브젝트 파일은 `make clean` 명령어로 삭제된다.

```bash
make clean
```

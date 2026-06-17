<a name="readme-top"></a>

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/gdk9609/park_JJANG">
    <img src="logo.png" alt="Logo" onerror="this.src='https://cdn-icons-png.flaticon.com/512/2830/2830312.png'">
  </a>

  <h3 align="center">리눅스 기반 무인 주차 예약 및 관리 시스템</h3>

  <p align="center">
    C 언어와 POSIX API로 직접 구현한 TCP 소켓 기반 무인 주차장 웹 서버 프로젝트
    <br />
    <br />
    <a href="https://github.com/gdk9609/park_JJANG"><strong>코드 확인하기 »</strong></a>
    <br />
    <br />
    <a href="https://github.com/gdk9609/park_JJANG/issues">버그 리포트</a>
    ·
    <a href="https://github.com/gdk9609/park_JJANG/issues">기능 요청</a>
  </p>
</div>

<!-- BADGES -->
<div align="center">
  <img src="https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white" alt="C" />
  <img src="https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black" alt="Linux" />
  <img src="https://img.shields.io/badge/POSIX_API-4B32C3?style=for-the-badge&logo=gnu&logoColor=white" alt="POSIX" />
  <img src="https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white" alt="HTML5" />
</div>
<br />

<!-- TABLE OF CONTENTS -->
<details>
  <summary>📖 목차 (Table of Contents)</summary>
  <ol>
    <li><a href="#-프로젝트-소개-about-the-project">프로젝트 소개</a></li>
    <li><a href="#-주요-기능-features">주요 기능</a></li>
    <li>
      <a href="#-시작하기-getting-started">시작하기 (Getting Started)</a>
      <ul>
        <li><a href="#실행-환경">실행 환경</a></li>
        <li><a href="#빌드-및-실행">빌드 및 실행</a></li>
      </ul>
    </li>
    <li><a href="#-시스템-데이터-구조">시스템 데이터 구조</a></li>
    <li><a href="#-주차타워-및-요금-정책">주차타워 및 요금 정책</a></li>
    <li><a href="#-api-명세">API 명세</a></li>
  </ol>
</details>

<br />

## 🌟 프로젝트 소개 (About The Project)

<div align="center">
  <img width="430" alt="메인 화면" src="https://github.com/user-attachments/assets/f6307a23-e433-49f6-ac03-3aa41c5a9cbe" />
</div>
<br/>

이 프로젝트는 **C 언어와 POSIX-level API를 기반으로 구현한 리눅스 환경의 무인 주차 예약 및 관리 시스템**입니다. 
가장 큰 특징은 Nginx나 Apache 같은 별도의 웹 프레임워크를 사용하지 않고, **TCP 소켓 기반의 HTTP 서버를 직접 구현**하여 클라이언트의 요청을 처리한다는 점입니다. 

사용자는 웹 브라우저를 통해 주차 예약부터 입차, 출차, 정산까지 직관적으로 수행할 수 있습니다.

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## ✨ 주요 기능 (Features)

* 📅 **주차 예약:** 차량/전화번호, 차종, 타워를 입력하여 예약 (20분 내 미입차 시 자동 만료)
* 🚗 **입/출차 처리:** 예약번호와 차량번호를 매칭하여 입차 및 주차 대수 자동 갱신
* 🔍 **예약 조회 및 변경:** 차량/전화번호로 예약 조회 및 차종/타워 변경 지원
* 💳 **출차 및 정산:** 입차 시간을 기준으로 주차 시간을 계산하여 요금 정산 및 영수증(로그) 발급
* 👨‍💼 **관리자 기능:** 전체 주차장 현황 조회 및 입차 중인 차량의 입차 시간 강제 수정 기능
* 💾 **파일 기반 동시성 DB:** `.dat` 및 `.txt` 파일을 활용한 데이터베이스 구축 및 파일 잠금(Lock) 기능으로 동시성 문제 해결

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 🚀 시작하기 (Getting Started)

### 실행 환경
* **OS:** Linux 또는 WSL (Windows Subsystem for Linux)
* **Compiler:** GCC
* **Build Tool:** Make
* **Concurrency:** POSIX thread (`pthread`)

### 빌드 및 실행

1. **레포지토리 클론**
   ```sh
   git clone https://github.com/gdk9609/park_JJANG.git
   cd park_JJANG
   ```

2. **프로젝트 빌드**
   ```sh
   make
   ```

3. **서버 실행 (기본 포트: 8080)**
   ```sh
   ./parking_server
   ```
   *포트 번호를 지정하고 싶다면 `./parking_server 9090` 과 같이 실행하세요.*

4. **웹 브라우저 접속**
   * 접속 주소: `http://localhost:8080` (포트를 변경했다면 해당 포트 번호 입력)
   * 서버 종료는 터미널에서 `Ctrl + C`를 입력합니다.

5. **데이터 초기화 (선택)**
   ```sh
   make reset-data
   ```
   *기존 저장된 데이터를 지우고 싶을 때 사용합니다. `data` 디렉토리를 초기화하고 재생성합니다.*

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 📂 시스템 데이터 구조

<details>
<summary><b>프로젝트 디렉토리 트리 보기 (클릭하여 펼치기)</b></summary>

```text
park_JJANG/
├── Makefile
├── server.c         # TCP socket 기반 HTTP 서버 및 API 라우팅
├── parking.h        # 공통 상수, 구조체, 함수 선언
├── reservation.c    # 예약 생성, 조회, 변경, 취소, 만료 처리
├── parking.c        # 입차, 출차, 주차 기록 관리
├── payment.c        # 결제 기록, 영수증, 매출 계산
├── fee.c            # 주차 요금 계산
├── tower.c          # 주차타워 정보 및 잔여 대수 관리
├── admin.c          # 관리자 기능 (입차시간 수정 등)
├── filedb.c         # 파일 기반 데이터베이스 처리
├── utils.c          # 데이터 검증 유틸리티 (차량번호, 전화번호 등)
├── time_utils.c     # 시간 변환 및 주차 시간 계산
├── index.html       # 클라이언트 웹 UI
├── logo.png         # 웹 UI 로고 이미지
└── data/            # (파일 기반 DB 폴더)
    ├── towers.dat / reservations.dat / parking.dat / receipt_records.dat
    ├── messages.txt / receipts.txt / sales_report.csv
    └── data.lock    # 동시성 접근 방지를 위한 잠금 파일
```
</details>

### 데이터 파일 명세

| 파일명 | 설명 |
| --- | --- |
| `towers.dat` | 타워별 수용량, 차종별 가능 대수, 현재 주차 대수 |
| `reservations.dat` | 입차 대기 중인 예약 정보 구조체 |
| `parking.dat` | 현재 입차 중인 차량 정보 구조체 |
| `receipt_records.dat` | 결제 완료된 영수증 구조체 데이터 |
| `messages.txt` | 예약 완료 발송 메시지 기록 로그 |
| `receipts.txt` | 결제 완료 영수증 텍스트 기록 |
| `sales_report.csv` | 매출 통계 보고서용 데이터 |
| `data.lock` | 동시 접근 방지를 위한 뮤텍스(Mutex) 역할의 파일 |

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 🏢 주차타워 및 요금 정책

### 주차타워 기본 수용량
| 타워명 | 총 수용량 | 경차 | 중형차 | SUV | 대형차 | 전기차 |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **A Tower** | 30 | 6 | 10 | 6 | 4 | 4 |
| **B Tower** | 40 | 8 | 14 | 8 | 5 | 5 |
| **C Tower** | 50 | 10 | 18 | 10 | 6 | 6 |

### 요금 정책
| 항목 | 금액 | 비고 |
| --- | --- | --- |
| **예약 보증금** | 2,000원 | 최종 정산 시 차감 |
| **기본 요금** | 2,000원 | 최초 1시간 적용 |
| **추가 요금** | 300원 / 10분 | 기본 요금 초과 시 부과 |

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 🔌 API 명세

HTTP `GET` 및 `POST` 요청을 통해 서버와 통신합니다.

| Action | 기능 설명 | 필수 파라미터 |
| :--- | :--- | :--- |
| `reserve` | 신규 주차 예약 | `carNumber`, `phoneNumber`, `carType`, `towerId` |
| `find` | 내 예약 번호 찾기 | `carNumber`, `phoneNumber` |
| `status` | 예약/주차 상태 조회 | `reservationNo` |
| `change` | 예약 정보 변경 | `reservationNo`, `carType`, `towerId` |
| `cancel` | 주차 예약 취소 | `reservationNo` |
| `entry` | 예약 차량 입차 처리 | `reservationNo`, `carNumber` |
| `exit` | 차량 출차 및 요금 조회 | `carNumber` |
| `pay` | 요금 정산 및 결제 처리 | `carNumber`, `amount` |
| `admin` | 관리자 주차장 현황 조회 | 관리자 권한 |

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 🛠 사용한 주요 POSIX-level API

본 프로젝트는 운영체제 수준의 API를 직접 호출하여 서버를 구축하고 데이터를 관리합니다.

### 1. Network & Socket API
* `socket()`: 클라이언트의 요청을 받을 TCP 소켓 생성
* `bind()`: 소켓에 IP 주소와 포트 번호(기본 8080) 할당
* `listen()`: 클라이언트의 접속 대기 상태 설정
* `accept()`: 클라이언트 연결 요청 수락 및 통신용 새 소켓 반환
* `recv()` / `send()`: HTTP Request 메시지 수신 및 HTTP Response 데이터 송신

### 2. Thread API (Multi-threading)
* `pthread_create()`: 클라이언트 접속 시마다 새로운 스레드를 생성하여 요청 병렬 처리
* `pthread_detach()`: 종료된 스레드의 자원을 운영체제가 자동으로 회수하도록 설정

### 3. File I/O & IPC API
* `open()`, `read()`, `write()`, `close()`: `.dat` 및 `.txt` 파일을 열고 데이터를 읽고 쓰는 저수준 파일 입출력 제어
* `flock()`: 데이터 파일에 대한 동시성 제어를 위해 파일 잠금(File Lock) 적용

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>

---

## 🔄 동시성 처리 (Concurrency Handling)

웹 서버 특성상 여러 사용자가 동시에 예약이나 입차를 시도할 수 있습니다. 이를 해결하기 위해 두 가지 계층에서 동시성 제어를 구현했습니다.

1. **멀티 스레딩 (Multi-threading)**
   * 클라이언트의 연결 요청(`accept`)이 들어올 때마다 메인 프로세스가 `pthread_create()`를 통해 **새로운 워커 스레드(Worker Thread)**를 생성합니다.
   * 각 스레드는 독립적으로 클라이언트의 HTTP 요청을 분석하고 응답하므로, 여러 사용자의 동시 접속을 지연 없이 처리합니다.

2. **파일 잠금 (File Locking) - 임계 구역 보호**
   * 여러 스레드가 동시에 `reservations.dat`(예약 정보)나 `towers.dat`(주차장 현황) 파일을 수정하려고 할 때 발생하는 **경쟁 상태(Race Condition)**를 방지합니다.
   * 데이터를 읽거나 쓸 때 `flock(fd, LOCK_EX)`를 호출하여 **독점적 락(Exclusive Lock)**을 걸어, 한 번에 하나의 스레드만 파일에 접근하도록 임계 구역(Critical Section)을 보호합니다. 작업이 끝나면 `flock(fd, LOCK_UN)`으로 락을 해제합니다.

<p align="right">(<a href="#readme-top">위로 가기</a>)</p>
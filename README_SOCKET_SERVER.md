# Parking C Socket Server Version

이 버전은 Apache/CGI를 사용하지 않고, C 소켓 서버가 직접 HTTP 요청을 받도록 정리한 버전입니다.

## 실행 방법

```bash
make clean
make
./parking_server
```

기본 포트는 8080입니다.

브라우저에서 아래 주소로 접속합니다.

```text
http://localhost:8080
```

다른 포트를 쓰려면:

```bash
./parking_server 9090
```

## 구현 구조

- `server.c`: C 소켓 서버
  - `socket()`, `bind()`, `listen()`, `accept()` 사용
  - 클라이언트 접속마다 `pthread_create()`로 스레드 생성
  - `/api` 요청을 받아 기존 예약/입차/출차 API 함수 호출
  - `/`, `/index.html`, `/logo.png` 같은 정적 파일도 직접 전송
- `parking.h`, `reservation.c`, `parking.c`, `payment.c` 등 기존 기능 파일은 최대한 유지
- `index.html`의 API 주소는 `/api`로 변경

## 동시접속/파일잠금

- 동시접속 처리는 pthread 기반입니다.
- 여러 스레드가 동시에 `data/*.dat` 파일을 수정하지 않도록 API 요청 처리 구간에 `pthread_mutex`를 적용했습니다.
- 추가로 `data/data.lock` 파일에 `flock()` 기반 파일잠금을 적용했습니다.

## Apache와 다른 점

기존 Apache CGI 구조에서는 Apache가 웹 요청을 받고 `parking_api.cgi`를 실행했습니다.
이 버전에서는 `parking_server`가 계속 실행되면서 브라우저 요청을 직접 받고, JSON 응답도 `send()`로 직접 전송합니다.

# LoRa 텔레메트리 브리지 설계

> **[폐기됨]** 이 문서는 `telemetry_app`을 위한 LoRa heartbeat 브리지 설계를 기술하나,
> `telemetry_app`은 현재 시스템 기준선에 포함되어 있지 않다.
> 실제 LoRa downlink 구현은 `lora_fc_downlink_app`이 담당하며, uplink 브리지는 `bridge/lora_uplink_bridge.py`를 참조한다.
> 이 문서는 역사적 참고용으로만 보관한다.

## 1. 목적

LoRa 직렬 하트비트 입력을 `telemetry_app`이 소비하는 텔레메트리 모니터 입력 계약으로 변환하는 브리지를 정의한다.

## 2. 외부 입력

### 2.1 직렬 장치

기준 Linux 직렬 장치 규칙은 다음과 같다.

- 가능하면 `/dev/serial/by-id/...`를 사용한다.
- 기본 구성 경로로 `/dev/ttyUSB*`에 의존하지 않는다.

### 2.2 직렬 구성

기준 설정은 다음과 같다.

- baud rate: 57600
- transport type: LoRa serial
- active transport identifier: 1

### 2.3 표준 하트비트 페이로드

최소 표준 하트비트 페이로드 필드는 다음과 같다.

- `node_id`
- `seq`
- `tx_time_ms`
- `sensor_ok`
- `crc`

표준 프레임 형식:

`HB,<node_id>,<seq>,<tx_time_ms>,<sensor_ok>,<crc16_hex>`

예시:

`HB,1,42,12345,1,ABCD`

### 2.4 수동 브링업 페이로드

PuTTY 또는 동등한 직렬 터미널을 통한 운용자 테스트를 위해 브리지는 다음과 같은 단순 수동 테스트 프레임도 허용한다.

- `HB`
- `HB,<seq>`
- `HELLO`
- `HELLO,<seq>`

수동 테스트 프레임은 브링업 전용 기능이다. 이러한 프레임은 브리지 로컬 기본값과 현재 수신 시간을 사용하여 유효한 하트비트로 변환한다.

## 3. 유효 프레임 규칙

수신된 하트비트 프레임은 다음 조건을 모두 만족할 때만 유효한 것으로 간주한다.

- 프레임 파싱에 성공한다.
- 표준 프레임인 경우 CRC 검증에 성공한다.
- 시퀀스 값이 있으면 시퀀스 진행이 허용 범위 안에 있다.
- `tx_time_ms`가 수신 측 invalid jump 정책을 위반하지 않는다.

유효하지 않은 프레임은 링크 상태 타이밍을 갱신하지 않는다.

## 4. 브리지 내부 상태

브리지는 최소한 다음 상태를 유지해야 한다.

- configured serial path
- configured baud rate
- `active_transport_id`
- `last_valid_rx_time`
- `last_valid_seq`
- serial connection status
- optional bridge-local fault counter

## 5. 브리지 출력 계약

브리지는 `telemetry_app`에 필요한 텔레메트리 모니터 입력 필드를 생성해야 한다.

- `active_transport_id`
- `valid`
- `update_age_ms`

출력 규칙:

- 기준 LoRa 운용에서 `active_transport_id = 1`
- `valid = True`는 승인된 유효 하트비트 프레임에만 적용
- `update_age_ms = current_time - last_valid_rx_time`

## 6. 타이밍 동작

기준 타이밍 상수:

- nominal heartbeat period: 500 ms
- monitor evaluation period: 100 ms
- degraded timeout: 1000 ms
- lost timeout: 3000 ms

브리지는 최종 링크 상태를 분류하지 않는다. 최종 `ALIVE`, `DEGRADED`, `LOST` 분류는 `telemetry_app`의 책임이다.

## 7. 오류 처리

브리지는 다음 동작을 수행해야 한다.

- 유효하지 않은 프레임을 폐기하고 nominal link timing은 갱신하지 않는다.
- 단발성 invalid-frame 이벤트 이후에도 계속 실행한다.
- 연결 해제 또는 read failure 이후 직렬 재연결을 시도한다.
- 새로운 유효 프레임이 승인될 때까지 마지막 유효 수신 시각을 유지한다.

## 8. 재설정 및 복구 경계

브리지는 직렬 세션 복구만 담당한다.

브리지 책임:

- serial reopen
- parser recovery
- invalid-frame isolation

`telemetry_app` 책임:

- `ALIVE` / `DEGRADED` / `LOST` 분류
- recovery transition 처리
- HK 및 상태 게시

시스템 수준 재설정 정책은 브리지 외부에 있으며, 시스템 요구사항과 cFS 복구 정책이 이를 관리한다.

## 9. 초기 구현 단계

1. 직렬 포트를 연다.
2. LoRa 하트비트 프레임을 읽는다.
3. 프레임 유효성을 검사한다.
4. `last_valid_rx_time`을 갱신한다.
5. `active_transport_id`, `valid`, `update_age_ms`를 생성한다.
6. `telemetry_app` 쪽으로 텔레메트리 모니터 입력을 전달한다.

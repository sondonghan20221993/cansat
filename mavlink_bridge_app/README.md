# mavlink_bridge_app

`mavlink_bridge_app`는 ArduPilot 호환 비행제어기에서 들어오는 MAVLink 데이터를 수신하고, 선택한 메시지를 cFS Software Bus 출력으로 변환하기 위한 companion cFS 애플리케이션 스켈레톤이다.

## 의도된 책임

- FC에서 UART 또는 USB를 통해 MAVLink를 수신한다.
- 기준 MAVLink 메시지 일부를 파싱한다.
- FC EKF local state, attitude state, GPS raw state, EKF status를 게시한다.
- 장애를 link, parse, data-quality fault로 분류한다.
- 상위 복구 단계로 에스컬레이션하기 전에 재연결을 시도한다.

## 기준 MAVLink 입력

- `LOCAL_POSITION_NED`
- `ATTITUDE`
- `GPS_RAW_INT`
- `EKF_STATUS_REPORT`
- optional `ODOMETRY`

## 기준 cFS 출력

- `FC_EKF_LOCAL_STATE_MID`
- `FC_ATTITUDE_STATE_MID`
- `FC_GPS_RAW_STATE_MID`
- `FC_EKF_STATUS_MID`
- HK packet

## 현재 스켈레톤 범위

- cFS 앱 생명주기 스캐폴드
- command pipe 및 HK request 처리
- NOOP 및 Reset Counters 명령 스캐폴드
- 기준 메시지 구조와 publish helper
- reconnect, parse discard, stale-data marking을 위한 복구 placeholder

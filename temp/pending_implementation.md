# 미구현/미수정 항목 임시 기록 (temp)

작성: 2026-07-13. 정식 spec 문서(03/04/08 등)에는 아직 반영하지 않은,
구현·수정이 필요한 항목을 유실 방지용으로 여기 모아 기록한다.
착수 시 해당 리포의 정식 spec/behavior 문서로 옮기고 이 목록에서 제거한다.

## Stage 3 게이트 (lora_stage_measurement_runbook.md, Stage 1/2 실측 완료 후 착수)

1. **lora_tdm `RunRxWindow` 버퍼 static/전역화**
   - 위치: `cfs-telemetry-app/lora_tdm_app/fsw/src/lora_tdm_app.c:106-108`
   - 문제: `char Buf[LORA_TDM_APP_LINE_BUF_LEN]`가 함수 호출(RX 윈도우)마다 스택에 재선언됨 → RX 윈도우(300ms) 경계를 넘어가는 프레임이 유실
   - 확인: 2026-07-13 코드 재확인, 미착수

2. **C 파서를 길이 기반 상태머신으로 교체**
   - 위치: 동일 파일, 개행(`\n`) 종단 기반 read 루프
   - 문제: v2 바이너리(DL2/UP2/ACK2)는 종단문자가 없음, mavlink STX 버그(아래 항목)와 동일한 종류의 결함 재답습 위험
   - 확인: 2026-07-13 코드 재확인, 미착수

3. **C `LORA_TDM_APP_Crc16` ↔ Python `crc16_ccitt` 교차검증 UT**
   - C: `lora_tdm_app_utils.c:21`
   - Python: `bridge/lora_downlink_decoder.py:39`, `lora_uplink_bridge.py:37`, `lora_telemetry_bridge.py:29` (3곳 각각 구현)
   - 문제: 같은 표준 테스트 벡터로 맞대본 UT가 저장소 어디에도 없음 (2026-07-13 grep 확인, 결과 없음)
   - 확인: 미착수

4. **openMCT ws 서버 `readline()` → 바이트 스트림 상태머신**
   - 위치: `openMCT/fc_serial_ws_server.py`
   - 2번(C 파서)과 지상 측 대응 쌍. 참조 구현: `cfs-telemetry-app/bridge/lora_downlink_decoder.py`의 `DownlinkStream`
   - 확인: 미착수

## P1 (Stage 게이트와 무관, 독립 우선순위)

5. **mavlink_bridge STX 이스케이프 결함**
   - 위치: `cfs-telemetry-app/mavlink_bridge_app` MAVLink 파서
   - 문제: 페이로드 내 우연한 `0xFD`/`0xFE`를 새 프레임 시작으로 오인, 프레임 유실 (~20%/28B 프레임 추정)
   - 영향: SYSTEM_TIME(§16) 포함 모든 MAVLink 수신 경로
   - 확인: 미착수

## 문서 반영 누락 (구현은 됐으나 spec 문서 갱신 안 됨)

6. **`mission_app_runtime_spec.md` §5.1.1 MID 계약 테이블에 `FC_SYS_TIME_MID(0x1909)` 행 추가**
   - `mavlink_bridge_app_behavior_spec.md` §16.3에는 구현 완료로 갱신됨(2026-07-13, commit `38c2f22`)이나 baseline 8개 MID 테이블(§5.1.1)에는 아직 반영 안 됨

## 실측(필드 테스트) 대기 — 코드는 준비됐으나 Pi/실기 검증 없음

7. Stage 1 (ACK 실링크 검증) — `_send_ack` 구현 완료(openMCT), 실기 미검증
8. Stage 2 (TDM 주기 단축 500ms/400ms 실측 트라이얼) — 런북만 존재, 미실행
9. `FC_SYS_TIME_MID` SB 발행 Pi 실기 검증 — 로컬 UT만 통과, 실기 미검증

## 승인 대기

10. `lora_protocol_v2.py`를 openMCT git 히스토리에 반입 — auto-mode 분류기가 차단, 명시적 사용자 승인 필요. 3번 상태머신 구현이 실제로 이 파일을 쓰기 시작하는 시점과 묶어서 처리 예정

# Pi 런타임 검증 세션 (2026-07-23)

전날(2026-07-22) 발견된 BL-38/39/40 수정 + waypoint readback 신설(오늘
구현) 실기 검증. 환경: Pi 192.168.50.65(cfs.service 재배포·재기동
15:51), 지상국(Windows, fc_serial_ws_server.py + QGroundControl 동시
연결), 실내(GPS 없음, fault=3 EKF_INVALID 상시).

## BL-40 (앱 이름 상수 대문자 정정) — 실기 3종 전부 PASS

| # | 명령 | 결과 |
|---|---|---|
| 1 | RECOVERY RESTART_UPLINK | ✅ `CFE_ES_RestartApp: Restart Application UPLINK_APP Initiated`→`Completed` |
| 2 | RECOVERY RESTART_BRIDGE | ✅ Initiated→Completed(~970ms). **부수 확인**: 재시작 중 CSV에서 x/y/z/boot_ms가 정확히 6개 다운링크 사이클(seq 10244~10254, ~1.1초) 동안 완전 동일값 반복 — "끊김"이 아니라 "값 정지"가 정상 동작임을 실측 확인(lora_tdm이 매 사이클 캐시값 재전송하는 구조상 예상된 그림과 일치) |
| 3 | RECOVERY RESTART_LORA | ✅ Initiated→`LORA_TDM_APP: initialized`(재기동 로그로 확인, service 전체는 안 내려감) |

## waypoint readback (오늘 신설 기능) — 실기 PASS

- **경로**: DIAGNOSTIC 클래스(6) 자체가 지상에 한 번도 구현된 적 없었음을
  검증 중 발견 → `fc_serial_ws_server.py`에 `_handle_diagnostic()` +
  `/api/uplink/diagnostic` 신설(별도 커밋, openMCT repo).
- **waypoint 유효성 제약 재확인**(실기 REJECT_ROUTE(UFB=9) 1건 계기):
  X/Y ±50m, **고도(Z) 2.0~8.0m**, **인접 waypoint 간 거리 정확히
  2.0m(±0.0001m)** — 임의 좌표/거리로는 거부됨. 예시로 통과시킨 값:
  `(0,0,4)/(2,0,4)/(4,0,4)`(X축 2m 간격).
- **중요 정정(대화 중 오류 수정)**: "ROUTE_UPDATE는 cfs_core_app 캐시에만
  저장되고 FC에는 절대 안 올라간다"고 잘못 설명했었음 — 실제로는
  `mavlink_bridge_app`도 `ROUTE_UPDATE_MID`를 구독해 `StartMissionUpload()`로
  **실제 MAVLink MISSION_COUNT/MISSION_ITEM_INT 프로토콜을 통해 FC에
  진짜 미션 업로드**를 수행함(spec 2175행 부근에 이미 명시돼 있던
  내용, 대화 중 놓쳤던 것). 실측 로그로 확인:
  ```
  StartMissionUpload called op=1 wp=3 active=1 link=2
  no GPS ref - uploading with (0,0) origin   ← GPS 없어도 경고만, 진행됨
  rx MISSION_REQUEST ×3 crc_ok=1              ← FC가 각 waypoint 순서대로 요청
  mission upload success wp_count=3
  ```
  QGroundControl에서 "기체→다운로드"로 재조회 시 waypoint 1/2/3이
  실제로 표시됨 — FC 미션 저장소 반영 실물 확인.
- 사용자 발견: FC 위치(X/Y) 드리프트(부호 반복)는 GPS 없이 IMU만으로
  추측항법 중이라 발생하는 정상 현상(오차 누적), 속도(vx/vy/vz)는
  정지 상태에서 0 근처 잡음이라 부호 반복이 더더욱 정상.
- **미완**: 실제 readback(DIAGNOSTIC route_readback → DL2 페이지
  다운링크 → 지상 재조립) 자체는 서버 재시작 지연으로 이번 세션엔
  최종 실행까지 못 감 — mission upload 성공/QGC 표시로 "route가 FC까지
  간다"는 별개 경로는 검증됐으나, **오늘 신설한 readback 파이프라인
  자체의 실기 왕복 확인은 다음 세션 과제로 이월**.
- **미완(GUI)**: uplinkGUI/uplinkCLI에 DIAGNOSTIC 버튼/명령 없음(HTTP
  API만 가능) — 필요 시 추가.

## 🔴 신규 발견 — Pi 원인불명 재부팅 + persistent state 갭 (BL-41)

waypoint readback 실기 검증 도중 Pi가 원인불명으로 **재부팅**(하드웨어
리부팅, 16:36 KST — 커널 로그의 USB/WiFi/GPU 드라이버 재초기화로 확인,
서비스 재시작이 아니라 진짜 리부팅). `cfs_core_app`의 `MissionRoute`
캐시가 RAM 전용이라 소실 → `wp_count=0` 확인. 재부팅 원인은 journald
비영구 저장 설정 탓에 추적 불가(정직 인정) — `/var/log/journal` 생성 +
`systemctl restart systemd-journald`로 영구화 완료(재발 시 추적 가능).
CDS 사용 검토했으나 `POWER ON RESET` 시 CDS도 초기화됨을 로그로
확인 — 해법 아님. spec §12 재확인 결과 원래 설계는 파일 기반(현
BL-39 방식과 일치)이 맞았음. **8범주 지속 상태 후보 대비 구현 갭
감사 완료**(`persistent_state_gap_audit_2026-07-23.md`, BACKLOG BL-41)
— 구현은 보류, 우선순위 정의부터 다음 세션에서.

## 다음 세션 이월 항목

- [ ] **BL-41 persistent state 우선순위 정의 + 구현** (신규, 위 참조)
- [ ] waypoint readback 실제 왕복(DIAGNOSTIC 요청→DL2 페이지 수신→
      RouteReadbackAssembler 재조립) 실기 확인 — 서버 재시작 후 진행
- [ ] uplinkGUI/uplinkCLI에 DIAGNOSTIC(readback 등) 버튼/명령 추가
- [ ] BL-38(재시작 fault 체인 분리) 실기 재검증 — CI_LAB 재설정 필요
      (STOP_APP 주입 수단, 오늘 재배포로 CI_LAB 빠짐)
- [ ] BL-39(영속 상태 상대경로 수정) 실기 재검증 — boot_count 실측 증가 확인
- [ ] 어제 이월분: v2 표준화, BL-15 최종값 확정, 최종 전수 검증,
      권한검증 재검토, TDM 패킷 시각화, RT-LORA-001 재편입

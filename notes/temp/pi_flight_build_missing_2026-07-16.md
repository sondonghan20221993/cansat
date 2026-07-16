# Pi cFS 부팅 자동실행 실패 — 플라이트 빌드 누락 (2026-07-16)

## 증상
openMCT 텔레메트리 CSV가 헤더만 있고 데이터 행 0개. `fc_serial_ws_server.py`
서버/WS/HTTP는 정상 기동, WS 클라이언트 연결도 됨 — 업스트림(Pi) 문제로 특정.

## 원인
`cfs.service`(Pi 부팅시 `core-cpu1` 자동실행)가 참조하는
`~/cFS_clean/build/exe/cpu1/core-cpu1`이 없음 (`status=203/EXEC`).

지난 세션 cmake 환경 복구(`rm -rf build-ut` 사고 후 재클론) 때 유닛테스트 빌드
(`build-ut`)만 재생성했고, 실비행용 `build`(플라이트 빌드)는 누락됨.

## 수정
```bash
cd ~/cFS_clean && rm -rf build && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=debug -DSIMULATION=native -DCMAKE_INSTALL_PREFIX=/exe \
  -DMISSIONCONFIG=sample_defs -DMISSION_DEFS=/home/sdh2983/cfs-telemetry-app/mission_defs \
  ../cfe
make -j4 mission-all
make mission-install DESTDIR=/home/sdh2983/cFS_clean/build
```
- `SIMULATION=native` 필수 — 없으면 install prefix가 `/usr/local`로 잡혀 root 권한 필요
- `DESTDIR=<build 경로>` — `/exe` 절대경로를 build 하위로 재매핑, root 없이 설치
- `make -f cfe/cmake/Makefile.sample prep` 래퍼는 `MISSIONCONFIG` 값이 `sample`일 때
  `sample_defs`로 자동 접미사 처리되어 프로젝트 고유 `mission_defs`를 못 씀 —
  raw `cmake` 직접 호출로 우회

## 결과
`sudo systemctl restart` 없이도 `Restart=on-failure`(5초 간격) 자동재시도로
서비스 스스로 복구. `core-cpu1` 정상 기동, `LORA_TDM_APP`/`CFS_CORE_APP`/
`MAVLINK_BRIDGE_APP` 전부 활성, PC 텔레메트리 CSV 실시간 적재 확인(261행+).

## 부가 발견 (미해결)
로그에 `LORA_TDM_APP: ACK seq mismatch tx=227 rx=223~225` 반복 발생.
`lora_tdm_seq_fail_eid_logic_gap.md`에서 수정한 SeqEcho 검증 로직이 실제로
작동 중이라는 증거이나, mismatch 자체가 지속되면 링크 품질/재전송 원인 규명 필요
(원인 미파악, 후속 확인 대상).

## 관련
- `/etc/systemd/system/cfs.service` (Pi)
- `notes/temp/lora_tdm_seq_fail_eid_logic_gap.md`
- `camera/correlate_video_telemetry.py` (텔레메트리 CSV 정상화로 실측 테스트 가능해짐)

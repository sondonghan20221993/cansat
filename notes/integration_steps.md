# telemetry_app 재통합 절차

## 1. 공식 cFS 작업공간 준비

공식 `nasa/cFS`를 복제하거나 포크한 뒤, 먼저 기준 미션을 빌드한다.

## 2. 작업공간에 앱 복사

이 저장소의 `telemetry_app/` 디렉터리를 다음 경로로 복사한다.

```text
apps/telemetry_app
```

보조 스크립트는 다음 경로로 복사한다.

```text
tools/telemetry_app_e2e_sender.py
```

## 3. 샘플 미션 정의에 앱 등록

다음 파일을 갱신한다.

- `sample_defs/targets.cmake`
- `sample_defs/cpu1_cfe_es_startup.scr`

앱 목록과 startup script에 `telemetry_app`을 추가한다.

## 4. 빌드

일반적인 빌드 흐름:

```bash
make SIMULATION=native prep
make -j$(nproc)
make install
```

## 5. 런타임 검증

다음을 실행한다.

```bash
build/exe/cpu1/core-cpu1
```

기대 결과:

- `telemetry_app.so`가 로드된다.
- `Telemetry App Initialized...` 로그가 출력된다.
- 시스템이 `OPERATIONAL` 상태에 도달한다.

## 6. E2E 검증

다음을 사용한다.

```bash
python3 tools/telemetry_app_e2e_sender.py
```

검증 대상:

- `TELEMETRY_MONITOR_MID` 주입
- `ALIVE -> DEGRADED -> LOST -> recovery`
- HK/상태 페이로드 갱신

## 7. 단위 테스트 검증

다음으로 구성한다.

```bash
cmake -S cfe -B build/unit_telemetry -DENABLE_UNIT_TESTS=ON
```

그 다음 `telemetry_app` 관련 test runner를 빌드하고 실행한다.

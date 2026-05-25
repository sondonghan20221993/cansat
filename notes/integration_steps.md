# baseline app 재통합 절차

현재 baseline app set은 다음을 기준으로 한다.

- `mavlink_bridge_app`
- `cfs_core_app`
- `downlink_app` (`lora_fc_downlink_app` 기반)
- `uplink_app`

`telemetry_app`과 `img_app`은 현재 baseline app set에 포함하지 않는다.

## 1. 공식 cFS 작업공간 준비

공식 `nasa/cFS`를 복제하거나 포크한 뒤, 먼저 기준 미션을 빌드한다.

## 2. 작업공간에 앱 복사

다음 디렉터리를 cFS 작업공간 앱 경로로 복사한다.

```text
apps/mavlink_bridge_app
apps/cfs_core_app
apps/downlink_app
apps/uplink_app
```

현재 저장소 기준으로는 `downlink_app` 역할을 `lora_fc_downlink_app/`이 담당하므로,
실제 통합 시에는 앱 이름 정렬 또는 경로 정리가 추가로 필요하다.

## 3. 미션 정의에 앱 등록

다음 파일을 갱신한다.

- `sample_defs/targets.cmake`
- `sample_defs/cpu1_cfe_es_startup.scr`

앱 목록과 startup script에 baseline app set을 추가한다.

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

- baseline app shared object가 로드된다.
- 각 앱 초기화 로그가 출력된다.
- 시스템이 `OPERATIONAL` 상태에 도달한다.

## 6. E2E 검증

검증 대상:

- UART 기반 MAVLink 수신 및 상태 MID publish
- `mavlink_bridge_app -> cfs_core_app -> downlink_app` 경로 확인
- `uplink_app` 명령 수신, 검증, 상태 갱신 확인
- HK 및 상태 페이로드 갱신

## 7. 단위 테스트 검증

다음으로 구성한다.

```bash
cmake -S cfe -B build/unit_telemetry -DENABLE_UNIT_TESTS=ON
```

그 다음 baseline app set 관련 test runner를 빌드하고 실행한다.

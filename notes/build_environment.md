# 빌드 환경 안내

## 환경 구성

| 항목 | 경로 |
|---|---|
| cFS 프레임워크 | `~/Desktop/cFS_clean` |
| 소스 저장소 | `~/cfs-telemetry-app` |
| unit-test 빌드 디렉토리 | `~/Desktop/cFS_clean/build-ut` |

---

## PC (단위테스트 실행) 환경 설정

unit-test는 PC에서 native로 실행한다.
`~/Desktop/cFS_clean/sample_defs/targets.cmake`에서 `cpu1_SYSTEM`을 `native`로 설정한다.

```cmake
SET(cpu1_SYSTEM native)   # PC unit-test용
```

빌드 초기화 (처음 한 번):
```bash
cd ~/Desktop/cFS_clean/build-ut
cmake ~/Desktop/cFS_clean/cfe
make -j4
```

그 후 각 앱 unit-test 빌드 및 실행:
```bash
# 소스를 cFS_clean에 동기화
cp -r ~/cfs-telemetry-app/<app>/fsw/       ~/Desktop/cFS_clean/apps/<app>/fsw/
cp -r ~/cfs-telemetry-app/<app>/unit-test/ ~/Desktop/cFS_clean/apps/<app>/unit-test/

# 빌드
cd ~/Desktop/cFS_clean/build-ut/native/default_cpu1/apps/<app>/unit-test
make -j4

# 실행
./coverage-<app>-<module>-testrunner
```

---

## Raspberry Pi (실물 배포) 환경 설정

Pi에 배포할 바이너리는 i686 또는 ARM 크로스컴파일 타겟이 필요하다.
`~/Desktop/cFS_clean/sample_defs/targets.cmake`에서 `cpu1_SYSTEM`을 Pi 아키텍처로 설정한다.

```cmake
SET(cpu1_SYSTEM i686-linux-gnu)   # Raspberry Pi용 (32-bit)
# 또는 ARM Pi인 경우
# SET(cpu1_SYSTEM arm-linux-gnueabihf)
```

### 런타임 실행 시 `sudo` 필요 (2026-06-16 확인)

`core-cpu1`을 일반 사용자 권한으로 실행하면 다음 에러로 즉시 종료된다.

```
CFE_PSP: OS_API_Init() failure
```

**원인**: `OS_API_Init()` → `OS_API_Impl_Init()` → `OS_Posix_TableMutex_Init()`
(`osal/src/os/posix/src/os-impl-idmap.c`)에서 mutex 속성에
`pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT)`를 설정하는데,
이 우선순위 상속(priority-inheritance) mutex 속성은 일반 사용자 권한에서는 설정이
거부될 수 있다. `sudo`로 실행하면 정상 동작한다.

```bash
cd ~/cFS_clean/build/exe/cpu1
sudo ./core-cpu1 2>&1 | tee /tmp/cfs_run.log
```

---

## 환경 전환 방법

targets.cmake 수정 후 빌드 디렉토리를 재초기화해야 한다.

```bash
# build-ut 재초기화
cd ~/Desktop/cFS_clean/build-ut
rm -rf native i686-linux-gnu   # 기존 빌드 결과 삭제
cmake ~/Desktop/cFS_clean/cfe
make -j4
```

---

## 현재 권장 워크플로 (PC 단위테스트)

```bash
# 1. 소스 동기화
cp -r ~/cfs-telemetry-app/<app>/fsw/  ~/Desktop/cFS_clean/apps/<app>/fsw/
cp -r ~/cfs-telemetry-app/<app>/unit-test/ ~/Desktop/cFS_clean/apps/<app>/unit-test/

# 2. 빌드 (native 타겟 기준)
cd ~/Desktop/cFS_clean/build-ut/native/default_cpu1/apps/<app>/unit-test
make -j4

# 3. 전체 testrunner 실행
for f in ./coverage-<app>-*-testrunner; do $f; done

# 4. Python pytest
cd ~/cfs-telemetry-app
.venv/bin/python -m pytest tests/ -v
```

---

## 주의사항

- `targets.cmake`의 `cpu1_SYSTEM`을 바꾸면 cmake 재초기화가 필요하다.
- `cmake` 실행 시 `~/Desktop/cFS_clean/cfe`를 소스 디렉토리로 지정해야 한다.
- `native` 타겟 빌드 결과는 `build-ut/native/default_cpu1/` 경로에 생성된다.
- `i686-linux-gnu` 타겟 빌드 결과는 `build-ut/i686-linux-gnu/default_cpu1/` 경로에 생성된다.

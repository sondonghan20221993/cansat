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

## 현재 권장 워크플로 (Pi 실기 빌드·배포) — 2026-07-28 추가

⚠️ **`make`만 하면 실기에 반영되지 않는다.** 빌드 산출물은 스테이징 디렉토리에
생기고, cFS가 실제로 실행하는 `build/exe/cpu1/`에는 별도 설치 단계로 복사된다.
2026-07-28에 이 단계 누락으로 **소스는 최신인데 3일 전(7/25) 바이너리가 계속
돌아가는** 상황이 발생했다(BL-70/71 프로토콜 변경이 실기에 미반영 → waypoint
readback이 지상에서 영원히 조립 안 됨). 재발 방지를 위해 절차를 명문화한다.

```bash
# 1. 빌드 (Pi에서, 타겟명 주의 — 그냥 `make`는 도구류만 빌드하고 앱은 안 건드림)
cd ~/cFS_clean/build
make native_default_cpu1-all -j4

# 2. 설치 — ⚠️ `make mission-install`은 현재 이 환경에서 실패한다(아래 주의사항)
#    실패 시 아래처럼 수동 복사한다. core-cpu1은 실행 중이면 "Text file busy"가
#    나므로 반드시 서비스를 먼저 정지한다.
sudo systemctl stop cfs.service
cp ~/cFS_clean/build/native/default_cpu1/cpu1/core-cpu1 \
   ~/cFS_clean/build/exe/cpu1/core-cpu1
cp ~/cFS_clean/build/native/default_cpu1/apps/<app>/<app>.so \
   ~/cFS_clean/build/exe/cpu1/cf/<app>.so
sudo systemctl start cfs.service

# 3. 배포 검증 — mtime이 방금 시각인지 반드시 눈으로 확인할 것
ls -la ~/cFS_clean/build/exe/cpu1/core-cpu1 ~/cFS_clean/build/exe/cpu1/cf/*.so
systemctl is-active cfs.service
```

**검증 없이 "배포 완료"로 기록하지 말 것** — 3단계의 mtime 확인이 이번 사고의
유일한 조기 발견 수단이었다.

---

## 주의사항

- ⚠️ **`make mission-install` 실패(2026-07-28 발견, 미해결)**: `CMakeCache.txt`의
  `CMAKE_INSTALL_PREFIX`가 `/exe`(루트 밑 절대경로)로 잡혀 있어
  `file cannot create directory: /exe/cpu1/cf. Maybe need administrative
  privileges.`로 실패한다. 현재는 위 워크플로처럼 수동 `cp`로 우회 중이며,
  **prefix를 `~/cFS_clean/build`로 정정하는 것이 근본 해결**(cmake 재초기화 필요).
  우회를 계속 쓰면 빌드할 때마다 같은 함정을 반복한다.
- `make`(타겟 미지정)는 `elf2cfetbl`, `cfeconfig_platformdata_tool` 등 **도구류만**
  빌드하고 앱/코어는 손대지 않는다 — 앱 반영을 원하면 `native_default_cpu1-all`.
- `targets.cmake`의 `cpu1_SYSTEM`을 바꾸면 cmake 재초기화가 필요하다.
- `cmake` 실행 시 `~/Desktop/cFS_clean/cfe`를 소스 디렉토리로 지정해야 한다.
- `native` 타겟 빌드 결과는 `build-ut/native/default_cpu1/` 경로에 생성된다.
- `i686-linux-gnu` 타겟 빌드 결과는 `build-ut/i686-linux-gnu/default_cpu1/` 경로에 생성된다.

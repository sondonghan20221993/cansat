# cfs_core_app — 명령 캐싱만 구현, 실행 미구현

## 배경

`cfs_core_app_behavior_spec.md` §20 "알려진 미구현 항목" 참조:

## 미구현 명령들

### 1. VIEWPOINT_CMD (0x190D)
- **구현**: 페이로드(type/frame/X/Y/Z/Yaw/Pitch/HoldTime) → `ViewpointCmd` 캐시 저장, `VIEWPOINT_EID` 발생
- **미구현**: 실제 viewpoint 위치 설정/전환 로직

### 2. RECOVERY_CMD (0x190C)
- **구현**: payload 무관 무조건 `RecoveryStartMs=0`, `BridgeRestartCount=0` 리셋, `RECOVERY_CMD_EID` 발생
- **미구현**: `recovery_action`/`target_component` 필드 검증 및 구분 처리 (mission spec §18.4.6.4)

### 3. MODE_CMD (0x190F)
- **구현**: `Payload[0]` → `LastModeValue` 캐시, `MODE_CMD_EID` 발생
- **미구현**: 상태 전이 로직, 허용 전이 검증

### 4. 타임스탐프 유효성 (§7)
- **구현**: 미래 타임스탐프 거부(`Msg->TimestampMs > NowMs + 5000ms`)
- **미구현**: 타임스탐프 **기준/출처**(time base) 유효성 검사

## 상태

- [x] 명령 페이로드 파싱 (cached)
- [ ] VIEWPOINT 실행 로직
- [ ] RECOVERY 필드 검증 및 구분 처리
- [ ] MODE 상태 전이 검증
- [ ] 타임스탐프 time base 검증

## 참고

- `cfs_core_app/fsw/src/cfs_core_app_cmds.c` (명령 핸들러)
- `cfs_core_app/fsw/src/cfs_core_app.h` (상태 구조체)

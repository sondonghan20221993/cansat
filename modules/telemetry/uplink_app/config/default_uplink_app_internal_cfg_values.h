#ifndef DEFAULT_UPLINK_APP_INTERNAL_CFG_VALUES_H
#define DEFAULT_UPLINK_APP_INTERNAL_CFG_VALUES_H

#define UPLINK_APP_PLATFORM_PIPE_DEPTH  16
#define UPLINK_APP_PLATFORM_PIPE_NAME   "UPLINK_CMD"
#define UPLINK_APP_SB_POLL_TIMEOUT_MS   200
#define UPLINK_APP_PROTOTYPE_PERIOD_MS  1000
#define UPLINK_APP_PROTOCOL_VERSION     1
#define UPLINK_APP_MAX_PAYLOAD_LENGTH   196
/* BL-39(2026-07-23): 절대경로 "/cf/..."는 raw POSIX open()에서 OSAL 가상
 * 경로 매핑을 거치지 않아 Pi 실파일시스템에 없는 "/cf"를 그대로 찾다
 * ENOENT로 실패했다(SaveState 무동작, 실측 확인). 상대경로로 바꿔
 * cfs.service의 WorkingDirectory(~/cFS_clean/build/exe/cpu1) 기준
 * cf/ — EEPROM.DAT 등이 이미 쓰는 실제 경로와 일치시킨다. */
#define UPLINK_APP_STATE_FILE_PATH      "cf/uplink_app_state.bin"

#define UPLINK_APP_VIEWPOINT_VERSION    1
#define UPLINK_APP_VIEWPOINT_MAX_TYPE   2   /* 0=absolute 1=relative 2=track */
#define UPLINK_APP_VIEWPOINT_MAX_FRAME  0   /* 0=LOCAL_NED only */
#define UPLINK_APP_VIEWPOINT_MIN_X_M    (-50.0f)
#define UPLINK_APP_VIEWPOINT_MAX_X_M    50.0f
#define UPLINK_APP_VIEWPOINT_MIN_Y_M    (-50.0f)
#define UPLINK_APP_VIEWPOINT_MAX_Y_M    50.0f
#define UPLINK_APP_VIEWPOINT_MIN_ALT_M  2.0f
#define UPLINK_APP_VIEWPOINT_MAX_ALT_M  8.0f
#define UPLINK_APP_VIEWPOINT_MAX_YAW    3.14159265f
#define UPLINK_APP_VIEWPOINT_MAX_PITCH  1.57079632f
#define UPLINK_APP_VIEWPOINT_MAX_HOLD_MS 30000U

/* UP 프레임 flags 필드(현재 예약, 항상 0으로 옴) 비트0 — §18.10.1 health state
 * 게이팅을 명령 단위로 강제 통과. 매 명령마다 지상에서 명시적으로 세워야 하고,
 * 컴파일타임 기본값이 아니라 무선으로 실제 오는 값이라 흔적(로그)이 항상 남는다. */
#define UPLINK_APP_FORCE_FLAG  0x01U

#endif


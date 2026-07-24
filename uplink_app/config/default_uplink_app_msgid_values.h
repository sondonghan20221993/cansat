#ifndef DEFAULT_UPLINK_APP_MSGID_VALUES_H
#define DEFAULT_UPLINK_APP_MSGID_VALUES_H

#include "uplink_app_interface_cfg_values.h"

#define UPLINK_APP_HK_TLM_MID 0x08D0
#define UPLINK_STATUS_MID     0x190A
#define ROUTE_UPDATE_MID      0x190B
#define RECOVERY_CMD_MID      0x190C
#define VIEWPOINT_CMD_MID     0x190D
#define CONFIG_CMD_MID        0x190E
#define MODE_CMD_MID          0x190F
#define DIAGNOSTIC_CMD_MID    0x1910
#define EXEC_RESULT_MID       0x1912 /* BL-08(2026-07-22): 대상앱→uplink_app 실행결과 회신 (공용, shared_msgs/exec_result_msg.h) */
#define SYSTEM_HEALTH_MID_VALUE 0x1904
#define BRIDGE_HK_MID_VALUE     0x08A0

/* counter management(§18.4.6.7, 2026-07-22) 직접 라우팅용 — 대상 앱 CMD_MID 및
 * 각 앱이 이미 보유한 RESET_COUNTERS CC(전부 1). 새 MID/CC를 대상 앱에
 * 추가하지 않고, cfs_core_app의 P1-a SendBridgeCtrlCmd와 동일하게
 * CFE_MSG_SetFcnCode로 기존 CMD_MID에 얹어 보낸다. */
#define UPLINK_APP_COUNTER_TARGET_MAVLINK_BRIDGE_CMD_MID 0x18A0U
#define UPLINK_APP_COUNTER_TARGET_CFS_CORE_CMD_MID       0x18C0U
#define UPLINK_APP_COUNTER_TARGET_LORA_TDM_CMD_MID       0x18E0U
#define UPLINK_APP_COUNTER_TARGET_RESET_COUNTERS_CC      1U

/* BL-44(2026-07-24): flight mode base 명령(§18.4.6.8) — mavlink_bridge_app CMD_MID에
 * FcnCode(SET_FLIGHT_MODE_CC=5, mavlink_bridge_app 쪽 정의와 값 일치 유지 필요)를
 * 얹어 직접 전송. counter management와 동일 패턴(cfs_core_app 미경유). */
#define UPLINK_APP_FLIGHT_MODE_TARGET_CMD_MID            0x18A0U
#define UPLINK_APP_FLIGHT_MODE_SET_FLIGHT_MODE_CC        5U

#endif


#ifndef DEFAULT_CFS_CORE_APP_MSGID_VALUES_H
#define DEFAULT_CFS_CORE_APP_MSGID_VALUES_H

#include "cfs_core_app_interface_cfg_values.h"

#define CFS_CORE_APP_HK_TLM_MID 0x08C0
#define SYSTEM_HEALTH_MID       0x1904
#define ROUTE_UPDATE_MID        0x190B
#define RECOVERY_CMD_MID        0x190CU
#define VIEWPOINT_CMD_MID       0x190DU
#define CONFIG_CMD_MID          0x190EU
#define MODE_CMD_MID            0x190FU
#define EXEC_RESULT_MID          0x1912U /* BL-08(2026-07-22): 대상앱→uplink_app 실행결과 회신 (공용) */

/* ground_controllable_capability_plan P1-a(2026-07-22): RECOVERY의
 * PARSER_RESET/SERIAL_RECONNECT를 mavlink_bridge_app에 실제 전달하기 위해
 * 그 앱의 기존 CMD_MID를 그대로 참조(신규 MID 신설 대신 FcnCode로 구분 —
 * mavlink_bridge_app 자신도 이미 이 MID 안에서 NOOP/RESET_COUNTERS/
 * MISSION_QUERY를 FcnCode로 구분하는 방식과 일관). 값은
 * mavlink_bridge_app/config/default_mavlink_bridge_app_interface_cfg_values.h와
 * 반드시 동일하게 유지해야 함(공유 헤더 없음, 관례상 각 앱이 독립적으로
 * MID 값을 재선언 — 이 저장소 전역 컨벤션). */
#define MAVLINK_BRIDGE_APP_CMD_MID_VALUE 0x18A0U
#define MAVLINK_BRIDGE_APP_PARSER_RESET_CC     3U
#define MAVLINK_BRIDGE_APP_SERIAL_RECONNECT_CC 4U

#endif


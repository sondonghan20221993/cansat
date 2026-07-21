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

#endif


#ifndef DEFAULT_MAVLINK_BRIDGE_APP_MSGID_VALUES_H
#define DEFAULT_MAVLINK_BRIDGE_APP_MSGID_VALUES_H

#include "mavlink_bridge_app_interface_cfg_values.h"

#define MAVLINK_BRIDGE_APP_HK_TLM_MID 0x08A0
#define ROUTE_UPDATE_MID              0x190BU
#define CONFIG_CMD_MID                0x190EU
#define EXEC_RESULT_MID               0x1912U /* BL-08(2026-07-22): 대상앱→uplink_app 실행결과 회신 (공용) */

#endif

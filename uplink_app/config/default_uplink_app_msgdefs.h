#ifndef DEFAULT_UPLINK_APP_MSGDEFS_H
#define DEFAULT_UPLINK_APP_MSGDEFS_H

typedef enum
{
    UPLINK_APP_RESULT_NONE           = 0,
    UPLINK_APP_RESULT_ACCEPT         = 1,
    UPLINK_APP_RESULT_REJECT         = 2,
    UPLINK_APP_RESULT_ROUTED         = 3,
    UPLINK_APP_RESULT_FAILED         = 4,
    UPLINK_APP_RESULT_REJECT_VERSION = 5,
    UPLINK_APP_RESULT_REJECT_CLASS   = 6,
    UPLINK_APP_RESULT_REJECT_LENGTH  = 7,
    UPLINK_APP_RESULT_ROUTE_MISS     = 8,
    UPLINK_APP_RESULT_REJECT_ROUTE   = 9,
    UPLINK_APP_RESULT_REJECT_SEQUENCE  = 10,
    UPLINK_APP_RESULT_REJECT_STATE     = 11,
    UPLINK_APP_RESULT_REJECT_CHECKSUM  = 12,
    UPLINK_APP_RESULT_REJECT_VIEWPOINT = 13,
    UPLINK_APP_RESULT_DUPLICATE        = 14, /* seq == last accepted: 4x 재전송 슬롯 중복, replay 아님 (BL-01) */
    UPLINK_APP_RESULT_EXECUTED_OK      = 15, /* 대상앱 EXEC_RESULT 회신: 처리 성공 (BL-08) */
    UPLINK_APP_RESULT_EXECUTED_FAILED  = 16, /* 대상앱 EXEC_RESULT 회신: 처리 실패 (BL-08) */
    UPLINK_APP_RESULT_REJECT_COUNTER   = 17, /* counter management: scope/action 오류 (§18.4.6.7, 2026-07-22) */
    UPLINK_APP_RESULT_REJECT_FLIGHT_MODE = 18 /* flight mode: flight_mode/waypoint_start_index 오류 (BL-44, §18.4.6.8) */
} UPLINK_APP_Result_t;

typedef enum
{
    UPLINK_APP_LINK_NOMINAL  = 0,
    UPLINK_APP_LINK_DEGRADED = 1,
    UPLINK_APP_LINK_LOST     = 2,
    UPLINK_APP_LINK_FAILED   = 3
} UPLINK_APP_LinkState_t;

typedef enum
{
    UPLINK_APP_CLASS_NONE         = 0,
    UPLINK_APP_CLASS_CONFIG       = 1,
    UPLINK_APP_CLASS_ROUTE_UPDATE = 2,
    UPLINK_APP_CLASS_VIEWPOINT    = 3,
    UPLINK_APP_CLASS_RECOVERY     = 4,
    UPLINK_APP_CLASS_MODE         = 5,
    UPLINK_APP_CLASS_DIAGNOSTIC   = 6,
    UPLINK_APP_CLASS_COUNTER_MGMT = 7,
    UPLINK_APP_CLASS_FLIGHT_MODE  = 8 /* BL-44(2026-07-24): FC 비행모드 base 명령 (§18.4.6.8) */
} UPLINK_APP_CommandClass_t;

/* BL-44(2026-07-24): flight mode payload의 flight_mode 값 (§18.4.6.8) */
typedef enum
{
    UPLINK_APP_FLIGHT_MODE_HOVER    = 0,
    UPLINK_APP_FLIGHT_MODE_WAYPOINT = 1,
    UPLINK_APP_FLIGHT_MODE_LAND     = 2
} UPLINK_APP_FlightMode_t;

typedef enum
{
    UPLINK_APP_COUNTER_SCOPE_MAVLINK_BRIDGE = 1,
    UPLINK_APP_COUNTER_SCOPE_CFS_CORE       = 2,
    UPLINK_APP_COUNTER_SCOPE_UPLINK         = 3,
    UPLINK_APP_COUNTER_SCOPE_LORA_TDM       = 4
} UPLINK_APP_CounterScope_t;

typedef enum
{
    UPLINK_APP_ROUTE_OP_NONE    = 0,
    UPLINK_APP_ROUTE_OP_REPLACE = 1,
    UPLINK_APP_ROUTE_OP_APPEND  = 2,
    UPLINK_APP_ROUTE_OP_DELETE  = 3
} UPLINK_APP_RouteOpType_t;

typedef enum
{
    UPLINK_APP_ROUTE_NONE          = 0,
    UPLINK_APP_ROUTE_CORE          = 1,
    UPLINK_APP_ROUTE_DOWNLINK      = 2,
    UPLINK_APP_ROUTE_COUNTER_MGMT  = 3,
    UPLINK_APP_ROUTE_FLIGHT_MODE   = 4 /* BL-44: mavlink_bridge 직접 라우팅(cfs_core 미경유), §18.4.6.8 */
} UPLINK_APP_RouteTarget_t;

typedef enum
{
    UPLINK_APP_CONFIG_IDLE       = 0,
    UPLINK_APP_CONFIG_PENDING    = 1,
    UPLINK_APP_CONFIG_VALIDATING = 2,
    UPLINK_APP_CONFIG_REJECTED   = 3
} UPLINK_APP_ConfigPendingState_t;

#endif


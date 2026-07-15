#ifndef DEFAULT_CFS_CORE_APP_MSGSTRUCT_H
#define DEFAULT_CFS_CORE_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "cfs_core_app_msgdefs.h"
#include "cfs_core_app_mission_cfg.h"
#include "system_health_msg.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} CFS_CORE_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} CFS_CORE_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint8                     MissionRouteWaypointCount;
    uint8                     LandingRouteWaypointCount;
    uint32                    PublishCount;
    uint32                    LastPublishTimestampMs;
    uint32                    LastRouteUpdateTimestampMs;
    uint32                    RouteUpdateCount;
} CFS_CORE_APP_HkTlm_t;

typedef INPUT_STATUS_t  CFS_CORE_APP_InputStatus_t;
typedef BRIDGE_STATUS_t CFS_CORE_APP_BridgeStatus_t;
typedef APP_STATUS_t    CFS_CORE_APP_AppStatus_t;
typedef SYSTEM_HEALTH_TLM_t CFS_CORE_APP_SystemHealthTlm_t;

typedef struct
{
    float X;
    float Y;
    float Z;
} CFS_CORE_APP_Waypoint_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    SourceSequence;
    uint8                     RouteType;
    uint8                     RouteVersion;
    uint8                     WaypointCount;
    uint8                     Reserved;
    CFS_CORE_APP_Waypoint_t   Waypoints[CFS_CORE_APP_ROUTE_MAX_WAYPOINTS];
} CFS_CORE_APP_RouteUpdateTlm_t;

/* CONFIG_CMD_MID 수신용 — uplink_app의 UPLINK_APP_ConfigCmdTlm_t와 동일 레이아웃 */
#define CFS_CORE_APP_CONFIG_MAX_PAYLOAD 196

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     PayloadLength;
    uint8                     Reserved;
    uint8                     Payload[CFS_CORE_APP_CONFIG_MAX_PAYLOAD];
} CFS_CORE_APP_ConfigCmdTlm_t;

/* RECOVERY_CMD_MID 수신용 — uplink_app의 UPLINK_APP_RecoveryCmdTlm_t와 동일 레이아웃 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     RecoveryAction;
    uint8                     TargetComponent;
    uint16                    ReasonCode;
    uint32                    RequestToken;
} CFS_CORE_APP_RecoveryCmdTlm_t;

/* MODE_CMD_MID 수신용 — uplink_app의 UPLINK_APP_ModeCmdTlm_t와 동일 레이아웃 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     ModeAction;
    uint8                     RequestedState;
    uint32                    RequestToken;
} CFS_CORE_APP_ModeCmdTlm_t;

/* VIEWPOINT_CMD_MID 수신용 — uplink_app의 UPLINK_APP_ViewpointCmdTlm_t와 동일 레이아웃 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     ViewpointType;
    uint8                     PositionFrame;
    float                     X;
    float                     Y;
    float                     Z;
    float                     Yaw;
    float                     Pitch;
    uint32                    HoldTimeMs;
} CFS_CORE_APP_ViewpointCmdTlm_t;

/* config payload 내부 헤더 */
typedef struct
{
    uint8  ConfigScope;
    uint8  ConfigVersion;
    uint16 ParameterId;
    uint8  ValueType;
    uint8  ValueLength;
    uint16 Checksum;   /* uint16 additive sum: scope+version+param_id(2B)+value_type+value_length+value_bytes */
} CFS_CORE_APP_ConfigPayloadHdr_t;

#endif


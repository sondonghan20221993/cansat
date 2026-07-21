#ifndef DEFAULT_UPLINK_APP_MSGSTRUCT_H
#define DEFAULT_UPLINK_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "uplink_app_msgdefs.h"
#include "uplink_app_mission_cfg.h"
#include "uplink_app_internal_cfg_values.h" /* UPLINK_APP_MAX_PAYLOAD_LENGTH — msgstruct 자립화 */
#include "route_msg.h"
#include "config_msg.h"
#include "bridge_hk_msg.h"
#include "system_health_msg.h"
#include "uplink_fwd_cmd_msg.h"
#include "diagnostic_cmd_msg.h"
#include "recovery_cmd_msg.h"
#include "exec_result_msg.h"
#include "mode_cmd_msg.h"
#include "viewpoint_cmd_msg.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} UPLINK_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} UPLINK_APP_ResetCountersCmd_t;

typedef UPLINK_FWD_CMD_TLM_t UPLINK_APP_ProcessUplinkCmd_t;

typedef ROUTE_WAYPOINT_t UPLINK_APP_Waypoint_t;

typedef struct
{
    uint8  RouteType;
    uint8  RouteVersion;
    uint8  WaypointCount;
    uint8  Reserved;
    UPLINK_APP_Waypoint_t Waypoints[ROUTE_MAX_WAYPOINTS];
} UPLINK_APP_RouteUpdatePayload_t;

typedef ROUTE_UPDATE_TLM_t UPLINK_APP_RouteUpdateTlm_t;

typedef RECOVERY_CMD_TLM_t UPLINK_APP_RecoveryCmdTlm_t;

typedef EXEC_RESULT_TLM_t UPLINK_APP_ExecResultTlm_t;

typedef struct
{
    uint8  ViewpointType;    /* 0=absolute 1=relative 2=track */
    uint8  ViewpointVersion; /* must == UPLINK_APP_VIEWPOINT_VERSION */
    uint8  PositionFrame;    /* 0=LOCAL_NED */
    uint8  Reserved;
    float  X;
    float  Y;
    float  Z;
    float  Yaw;
    float  Pitch;
    uint32 HoldTimeMs;
} UPLINK_APP_ViewpointPayload_t;

typedef VIEWPOINT_CMD_TLM_t UPLINK_APP_ViewpointCmdTlm_t;

typedef CONFIG_CMD_TLM_t UPLINK_APP_ConfigCmdTlm_t;

/* config payload 내부 헤더 (openMCT fc_serial_ws_server.py _build_config_payload와 동일 레이아웃) */
typedef struct
{
    uint8  ConfigScope;
    uint8  ConfigVersion;
    uint16 ParameterId;
    uint8  ValueType;
    uint8  ValueLength;
    uint16 Checksum; /* additive sum: scope+version+param_id(2B)+value_type+value_length+value_bytes */
} UPLINK_APP_ConfigPayloadHdr_t;

typedef MODE_CMD_TLM_t UPLINK_APP_ModeCmdTlm_t;

typedef DIAGNOSTIC_CMD_TLM_t UPLINK_APP_DiagnosticCmdTlm_t;

typedef SYSTEM_HEALTH_TLM_t UPLINK_APP_SysHealthMirror_t;

typedef BRIDGE_HK_TLM_t UPLINK_APP_BridgeHkMirror_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint16                    Reserved;
    uint32                    PublishCount;
    uint32                    LastPublishTimestampMs;
} UPLINK_APP_HkTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastRxTimestampMs;
    uint32                    AcceptedCount;
    uint32                    RejectedCount;
    uint32                    RoutingFailureCount;
    uint16                    LastCommandCode;
    uint16                    LastCommandSequence;
    uint8                     LastCommandResult;
    uint8                     LinkState;
    uint8                     Valid;
    uint8                     ActiveTransportId;
    uint8                     LastRouteTarget;
    uint8                     ConfigPendingState;
    uint8                     LastConfigResult;
    uint8                     LastRollbackReason;
    uint8                     FcMissionResult;             /* FC MISSION_ACK 결과 (BRIDGE_HK LastUploadResult 캐시) */
    uint8                     FcMissionUploadState;        /* 0=IDLE 1=ACTIVE (BRIDGE_HK 최신 수신 여부 기반) */
    uint8                     BootCount;                   /* BL-03/BL-12: 부팅마다 +1(uint8 wrap), DL2 프레임에 동봉되어 지상 재부팅 감지에 쓰임 */
    uint32                    FcMissionUploadSuccessCount; /* BRIDGE_HK MissionUploadSuccessCount 캐시 */
    uint16                    LastAcceptedSequence;        /* BL-03: 지상 자가복구용 — 기체가 다음에 accept할 최소 seq(N+1의 근거), LastCommandSequence(수신시도)와 별개 */
} UPLINK_APP_StatusTlm_t;

#endif


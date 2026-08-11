#ifndef DEFAULT_CFS_CORE_APP_MSGSTRUCT_H
#define DEFAULT_CFS_CORE_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "cfs_core_app_msgdefs.h"
#include "cfs_core_app_mission_cfg.h"
#include "fc_state_msg.h"
#include "system_health_msg.h"
#include "route_msg.h"
#include "config_msg.h"
#include "recovery_cmd_msg.h"
#include "mode_cmd_msg.h"
#include "viewpoint_cmd_msg.h"
#include "exec_result_msg.h"
#include "diagnostic_cmd_msg.h"
#include "route_snapshot_msg.h"

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
    uint32                    PublishCount;
    uint32                    LastPublishTimestampMs;
    uint32                    LastRouteUpdateTimestampMs;
    uint32                    RouteUpdateCount;
    /* BL-43(2026-07-23): 앱 상태 보고 — 재시작 누계(영속) + 마지막 fault (spec §12.3) */
    uint32                    BridgeRestartCount;
    uint32                    UplinkRestartCount;
    uint32                    LoraRestartCount;
    uint8                     LastFaultCode;
    uint8                     Bl43Reserved[3];
    uint32                    TimebaseShiftCount; /* BL-42(2026-07-24): FC 재부팅/time base 불연속 누계 */
} CFS_CORE_APP_HkTlm_t;

typedef INPUT_STATUS_t  CFS_CORE_APP_InputStatus_t;
typedef BRIDGE_STATUS_t CFS_CORE_APP_BridgeStatus_t;
typedef APP_STATUS_t    CFS_CORE_APP_AppStatus_t;
typedef SYSTEM_HEALTH_TLM_t CFS_CORE_APP_SystemHealthTlm_t;

typedef FC_ATTITUDE_TLM_t   CFS_CORE_APP_AttitudeTlm_t;
typedef FC_EKF_LOCAL_TLM_t  CFS_CORE_APP_EkfLocalTlm_t;
typedef FC_GPS_RAW_TLM_t    CFS_CORE_APP_GpsRawTlm_t;
typedef FC_EKF_STATUS_TLM_t CFS_CORE_APP_EkfStatusTlm_t;

typedef ROUTE_WAYPOINT_t CFS_CORE_APP_Waypoint_t;
typedef ROUTE_UPDATE_TLM_t CFS_CORE_APP_RouteUpdateTlm_t;

/* CONFIG_CMD_MID 수신용 — uplink_app의 UPLINK_APP_ConfigCmdTlm_t와 동일 레이아웃 */
typedef CONFIG_CMD_TLM_t CFS_CORE_APP_ConfigCmdTlm_t;

/* RECOVERY_CMD_MID 수신용 — shared_msgs 단일 진실 (2026-07 병합) */
typedef RECOVERY_CMD_TLM_t CFS_CORE_APP_RecoveryCmdTlm_t;

typedef EXEC_RESULT_TLM_t CFS_CORE_APP_ExecResultTlm_t;

/* MODE_CMD_MID 수신용 — shared_msgs 단일 진실 (2026-07 병합) */
typedef MODE_CMD_TLM_t CFS_CORE_APP_ModeCmdTlm_t;

/* VIEWPOINT_CMD_MID 수신용 — shared_msgs 단일 진실 (2026-07 병합) */
typedef VIEWPOINT_CMD_TLM_t CFS_CORE_APP_ViewpointCmdTlm_t;

/* DIAGNOSTIC_CMD_MID 수신용 — shared_msgs 단일 진실, lora_tdm_app과 공동구독
 * (waypoint readback, 2026-07-23) */
typedef DIAGNOSTIC_CMD_TLM_t CFS_CORE_APP_DiagnosticCmdTlm_t;

/* ROUTE_SNAPSHOT_MID 발행용 — waypoint readback(2026-07-23) */
typedef ROUTE_SNAPSHOT_TLM_t CFS_CORE_APP_RouteSnapshotTlm_t;

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


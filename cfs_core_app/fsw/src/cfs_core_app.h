#ifndef CFS_CORE_APP_H
#define CFS_CORE_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "cfs_core_app_mission_cfg.h"
#include "cfs_core_app_platform_cfg.h"
#include "cfs_core_app_internal_cfg.h"

#include "cfs_core_app_perfids.h"
#include "cfs_core_app_msg.h"
#include "cfs_core_app_msgids.h"
#include "cfs_core_app_topicids.h"

typedef struct
{
    uint32 TimestampMs;
    uint32 Seq;
    uint8  Valid;
    uint8  Stale;
    uint8  ErrorCode;
    bool   Received;
} CFS_CORE_APP_StateCache_t;

typedef struct
{
    uint8  LinkState;
    uint8  LastErrorCode;
    uint32 LastRxTimestampMs;
    bool   Received;
} CFS_CORE_APP_BridgeCache_t;

typedef struct
{
    uint32 LastHkRxMs;
    bool   Received;
    uint8  Pad[3];
} CFS_CORE_APP_AppHkCache_t;

typedef struct
{
    uint32                  TimestampMs;
    uint32                  SourceSequence;
    uint32                  UpdateCount;
    uint8                   RouteType;
    uint8                   RouteVersion;
    uint8                   WaypointCount;
    bool                    Valid;
    CFS_CORE_APP_Waypoint_t Waypoints[CFS_CORE_APP_ROUTE_MAX_WAYPOINTS];
} CFS_CORE_APP_RouteCache_t;

typedef struct
{
    uint32 TimestampMs;
    uint32 SourceSequence;
    uint8  ViewpointType;
    uint8  PositionFrame;
    bool   Valid;
    float  X;
    float  Y;
    float  Z;
    float  Yaw;
    float  Pitch;
    uint32 HoldTimeMs;
} CFS_CORE_APP_ViewpointCache_t;

/* ground_controllable_capability_plan P1-a(2026-07-22): mavlink_bridge_app의
 * CMD_MID로 보낼 payload 없는 명령(CommandHeader만) — PARSER_RESET/
 * SERIAL_RECONNECT 둘 다 이 구조로 충분, FcnCode만 전송 시점에 다르게 설정 */
typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} CFS_CORE_APP_BridgeCtrlCmd_t;

typedef struct
{
    uint8                           CmdCounter;
    uint8                           ErrCounter;
    uint8                           LastHealthState;
    uint8                           ConfigPendingState;
    uint8                           LastConfigResult;
    uint8                           LastRollbackReason;
    uint16                          ConfigReserved;
    uint32                          ConfigGeneration;
    CFS_CORE_APP_ConfigParams_t     ActiveConfig;
    CFS_CORE_APP_ConfigParams_t     PendingConfig;
    CFS_CORE_APP_ConfigParams_t     PreviousConfig;
    uint32                      SeqRejectedCount;
    uint32                      SeqGapCount;
    uint32                      TimestampRejectedCount;
    uint32                      BridgeRestartCount;
    uint32                      NextBridgeRestartMs;
    uint32                      UplinkRestartCount;
    uint32                      NextUplinkRestartMs;
    uint32                      LoraRestartCount;
    uint32                      NextLoraRestartMs;
    uint8                       LastFaultCode; /* BL-43(2026-07-23): 마지막 fault 코드 (영속·HK 노출) */
    uint32                      NominalEligibleSince;
    uint32                      RecoveryStartMs;
    uint32                      RecoveryRequestedCount;
    uint8                       CurrentModeState;
    uint32                      LastModeRequestToken;
    uint32                      RunStatus;
    uint32                      SequenceCounter;
    uint32                      PublishCount;
    uint32                      LastPublishTimeMs;
    CFE_SB_PipeId_t             CommandPipe;
    CFS_CORE_APP_StateCache_t   AttitudeState;
    CFS_CORE_APP_StateCache_t   LocalState;
    CFS_CORE_APP_StateCache_t   GpsState;
    CFS_CORE_APP_StateCache_t   EkfState;
    CFS_CORE_APP_BridgeCache_t  BridgeState;
    CFS_CORE_APP_AppHkCache_t   UplinkAppState;
    CFS_CORE_APP_AppHkCache_t   LoraAppState;
    CFS_CORE_APP_RouteCache_t      MissionRoute;
    CFS_CORE_APP_RouteCache_t      LandingRoute;
    CFS_CORE_APP_ViewpointCache_t  ViewpointCmd;
    CFS_CORE_APP_HkTlm_t           HkTlm;
    CFS_CORE_APP_SystemHealthTlm_t SystemHealthTlm;
    CFS_CORE_APP_ExecResultTlm_t   ExecResultTlm; /* BL-08(2026-07-22) */
    CFS_CORE_APP_BridgeCtrlCmd_t   BridgeCtrlCmd; /* P1-a(2026-07-22) */
    CFS_CORE_APP_RouteSnapshotTlm_t RouteSnapshotTlm; /* waypoint readback(2026-07-23) */
} CFS_CORE_APP_Data_t;

extern CFS_CORE_APP_Data_t CFS_CORE_APP_Data;

void         CFS_CORE_APP_Main(void);
CFE_Status_t CFS_CORE_APP_Init(void);
void         CFS_CORE_APP_ReportHousekeeping(void);
bool         CFS_CORE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         CFS_CORE_APP_ServicePrototype(void);
void         CFS_CORE_APP_ProcessStateMessage(CFE_SB_Buffer_t *SBBufPtr);
void         CFS_CORE_APP_UpdateHealth(uint32 NowMs, bool ForcePublish);

#endif


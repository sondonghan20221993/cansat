#ifndef MAVLINK_BRIDGE_APP_H
#define MAVLINK_BRIDGE_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "mavlink_bridge_app_mission_cfg.h"
#include "mavlink_bridge_app_platform_cfg.h"
#include "mavlink_bridge_app_internal_cfg.h"

#include "mavlink_bridge_app_perfids.h"
#include "mavlink_bridge_app_msg.h"
#include "mavlink_bridge_app_msgids.h"

typedef struct
{
    uint8     CmdCounter;
    uint8     ErrCounter;
    uint16    Reserved;
    uint32    RunStatus;
    uint32    SequenceCounter;
    uint32    ReconnectIntervalMs;
    uint32    LastRxTimestampMs;
    uint32    LastReconnectAttemptMs;
    uint32    LastHeartbeatTxMs;
    uint32    LastStreamRequestMs;
    uint32    TargetDiscoveryStartMs;
    uint32    LastAttitudeRxMs;
    uint32    LastEkfLocalRxMs;
    uint32    LastGpsRawRxMs;
    uint32    LastEkfStatusRxMs;
    uint32    LastSysTimeRxMs;
    uint32    SysTimePad;
    uint64    LastSysTimeUnixUsec;
    uint32    ReconnectAttemptCount;
    uint32    ParseErrorCount;
    uint32    NonFiniteValueCount;
    uint32    BytesReceived;
    int32     SerialFd;
    uint8     TargetSystemId;
    uint8     TargetComponentId;
    uint8     StreamRequestPending;
    uint8     StreamRequestAckCount;
    uint8     LinkState;
    uint8     LastErrorCode;
    uint16    Spare;
    uint8     MissionUploadState;
    uint8     MissionUploadRetry;
    uint8     MissionUploadWpCount;
    uint8     MissionSpare;
    uint32    MissionUploadTimeoutMs;
    uint32    MissionUploadSuccessCount;
    uint32    MissionUploadFailCount;
    uint32    LastUploadTimestampMs;
    uint8     LastUploadWaypointCount;
    uint8     LastUploadResult;
    uint16    MissionPad;
    /* BL-56(2026-07-25): waypoint가 항상 절대좌표(LatE7/LonE7)+CmdType+Param1~4로 확장되어
     * 로컬 X/Y/Z pending/active 배열을 CmdType/Param/LatE7/LonE7/Z 배열로 교체한다. */
    uint8     MissionPendingCmdType[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingParam1[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingParam2[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingParam3[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingParam4[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    int32     MissionPendingLatE7[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    int32     MissionPendingLonE7[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingZ[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    uint8     ActiveWaypointCmdType[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointParam1[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointParam2[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointParam3[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointParam4[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    int32     ActiveWaypointLatE7[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    int32     ActiveWaypointLonE7[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointZ[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    uint8     ActiveWaypointCount;
    /* BL-56(2026-07-25): PX4가 업로드 트랜잭션 내 current=1 항목 seq를 재개 인덱스로 채택
     * (§18.4.6.2) — FC의 MISSION_CURRENT(msg #42)로 갱신, 첫 부팅 시 0. */
    uint8     ActiveResumeIndex;
    uint8     ActiveWaypointPad[2];
    uint8     MissionDownloadState;
    uint8     MissionDownloadSeq;
    uint8     MissionDownloadExpectedCount;
    uint8     MissionDownloadSpare;
    uint32    MissionDownloadTimeoutMs;

    /* BL-41 route(2026-07-23): FC 미션 readback — 다운로드 항목을 로컬 미터로
     * 역변환해 버퍼링 후 완료 시 FC_MISSION_READBACK_MID(0x1914) 게시.
     * timeout 시 지수 백오프(1→2→4→5s 상한) 무한 재시도 (mavlink spec §10). */
    ROUTE_WAYPOINT_t MissionDownloadWaypoints[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    uint8     MissionReadbackPending;
    uint8     MissionReadbackPad[3];
    uint32    MissionReadbackBackoffMs;
    uint32    MissionReadbackNextRetryMs;
    uint32    MissionReadbackSeq;
    ROUTE_UPDATE_TLM_t FcMissionReadbackTlm;
    uint8     FcBaseMode;
    uint8     FcSystemStatus;
    uint8     IsArmed;
    uint8     FcStatePad;
    uint8                              ConfigPendingState;
    uint8                              LastConfigResult;
    uint8                              LastRollbackReason;
    uint8                              ConfigReserved;
    uint32                             ConfigGeneration;
    MAVLINK_BRIDGE_APP_ConfigParams_t  ActiveConfig;
    MAVLINK_BRIDGE_APP_ConfigParams_t  PendingConfig;
    MAVLINK_BRIDGE_APP_ConfigParams_t  PreviousConfig;
    CFE_SB_PipeId_t CommandPipe;
    MAVLINK_BRIDGE_APP_HkTlm_t        HkTlm;
    MAVLINK_BRIDGE_APP_EkfLocalTlm_t  EkfLocalTlm;
    MAVLINK_BRIDGE_APP_AttitudeTlm_t  AttitudeTlm;
    MAVLINK_BRIDGE_APP_GpsRawTlm_t    GpsRawTlm;
    MAVLINK_BRIDGE_APP_EkfStatusTlm_t EkfStatusTlm;
    MAVLINK_BRIDGE_APP_SysTimeTlm_t   SysTimeTlm;
    MAVLINK_BRIDGE_APP_ExecResultTlm_t ExecResultTlm; /* BL-08(2026-07-22) */
} MAVLINK_BRIDGE_APP_Data_t;

extern MAVLINK_BRIDGE_APP_Data_t MAVLINK_BRIDGE_APP_Data;

void         MAV_BRIDGE_APP_Main(void);
CFE_Status_t MAVLINK_BRIDGE_APP_Init(void);
void         MAVLINK_BRIDGE_APP_ReportHousekeeping(void);
bool         MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         MAVLINK_BRIDGE_APP_ServiceSerial(void);
void         MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(uint8 BaseMode, uint8 SystemStatus);

#endif

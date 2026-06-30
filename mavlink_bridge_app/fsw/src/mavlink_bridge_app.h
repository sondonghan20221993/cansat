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
    uint32    ReconnectAttemptCount;
    uint32    ParseErrorCount;
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
    float     MissionPendingX[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingY[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     MissionPendingZ[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointX[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointY[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    float     ActiveWaypointZ[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
    uint8     ActiveWaypointCount;
    uint8     ActiveWaypointPad[3];
    uint8     MissionDownloadState;
    uint8     MissionDownloadSeq;
    uint8     MissionDownloadExpectedCount;
    uint8     MissionDownloadSpare;
    uint32    MissionDownloadTimeoutMs;
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
} MAVLINK_BRIDGE_APP_Data_t;

extern MAVLINK_BRIDGE_APP_Data_t MAVLINK_BRIDGE_APP_Data;

void         MAV_BRIDGE_APP_Main(void);
CFE_Status_t MAVLINK_BRIDGE_APP_Init(void);
void         MAVLINK_BRIDGE_APP_ReportHousekeeping(void);
bool         MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         MAVLINK_BRIDGE_APP_ServiceSerial(void);
void         MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(uint8 BaseMode, uint8 SystemStatus);

#endif

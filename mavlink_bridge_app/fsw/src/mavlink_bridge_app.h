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
    uint32    ReconnectAttemptCount;
    uint32    ParseErrorCount;
    uint32    BytesReceived;
    int32     SerialFd;
    int32     LoRaFd;
    uint32    LoRaTxCount;
    uint8     LinkState;
    uint8     LastErrorCode;
    uint16    Spare;
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

#endif

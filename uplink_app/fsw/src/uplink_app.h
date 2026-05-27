#ifndef UPLINK_APP_H
#define UPLINK_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "uplink_app_mission_cfg.h"
#include "uplink_app_platform_cfg.h"
#include "uplink_app_internal_cfg.h"

#include "uplink_app_perfids.h"
#include "uplink_app_msg.h"
#include "uplink_app_msgids.h"

typedef struct
{
    uint8                  CmdCounter;
    uint8                  ErrCounter;
    uint16                 Reserved;
    uint32                 RunStatus;
    uint32                 SequenceCounter;
    uint32                 PublishCount;
    uint32                 LastPublishTimeMs;
    uint32                 LastRxTimeMs;
    uint32                 AcceptedCount;
    uint32                 RejectedCount;
    uint32                 RoutingFailureCount;
    uint32                 LastAcceptedSequence;
    uint16                 LastCommandCode;
    uint16                 LastRxSequence;
    uint8                  LastCommandResult;
    uint8                  LastRouteTarget;
    uint8                  LinkState;
    uint8                  Valid;
    uint8                  ActiveTransportId;
    uint8                  ConfigPendingState;
    uint8                  LastConfigResult;
    uint8                  LastRollbackReason;
    CFE_SB_PipeId_t        CommandPipe;
    UPLINK_APP_HkTlm_t     HkTlm;
    UPLINK_APP_StatusTlm_t StatusTlm;
} UPLINK_APP_Data_t;

extern UPLINK_APP_Data_t UPLINK_APP_Data;

void         UPLINK_APP_Main(void);
CFE_Status_t UPLINK_APP_Init(void);
void         UPLINK_APP_ReportHousekeeping(void);
bool         UPLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         UPLINK_APP_ServicePrototype(void);
void         UPLINK_APP_ProcessUplink(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ValidateProxyCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_Result_t *Result);
UPLINK_APP_RouteTarget_t UPLINK_APP_ResolveRouteTarget(uint8 CommandClass);
bool         UPLINK_APP_ParseRouteUpdatePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                                UPLINK_APP_RouteUpdatePayload_t *Payload);
bool         UPLINK_APP_PublishRouteUpdate(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                           const UPLINK_APP_RouteUpdatePayload_t *Payload);
bool         UPLINK_APP_ForwardRecoveryCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ForwardViewpointCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ForwardConfigCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
void         UPLINK_APP_LoadState(void);
void         UPLINK_APP_SaveState(void);

#endif


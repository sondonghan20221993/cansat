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
    uint8                       CmdCounter;
    uint8                       ErrCounter;
    uint16                      Reserved;
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
    CFS_CORE_APP_HkTlm_t        HkTlm;
    CFS_CORE_APP_SystemHealthTlm_t SystemHealthTlm;
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


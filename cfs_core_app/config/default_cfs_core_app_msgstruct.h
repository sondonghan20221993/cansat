#ifndef DEFAULT_CFS_CORE_APP_MSGSTRUCT_H
#define DEFAULT_CFS_CORE_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "cfs_core_app_msgdefs.h"
#include "cfs_core_app_mission_cfg.h"

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
    uint16                    Reserved;
    uint32                    PublishCount;
    uint32                    LastPublishTimestampMs;
} CFS_CORE_APP_HkTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastValidInputTimestampMs;
    uint8                     HealthState;
    uint8                     FaultCode;
    uint8                     RecoveryRequested;
    uint8                     Reserved;
} CFS_CORE_APP_SystemHealthTlm_t;

#endif


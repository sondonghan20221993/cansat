#ifndef CFS_CORE_APP_UTILS_H
#define CFS_CORE_APP_UTILS_H

#include "cfs_core_app.h"

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} CFS_CORE_APP_GenericStateTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint8                     LinkState;
    uint8                     LastErrorCode;
    uint32                    BytesReceived;
    uint32                    ReconnectAttemptCount;
    uint32                    ParseErrorCount;
    uint32                    LastRxTimestampMs;
} CFS_CORE_APP_BridgeHkMirror_t;

#endif


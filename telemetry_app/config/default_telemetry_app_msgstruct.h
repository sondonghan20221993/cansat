/************************************************************************
 * NASA Docket No. GSC-19,200-1, and identified as "cFS Draco"
 *
 * Copyright (c) 2023 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

#ifndef DEFAULT_TELEMETRY_APP_MSGSTRUCT_H
#define DEFAULT_TELEMETRY_APP_MSGSTRUCT_H

#include "common_types.h"
#include "cfe_msg_hdr.h"
#include "telemetry_app_msgdefs.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} TELEMETRY_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} TELEMETRY_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} TELEMETRY_APP_SendHkCmd_t;

typedef struct
{
    uint8  LinkState;
    uint8  ActiveTransportId;
    uint16 Reserved;
    uint32 LastUpdateAgeMs;
    uint32 DegradedTransitionCount;
    uint32 LostTransitionCount;
    uint32 RecoveryCount;
} TELEMETRY_APP_StatusPayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t     TelemetryHeader;
    TELEMETRY_APP_StatusPayload_t Payload;
} TELEMETRY_APP_StatusTlm_t;

typedef struct
{
    uint8  ActiveTransportId;
    uint8  Valid;
    uint16 Reserved;
    uint32 UpdateAgeMs;
} TELEMETRY_APP_MonitorPayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t      TelemetryHeader;
    TELEMETRY_APP_MonitorPayload_t Payload;
} TELEMETRY_APP_MonitorTlm_t;

typedef struct
{
    uint8  CommandErrorCounter;
    uint8  CommandCounter;
    uint8  LinkState;
    uint8  ActiveTransportId;
    uint32 LastUpdateAgeMs;
    uint32 NominalTimeoutMs;
    uint32 DegradedTimeoutMs;
    uint32 LostTimeoutMs;
    uint32 DegradedTransitionCount;
    uint32 LostTransitionCount;
    uint32 RecoveryCount;
} TELEMETRY_APP_HkPayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    TELEMETRY_APP_HkPayload_t Payload;
} TELEMETRY_APP_HkTlm_t;

#endif

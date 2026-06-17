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

/**
 * @file
 *
 * Main header file for the Telemetry application
 */

#ifndef TELEMETRY_APP_H
#define TELEMETRY_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "telemetry_app_mission_cfg.h"
#include "telemetry_app_platform_cfg.h"

#include "telemetry_app_perfids.h"
#include "telemetry_app_msg.h"
#include "telemetry_app_msgids.h"

typedef enum
{
    TELEMETRY_APP_LINK_ALIVE = 0,
    TELEMETRY_APP_LINK_DEGRADED = 1,
    TELEMETRY_APP_LINK_LOST = 2
} TELEMETRY_APP_LinkState_t;

typedef struct
{
    /*
    ** Command interface counters...
    */
    uint8     CmdCounter;
    uint8     ErrCounter;
    uint16    Reserved;

    /*
    ** Run status used by the main processing loop.
    */
    uint32    RunStatus;
    uint32    LastUpdateAgeMs;
    uint32    NominalTimeoutMs;
    uint32    DegradedTimeoutMs;
    uint32    LostTimeoutMs;
    uint32    DegradedTransitionCount;
    uint32    LostTransitionCount;
    uint32    RecoveryCount;
    uint8     ActiveTransportId;
    uint8     CurrentState;
    uint16    Spare;

    /*
    ** Operational data (not reported in housekeeping)...
    */
    CFE_SB_PipeId_t CommandPipe;

    TELEMETRY_APP_HkTlm_t     HkTlm;
    TELEMETRY_APP_StatusTlm_t StatusTlm;
} TELEMETRY_APP_Data_t;

extern TELEMETRY_APP_Data_t TELEMETRY_APP_Data;

void         TELEMETRY_APP_Main(void);
CFE_Status_t TELEMETRY_APP_Init(void);
void         TELEMETRY_APP_ProcessGroundUpdate(uint32 UpdateAgeMs);
void         TELEMETRY_APP_EvaluateLinkState(uint32 UpdateAgeMs);
void         TELEMETRY_APP_TransitionState(TELEMETRY_APP_LinkState_t NextState);
void         TELEMETRY_APP_ReportHousekeeping(void);
void         TELEMETRY_APP_PublishStatus(void);
bool         TELEMETRY_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);

#endif

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
 * \file
 *   This file contains the source code for the Telemetry App utility functions
 */

#include "telemetry_app.h"
#include "telemetry_app_eventids.h"
#include "telemetry_app_utils.h"

void TELEMETRY_APP_ProcessGroundUpdate(uint32 UpdateAgeMs)
{
    TELEMETRY_APP_Data.LastUpdateAgeMs = UpdateAgeMs;
    TELEMETRY_APP_EvaluateLinkState(UpdateAgeMs);
    TELEMETRY_APP_PublishStatus();
}

void TELEMETRY_APP_EvaluateLinkState(uint32 UpdateAgeMs)
{
    TELEMETRY_APP_LinkState_t NextState = TELEMETRY_APP_LINK_ALIVE;

    if (UpdateAgeMs >= TELEMETRY_APP_Data.LostTimeoutMs)
    {
        NextState = TELEMETRY_APP_LINK_LOST;
    }
    else if (UpdateAgeMs >= TELEMETRY_APP_Data.DegradedTimeoutMs)
    {
        NextState = TELEMETRY_APP_LINK_DEGRADED;
    }

    TELEMETRY_APP_TransitionState(NextState);
}

void TELEMETRY_APP_TransitionState(TELEMETRY_APP_LinkState_t NextState)
{
    TELEMETRY_APP_LinkState_t PreviousState = (TELEMETRY_APP_LinkState_t)TELEMETRY_APP_Data.CurrentState;

    if (PreviousState == NextState)
    {
        return;
    }

    TELEMETRY_APP_Data.CurrentState = (uint8)NextState;

    if (NextState == TELEMETRY_APP_LINK_DEGRADED)
    {
        ++TELEMETRY_APP_Data.DegradedTransitionCount;
        CFE_EVS_SendEvent(TELEMETRY_APP_DEGRADED_EID, CFE_EVS_EventType_INFORMATION,
                          "Telemetry App: link degraded, update_age_ms=%lu",
                          (unsigned long)TELEMETRY_APP_Data.LastUpdateAgeMs);
    }
    else if (NextState == TELEMETRY_APP_LINK_LOST)
    {
        ++TELEMETRY_APP_Data.LostTransitionCount;
        CFE_EVS_SendEvent(TELEMETRY_APP_LOST_EID, CFE_EVS_EventType_ERROR,
                          "Telemetry App: link lost, update_age_ms=%lu",
                          (unsigned long)TELEMETRY_APP_Data.LastUpdateAgeMs);
    }
    else
    {
        ++TELEMETRY_APP_Data.RecoveryCount;
        CFE_EVS_SendEvent(TELEMETRY_APP_RECOVERY_EID, CFE_EVS_EventType_INFORMATION,
                          "Telemetry App: link recovered, update_age_ms=%lu",
                          (unsigned long)TELEMETRY_APP_Data.LastUpdateAgeMs);
    }
}

void TELEMETRY_APP_ReportHousekeeping(void)
{
    TELEMETRY_APP_Data.HkTlm.Payload.CommandErrorCounter     = TELEMETRY_APP_Data.ErrCounter;
    TELEMETRY_APP_Data.HkTlm.Payload.CommandCounter          = TELEMETRY_APP_Data.CmdCounter;
    TELEMETRY_APP_Data.HkTlm.Payload.LinkState               = TELEMETRY_APP_Data.CurrentState;
    TELEMETRY_APP_Data.HkTlm.Payload.ActiveTransportId       = TELEMETRY_APP_Data.ActiveTransportId;
    TELEMETRY_APP_Data.HkTlm.Payload.LastUpdateAgeMs         = TELEMETRY_APP_Data.LastUpdateAgeMs;
    TELEMETRY_APP_Data.HkTlm.Payload.NominalTimeoutMs        = TELEMETRY_APP_Data.NominalTimeoutMs;
    TELEMETRY_APP_Data.HkTlm.Payload.DegradedTimeoutMs       = TELEMETRY_APP_Data.DegradedTimeoutMs;
    TELEMETRY_APP_Data.HkTlm.Payload.LostTimeoutMs           = TELEMETRY_APP_Data.LostTimeoutMs;
    TELEMETRY_APP_Data.HkTlm.Payload.DegradedTransitionCount = TELEMETRY_APP_Data.DegradedTransitionCount;
    TELEMETRY_APP_Data.HkTlm.Payload.LostTransitionCount     = TELEMETRY_APP_Data.LostTransitionCount;
    TELEMETRY_APP_Data.HkTlm.Payload.RecoveryCount           = TELEMETRY_APP_Data.RecoveryCount;

    CFE_ES_WriteToSysLog(
        "Telemetry App HK: cmd=%u err=%u state=%u active_transport_id=%u age_ms=%lu nom=%lu deg_to=%lu "
        "lost_to=%lu deg=%lu lost=%lu rec=%lu\n",
        (unsigned int)TELEMETRY_APP_Data.HkTlm.Payload.CommandCounter,
        (unsigned int)TELEMETRY_APP_Data.HkTlm.Payload.CommandErrorCounter,
        (unsigned int)TELEMETRY_APP_Data.HkTlm.Payload.LinkState,
        (unsigned int)TELEMETRY_APP_Data.HkTlm.Payload.ActiveTransportId,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.LastUpdateAgeMs,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.NominalTimeoutMs,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.DegradedTimeoutMs,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.LostTimeoutMs,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.DegradedTransitionCount,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.LostTransitionCount,
        (unsigned long)TELEMETRY_APP_Data.HkTlm.Payload.RecoveryCount);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(TELEMETRY_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(TELEMETRY_APP_Data.HkTlm.TelemetryHeader), true);
}

void TELEMETRY_APP_PublishStatus(void)
{
    TELEMETRY_APP_Data.StatusTlm.Payload.LinkState               = TELEMETRY_APP_Data.CurrentState;
    TELEMETRY_APP_Data.StatusTlm.Payload.ActiveTransportId       = TELEMETRY_APP_Data.ActiveTransportId;
    TELEMETRY_APP_Data.StatusTlm.Payload.LastUpdateAgeMs         = TELEMETRY_APP_Data.LastUpdateAgeMs;
    TELEMETRY_APP_Data.StatusTlm.Payload.DegradedTransitionCount = TELEMETRY_APP_Data.DegradedTransitionCount;
    TELEMETRY_APP_Data.StatusTlm.Payload.LostTransitionCount     = TELEMETRY_APP_Data.LostTransitionCount;
    TELEMETRY_APP_Data.StatusTlm.Payload.RecoveryCount           = TELEMETRY_APP_Data.RecoveryCount;

    CFE_ES_WriteToSysLog(
        "Telemetry App STATUS: state=%u active_transport_id=%u update_age_ms=%lu degraded=%lu lost=%lu recovery=%lu\n",
        (unsigned int)TELEMETRY_APP_Data.StatusTlm.Payload.LinkState,
        (unsigned int)TELEMETRY_APP_Data.StatusTlm.Payload.ActiveTransportId,
        (unsigned long)TELEMETRY_APP_Data.StatusTlm.Payload.LastUpdateAgeMs,
        (unsigned long)TELEMETRY_APP_Data.StatusTlm.Payload.DegradedTransitionCount,
        (unsigned long)TELEMETRY_APP_Data.StatusTlm.Payload.LostTransitionCount,
        (unsigned long)TELEMETRY_APP_Data.StatusTlm.Payload.RecoveryCount);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(TELEMETRY_APP_Data.StatusTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(TELEMETRY_APP_Data.StatusTlm.TelemetryHeader), true);
}

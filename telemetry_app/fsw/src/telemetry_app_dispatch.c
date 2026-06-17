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
 *   This file contains the source code for the Telemetry App.
 */

/*
** Include Files:
*/
#include "telemetry_app.h"
#include "telemetry_app_dispatch.h"
#include "telemetry_app_cmds.h"
#include "telemetry_app_eventids.h"
#include "telemetry_app_msgids.h"
#include "telemetry_app_msg.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Verify command packet length                                               */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
bool TELEMETRY_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    bool              result       = true;
    size_t            ActualLength = 0;
    CFE_SB_MsgId_t    MsgId        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t FcnCode      = 0;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_MSG_GetMsgId(MsgPtr, &MsgId);
        CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);

        CFE_EVS_SendEvent(TELEMETRY_APP_CMD_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %u, Len = %u, Expected = %u",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId), (unsigned int)FcnCode, (unsigned int)ActualLength,
                          (unsigned int)ExpectedLength);

        result = false;

        TELEMETRY_APP_Data.ErrCounter++;
    }

    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* TELEMETRY_APP ground commands                                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
void TELEMETRY_APP_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /*
    ** Process TELEMETRY_APP ground commands
    */
    switch (CommandCode)
    {
        case TELEMETRY_APP_NOOP_CC:
            if (TELEMETRY_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(TELEMETRY_APP_NoopCmd_t)))
            {
                TELEMETRY_APP_NoopCmd((const TELEMETRY_APP_NoopCmd_t *)SBBufPtr);
            }
            break;

        case TELEMETRY_APP_RESET_COUNTERS_CC:
            if (TELEMETRY_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(TELEMETRY_APP_ResetCountersCmd_t)))
            {
                TELEMETRY_APP_ResetCountersCmd((const TELEMETRY_APP_ResetCountersCmd_t *)SBBufPtr);
            }
            break;

        /* default case already found during FC vs length test */
        default:
            CFE_EVS_SendEvent(TELEMETRY_APP_CC_ERR_EID, CFE_EVS_EventType_ERROR, "Invalid ground command code: CC = %d",
                              CommandCode);
            break;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the Telemetry */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
void TELEMETRY_APP_TaskPipe(const CFE_SB_Buffer_t *SBBufPtr)
{
    static CFE_SB_MsgId_t CMD_MID     = CFE_SB_MSGID_RESERVED;
    static CFE_SB_MsgId_t SEND_HK_MID = CFE_SB_MSGID_RESERVED;
    static CFE_SB_MsgId_t MONITOR_MID = CFE_SB_MSGID_RESERVED;

    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    /* cache the local MID Values here, this avoids repeat lookups */
    if (!CFE_SB_IsValidMsgId(CMD_MID))
    {
        CMD_MID     = CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID);
        SEND_HK_MID = CFE_SB_ValueToMsgId(TELEMETRY_APP_SEND_HK_MID);
        MONITOR_MID = CFE_SB_ValueToMsgId(TELEMETRY_APP_MONITOR_MID);
    }

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    /* Process all SB messages */
    if (CFE_SB_MsgId_Equal(MsgId, SEND_HK_MID))
    {
        /* Housekeeping request */
        TELEMETRY_APP_SendHkCmd((const TELEMETRY_APP_SendHkCmd_t *)SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CMD_MID))
    {
        /* Ground command */
        TELEMETRY_APP_ProcessGroundCommand(SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, MONITOR_MID))
    {
        const TELEMETRY_APP_MonitorTlm_t *MonitorMsg = (const TELEMETRY_APP_MonitorTlm_t *)SBBufPtr;

        if (MonitorMsg->Payload.Valid == 0)
        {
            TELEMETRY_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(TELEMETRY_APP_MONITOR_INVALID_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: invalid monitor update");
        }
        else
        {
            TELEMETRY_APP_Data.ActiveTransportId = MonitorMsg->Payload.ActiveTransportId;
            TELEMETRY_APP_ProcessGroundUpdate(MonitorMsg->Payload.UpdateAgeMs);
        }
    }
    else
    {
        /* Unknown command */
        CFE_EVS_SendEvent(TELEMETRY_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Telemetry App: invalid command packet, MID = 0x%x",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId));
    }
}

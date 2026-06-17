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
#include "telemetry_app_cmds.h"
#include "telemetry_app_utils.h"
#include "telemetry_app_eventids.h"
#include "telemetry_app_dispatch.h"
#include "telemetry_app_version.h"

/*
** global data
*/
TELEMETRY_APP_Data_t TELEMETRY_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void TELEMETRY_APP_Main(void)
{
    CFE_Status_t     status;
    CFE_SB_Buffer_t *SBBufPtr;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(TELEMETRY_APP_PERF_ID);

    /*
    ** Perform application-specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = TELEMETRY_APP_Init();
    if (status != CFE_SUCCESS)
    {
        TELEMETRY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Telemetry App Runloop
    */
    while (CFE_ES_RunLoop(&TELEMETRY_APP_Data.RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(TELEMETRY_APP_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, TELEMETRY_APP_Data.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(TELEMETRY_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            TELEMETRY_APP_TaskPipe(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(TELEMETRY_APP_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: SB Pipe Read Error, App Will Exit");

            TELEMETRY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(TELEMETRY_APP_PERF_ID);

    CFE_ES_ExitApp(TELEMETRY_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t TELEMETRY_APP_Init(void)
{
    CFE_Status_t status;
    char         VersionString[TELEMETRY_APP_CFG_MAX_VERSION_STR_LEN];

    /* Zero out the global data structure */
    memset(&TELEMETRY_APP_Data, 0, sizeof(TELEMETRY_APP_Data));

    TELEMETRY_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    TELEMETRY_APP_Data.CurrentState = TELEMETRY_APP_LINK_ALIVE;
    TELEMETRY_APP_Data.ActiveTransportId = TELEMETRY_APP_ACTIVE_TRANSPORT_ID;
    TELEMETRY_APP_Data.NominalTimeoutMs  = TELEMETRY_APP_DEFAULT_NOMINAL_TIMEOUT_MS;
    TELEMETRY_APP_Data.DegradedTimeoutMs = TELEMETRY_APP_DEFAULT_DEGRADED_TIMEOUT_MS;
    TELEMETRY_APP_Data.LostTimeoutMs     = TELEMETRY_APP_DEFAULT_LOST_TIMEOUT_MS;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Telemetry App: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
    }
    else
    {
        /*
         ** Initialize housekeeping packet (clear user data area).
         */
        CFE_MSG_Init(CFE_MSG_PTR(TELEMETRY_APP_Data.HkTlm.TelemetryHeader), CFE_SB_ValueToMsgId(TELEMETRY_APP_HK_TLM_MID),
                     sizeof(TELEMETRY_APP_Data.HkTlm));
        CFE_MSG_Init(CFE_MSG_PTR(TELEMETRY_APP_Data.StatusTlm.TelemetryHeader), CFE_SB_ValueToMsgId(TELEMETRY_STATUS_MID),
                     sizeof(TELEMETRY_APP_Data.StatusTlm));

        /*
         ** Create Software Bus message pipe.
         */
        status = CFE_SB_CreatePipe(&TELEMETRY_APP_Data.CommandPipe, TELEMETRY_APP_PLATFORM_PIPE_DEPTH,
                                   TELEMETRY_APP_PLATFORM_PIPE_NAME);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TELEMETRY_APP_CR_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: Error creating SB Command Pipe, RC = 0x%08lX", (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Subscribe to Housekeeping request commands
        */
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TELEMETRY_APP_SEND_HK_MID), TELEMETRY_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TELEMETRY_APP_SUB_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: Error Subscribing to HK request, RC = 0x%08lX", (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Subscribe to ground command packets
        */
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID), TELEMETRY_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TELEMETRY_APP_SUB_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: Error Subscribing to Commands, RC = 0x%08lX", (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TELEMETRY_APP_MONITOR_MID), TELEMETRY_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TELEMETRY_APP_SUB_MONITOR_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Telemetry App: Error Subscribing to Monitor, RC = 0x%08lX", (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        CFE_Config_GetVersionString(VersionString, TELEMETRY_APP_CFG_MAX_VERSION_STR_LEN, "Telemetry App", TELEMETRY_APP_VERSION,
                                    TELEMETRY_APP_BUILD_CODENAME, TELEMETRY_APP_LAST_OFFICIAL);

        CFE_EVS_SendEvent(TELEMETRY_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION, "Telemetry App Initialized.%s",
                          VersionString);
    }

    return status;
}

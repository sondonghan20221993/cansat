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
 *   This file contains the source code for the Telemetry App Ground Command-handling functions
 */

/*
** Include Files:
*/
#include "telemetry_app.h"
#include "telemetry_app_cmds.h"
#include "telemetry_app_msgids.h"
#include "telemetry_app_eventids.h"
#include "telemetry_app_version.h"
#include "telemetry_app_utils.h"
#include "telemetry_app_msg.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t TELEMETRY_APP_SendHkCmd(const TELEMETRY_APP_SendHkCmd_t *Msg)
{
    TELEMETRY_APP_ReportHousekeeping();
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* TELEMETRY_APP NOOP commands                                                */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t TELEMETRY_APP_NoopCmd(const TELEMETRY_APP_NoopCmd_t *Msg)
{
    TELEMETRY_APP_Data.CmdCounter++;

    CFE_EVS_SendEvent(TELEMETRY_APP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION, "Telemetry App: NOOP command %s",
                      TELEMETRY_APP_VERSION);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t TELEMETRY_APP_ResetCountersCmd(const TELEMETRY_APP_ResetCountersCmd_t *Msg)
{
    TELEMETRY_APP_Data.CmdCounter = 0;
    TELEMETRY_APP_Data.ErrCounter = 0;
    TELEMETRY_APP_Data.DegradedTransitionCount = 0;
    TELEMETRY_APP_Data.LostTransitionCount     = 0;
    TELEMETRY_APP_Data.RecoveryCount           = 0;

    CFE_EVS_SendEvent(TELEMETRY_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "Telemetry App: RESET command");

    return CFE_SUCCESS;
}

#include "uplink_app.h"
#include "uplink_app_cmds.h"
#include "uplink_app_utils.h"
#include "uplink_app_eventids.h"
#include "uplink_app_dispatch.h"
#include "uplink_app_version.h"

#include <string.h>

UPLINK_APP_Data_t UPLINK_APP_Data;

void UPLINK_APP_Main(void)
{
    CFE_Status_t     Status;
    CFE_SB_Buffer_t *SBBufPtr;

    CFE_ES_PerfLogEntry(UPLINK_APP_PERF_ID);

    Status = UPLINK_APP_Init();
    if (Status != CFE_SUCCESS)
    {
        UPLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&UPLINK_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(UPLINK_APP_PERF_ID);
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, UPLINK_APP_Data.CommandPipe, UPLINK_APP_SB_POLL_TIMEOUT_MS);
        CFE_ES_PerfLogEntry(UPLINK_APP_PERF_ID);

        if (Status == CFE_SUCCESS)
        {
            UPLINK_APP_TaskPipe(SBBufPtr);
        }
        else if (Status == CFE_SB_TIME_OUT)
        {
            UPLINK_APP_ServicePrototype();
        }
        else
        {
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: SB Pipe Read Error, App Will Exit");
            UPLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    CFE_ES_PerfLogExit(UPLINK_APP_PERF_ID);
    CFE_ES_ExitApp(UPLINK_APP_Data.RunStatus);
}

CFE_Status_t UPLINK_APP_Init(void)
{
    CFE_Status_t Status;

    memset(&UPLINK_APP_Data, 0, sizeof(UPLINK_APP_Data));
    UPLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    Status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(UPLINK_APP_Data.HkTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(UPLINK_APP_HK_TLM_MID),
                          sizeof(UPLINK_APP_Data.HkTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(UPLINK_APP_Data.StatusTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(UPLINK_STATUS_MID),
                          sizeof(UPLINK_APP_Data.StatusTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_CreatePipe(&UPLINK_APP_Data.CommandPipe, UPLINK_APP_PLATFORM_PIPE_DEPTH, UPLINK_APP_PLATFORM_PIPE_NAME);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE), UPLINK_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(UPLINK_APP_SEND_HK_MID_VALUE), UPLINK_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SYSTEM_HEALTH_MID_VALUE), UPLINK_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(BRIDGE_HK_MID_VALUE), UPLINK_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    UPLINK_APP_Data.Valid = 1;
    UPLINK_APP_LoadState();
    UPLINK_APP_IncrementBootCount(); /* BL-12(2026-07-21): 복원값(또는 0)에서 +1, 즉시 영속화 */

    CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP Initialized");

    return CFE_SUCCESS;
}


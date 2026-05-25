#include "cfs_core_app.h"
#include "cfs_core_app_cmds.h"
#include "cfs_core_app_utils.h"
#include "cfs_core_app_eventids.h"
#include "cfs_core_app_dispatch.h"
#include "cfs_core_app_version.h"
#include "cfs_core_app_topicids.h"

#include <string.h>

CFS_CORE_APP_Data_t CFS_CORE_APP_Data;

void CFS_CORE_APP_Main(void)
{
    CFE_Status_t     Status;
    CFE_SB_Buffer_t *SBBufPtr;

    CFE_ES_PerfLogEntry(CFS_CORE_APP_PERF_ID);

    Status = CFS_CORE_APP_Init();
    if (Status != CFE_SUCCESS)
    {
        CFS_CORE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&CFS_CORE_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(CFS_CORE_APP_PERF_ID);
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, CFS_CORE_APP_Data.CommandPipe, CFS_CORE_APP_SB_POLL_TIMEOUT_MS);
        CFE_ES_PerfLogEntry(CFS_CORE_APP_PERF_ID);

        if (Status == CFE_SUCCESS)
        {
            CFS_CORE_APP_TaskPipe(SBBufPtr);
        }
        else if (Status == CFE_SB_TIME_OUT)
        {
            CFS_CORE_APP_ServicePrototype();
        }
        else
        {
            CFE_EVS_SendEvent(CFS_CORE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CFS_CORE_APP: SB Pipe Read Error, App Will Exit");
            CFS_CORE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    CFE_ES_PerfLogExit(CFS_CORE_APP_PERF_ID);
    CFE_ES_ExitApp(CFS_CORE_APP_Data.RunStatus);
}

CFE_Status_t CFS_CORE_APP_Init(void)
{
    CFE_Status_t Status;

    memset(&CFS_CORE_APP_Data, 0, sizeof(CFS_CORE_APP_Data));
    CFS_CORE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    Status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(CFS_CORE_APP_Data.HkTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(CFS_CORE_APP_HK_TLM_MID),
                          sizeof(CFS_CORE_APP_Data.HkTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(CFS_CORE_APP_Data.SystemHealthTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(SYSTEM_HEALTH_MID),
                          sizeof(CFS_CORE_APP_Data.SystemHealthTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_CreatePipe(&CFS_CORE_APP_Data.CommandPipe, CFS_CORE_APP_PLATFORM_PIPE_DEPTH, CFS_CORE_APP_PLATFORM_PIPE_NAME);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_SEND_HK_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_BRIDGE_HK_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE), CFS_CORE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    CFE_EVS_SendEvent(CFS_CORE_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP Initialized");

    return CFE_SUCCESS;
}


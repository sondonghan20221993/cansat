#include "img_app.h"
#include "img_app_cmds.h"
#include "img_app_utils.h"
#include "img_app_eventids.h"
#include "img_app_dispatch.h"
#include "img_app_version.h"

#include <string.h>

IMG_APP_Data_t IMG_APP_Data;

void IMG_APP_Main(void)
{
    CFE_Status_t     Status;
    CFE_SB_Buffer_t *SBBufPtr;

    CFE_ES_PerfLogEntry(IMG_APP_PERF_ID);

    Status = IMG_APP_Init();
    if (Status != CFE_SUCCESS)
    {
        IMG_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&IMG_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(IMG_APP_PERF_ID);
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, IMG_APP_Data.CommandPipe, IMG_APP_SB_POLL_TIMEOUT_MS);
        CFE_ES_PerfLogEntry(IMG_APP_PERF_ID);

        if (Status == CFE_SUCCESS)
        {
            IMG_APP_TaskPipe(SBBufPtr);
        }
        else if (Status == CFE_SB_TIME_OUT)
        {
            IMG_APP_ServicePrototype();
        }
        else
        {
            CFE_EVS_SendEvent(IMG_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "IMG_APP: SB Pipe Read Error, App Will Exit");
            IMG_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    CFE_ES_PerfLogExit(IMG_APP_PERF_ID);
    CFE_ES_ExitApp(IMG_APP_Data.RunStatus);
}

CFE_Status_t IMG_APP_Init(void)
{
    CFE_Status_t Status;

    memset(&IMG_APP_Data, 0, sizeof(IMG_APP_Data));
    IMG_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    Status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(IMG_APP_Data.HkTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(IMG_APP_HK_TLM_MID),
                          sizeof(IMG_APP_Data.HkTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(IMG_APP_Data.ImageMetaTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(IMAGE_META_MID_VALUE),
                          sizeof(IMG_APP_Data.ImageMetaTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_CreatePipe(&IMG_APP_Data.CommandPipe, IMG_APP_PLATFORM_PIPE_DEPTH, IMG_APP_PLATFORM_PIPE_NAME);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(IMG_APP_CMD_MID_VALUE), IMG_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(IMG_APP_SEND_HK_MID_VALUE), IMG_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    CFE_EVS_SendEvent(IMG_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "IMG_APP Initialized");

    return CFE_SUCCESS;
}

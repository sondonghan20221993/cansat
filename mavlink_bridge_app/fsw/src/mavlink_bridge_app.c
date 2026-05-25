#include "mavlink_bridge_app.h"
#include "mavlink_bridge_app_cmds.h"
#include "mavlink_bridge_app_utils.h"
#include "mavlink_bridge_app_eventids.h"
#include "mavlink_bridge_app_dispatch.h"
#include "mavlink_bridge_app_version.h"
#include <string.h>

MAVLINK_BRIDGE_APP_Data_t MAVLINK_BRIDGE_APP_Data;

void MAV_BRIDGE_APP_Main(void)
{
    CFE_Status_t     Status;
    CFE_SB_Buffer_t *SBBufPtr;

    CFE_ES_PerfLogEntry(MAVLINK_BRIDGE_APP_PERF_ID);

    Status = MAVLINK_BRIDGE_APP_Init();
    if (Status != CFE_SUCCESS)
    {
        MAVLINK_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&MAVLINK_BRIDGE_APP_Data.RunStatus) == true)
    {
        CFE_ES_PerfLogExit(MAVLINK_BRIDGE_APP_PERF_ID);
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, MAVLINK_BRIDGE_APP_Data.CommandPipe, MAVLINK_BRIDGE_APP_SB_POLL_TIMEOUT_MS);
        CFE_ES_PerfLogEntry(MAVLINK_BRIDGE_APP_PERF_ID);

        if (Status == CFE_SUCCESS)
        {
            MAVLINK_BRIDGE_APP_TaskPipe(SBBufPtr);
        }
        else if (Status == CFE_SB_TIME_OUT)
        {
            MAVLINK_BRIDGE_APP_ServiceSerial();
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "MAVLINK_BRIDGE_APP: SB Pipe Read Error, App Will Exit");
            MAVLINK_BRIDGE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    CFE_ES_PerfLogExit(MAVLINK_BRIDGE_APP_PERF_ID);
    CFE_ES_ExitApp(MAVLINK_BRIDGE_APP_Data.RunStatus);
}

CFE_Status_t MAVLINK_BRIDGE_APP_Init(void)
{
    CFE_Status_t Status;

    memset(&MAVLINK_BRIDGE_APP_Data, 0, sizeof(MAVLINK_BRIDGE_APP_Data));

    MAVLINK_BRIDGE_APP_Data.RunStatus           = CFE_ES_RunStatus_APP_RUN;
    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.ReconnectIntervalMs = MAVLINK_BRIDGE_APP_RECONNECT_INTERVAL_MS;
    MAVLINK_BRIDGE_APP_Data.SerialFd            = -1;
    MAVLINK_BRIDGE_APP_Data.LoRaFd              = -1;

    Status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.HkTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_HK_TLM_MID),
                          sizeof(MAVLINK_BRIDGE_APP_Data.HkTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(FC_EKF_LOCAL_STATE_MID_VALUE),
                          sizeof(MAVLINK_BRIDGE_APP_Data.EkfLocalTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.AttitudeTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(FC_ATTITUDE_STATE_MID_VALUE),
                          sizeof(MAVLINK_BRIDGE_APP_Data.AttitudeTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.GpsRawTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(FC_GPS_RAW_STATE_MID_VALUE),
                          sizeof(MAVLINK_BRIDGE_APP_Data.GpsRawTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_MSG_Init(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.EkfStatusTlm.TelemetryHeader),
                          CFE_SB_ValueToMsgId(FC_EKF_STATUS_MID_VALUE),
                          sizeof(MAVLINK_BRIDGE_APP_Data.EkfStatusTlm));
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_CreatePipe(&MAVLINK_BRIDGE_APP_Data.CommandPipe, MAVLINK_BRIDGE_APP_PLATFORM_PIPE_DEPTH,
                               MAVLINK_BRIDGE_APP_PLATFORM_PIPE_NAME);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE), MAVLINK_BRIDGE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE), MAVLINK_BRIDGE_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP Initialized");

    return CFE_SUCCESS;
}

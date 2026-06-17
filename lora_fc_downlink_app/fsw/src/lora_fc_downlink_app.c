/************************************************************************
 * LoRa FC Downlink App
 ************************************************************************/

#include "lora_fc_downlink_app.h"
#include "lora_fc_downlink_app_cmds.h"
#include "lora_fc_downlink_app_dispatch.h"
#include "lora_fc_downlink_app_eventids.h"
#include "lora_fc_downlink_app_utils.h"
#include "lora_fc_downlink_app_version.h"

LORA_FC_DOWNLINK_APP_Data_t LORA_FC_DOWNLINK_APP_Data;

void LORA_FC_DOWNLINK_APP_Main(void)
{
    CFE_Status_t     status;
    CFE_SB_Buffer_t *sb_buf_ptr;

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: main entry\n");

    status = LORA_FC_DOWNLINK_APP_Init();
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: init failed rc=0x%08lX\n", (unsigned long)status);
        LORA_FC_DOWNLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&LORA_FC_DOWNLINK_APP_Data.RunStatus) == true)
    {
        status = CFE_SB_ReceiveBuffer(&sb_buf_ptr, LORA_FC_DOWNLINK_APP_Data.CommandPipe, CFE_SB_PEND_FOREVER);

        if (status == CFE_SUCCESS)
        {
            LORA_FC_DOWNLINK_APP_TaskPipe(sb_buf_ptr);
        }
        else
        {
            CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LoRa FC Downlink App: SB Pipe Read Error, App Will Exit");
            LORA_FC_DOWNLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    CFE_ES_ExitApp(LORA_FC_DOWNLINK_APP_Data.RunStatus);
}

CFE_Status_t LORA_FC_DOWNLINK_APP_Init(void)
{
    CFE_Status_t status;
    char         version_string[LORA_FC_DOWNLINK_APP_CFG_MAX_VERSION_STR_LEN];

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: init begin\n");

    memset(&LORA_FC_DOWNLINK_APP_Data, 0, sizeof(LORA_FC_DOWNLINK_APP_Data));

    LORA_FC_DOWNLINK_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    LORA_FC_DOWNLINK_APP_Data.PacketType = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    LORA_FC_DOWNLINK_APP_Data.LoRaFd = -1;

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: before EVS register\n");
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("LoRa FC Downlink App: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
        return status;
    }

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: before HK init\n");
    CFE_MSG_Init(CFE_MSG_PTR(LORA_FC_DOWNLINK_APP_Data.HkTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_HK_TLM_MID), sizeof(LORA_FC_DOWNLINK_APP_Data.HkTlm));

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: before pipe create\n");
    status = CFE_SB_CreatePipe(&LORA_FC_DOWNLINK_APP_Data.CommandPipe, LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_DEPTH,
                               LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_NAME);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_CR_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LoRa FC Downlink App: Error creating SB Command Pipe, RC = 0x%08lX", (unsigned long)status);
        return status;
    }

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: subscribe send_hk\n");
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SEND_HK_MID), LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status == CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: subscribe cmd\n");
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID), LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    }

    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_SUB_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LoRa FC Downlink App: Error subscribing to required MID, RC = 0x%08lX", (unsigned long)status);
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE),
                              LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE),
                              LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE),
                              LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE),
                              LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_MID_VALUE),
                              LORA_FC_DOWNLINK_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: before version string\n");
    CFE_Config_GetVersionString(version_string, LORA_FC_DOWNLINK_APP_CFG_MAX_VERSION_STR_LEN, "LoRa FC Downlink App",
                                LORA_FC_DOWNLINK_APP_VERSION, LORA_FC_DOWNLINK_APP_BUILD_CODENAME,
                                LORA_FC_DOWNLINK_APP_LAST_OFFICIAL);

    CFE_ES_WriteToSysLog("LORA_FC_DOWNLINK_APP: init success\n");
    CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LoRa FC Downlink App Initialized.%s", version_string);

    return CFE_SUCCESS;
}

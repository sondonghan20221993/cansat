#include "uplink_app_dispatch.h"
#include "uplink_app_cmds.h"
#include "uplink_app_eventids.h"
#include "uplink_app_fcncodes.h"

void UPLINK_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t       MsgId;
    CFE_MSG_FcnCode_t    FcnCode;
    CFE_Status_t         Status;
    CFE_MSG_Message_t   *MsgPtr;

    MsgPtr  = &SBBufPtr->Msg;
    Status  = CFE_MSG_GetMsgId(MsgPtr, &MsgId);
    if (Status != CFE_SUCCESS)
    {
        UPLINK_APP_Data.ErrCounter++;
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == UPLINK_APP_SEND_HK_MID_VALUE)
    {
        UPLINK_APP_ReportHousekeeping();
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == SYSTEM_HEALTH_MID_VALUE)
    {
        const UPLINK_APP_SysHealthMirror_t *HealthMsg =
            (const UPLINK_APP_SysHealthMirror_t *)MsgPtr;
        UPLINK_APP_Data.CfsHealthState    = HealthMsg->HealthState;
        UPLINK_APP_Data.CfsHealthReceived = 1U;
        return;
    }

    /* raw ground uplink frame forwarded by lora_tdm_app (CP2102 owner):
     * parse the "UP,..." text here (app owns uplink semantics), then process. */
    if (CFE_SB_MsgIdToValue(MsgId) == UPLINK_APP_LORA_RAW_MID_VALUE)
    {
        const UPLINK_APP_LoRaRawMsg_t *Raw = (const UPLINK_APP_LoRaRawMsg_t *)MsgPtr;
        UPLINK_APP_ProcessUplinkCmd_t  Cmd;

        if (UPLINK_APP_ParseLoRaFrame(Raw->Frame, &Cmd))
        {
            UPLINK_APP_ProcessUplink(&Cmd);
        }
        else
        {
            UPLINK_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: LoRa frame parse failed: %.40s", Raw->Frame);
        }
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) != UPLINK_APP_CMD_MID_VALUE)
    {
        UPLINK_APP_Data.ErrCounter++;
        return;
    }

    Status = CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);
    if (Status != CFE_SUCCESS)
    {
        UPLINK_APP_Data.ErrCounter++;
        return;
    }

    switch (FcnCode)
    {
        case UPLINK_APP_NOOP_CC:
            if (UPLINK_APP_VerifyCmdLength(MsgPtr, sizeof(UPLINK_APP_NoopCmd_t)))
            {
                UPLINK_APP_Noop((const UPLINK_APP_NoopCmd_t *)MsgPtr);
            }
            break;

        case UPLINK_APP_RESET_COUNTERS_CC:
            if (UPLINK_APP_VerifyCmdLength(MsgPtr, sizeof(UPLINK_APP_ResetCountersCmd_t)))
            {
                UPLINK_APP_ResetCounters((const UPLINK_APP_ResetCountersCmd_t *)MsgPtr);
            }
            break;

        case UPLINK_APP_PROCESS_UPLINK_CC:
            if (UPLINK_APP_VerifyCmdLength(MsgPtr, sizeof(UPLINK_APP_ProcessUplinkCmd_t)))
            {
                UPLINK_APP_ProcessUplink((const UPLINK_APP_ProcessUplinkCmd_t *)MsgPtr);
            }
            break;

        default:
            UPLINK_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: invalid command code %u", (unsigned int)FcnCode);
            break;
    }
}


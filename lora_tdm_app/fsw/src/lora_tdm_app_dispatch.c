#include "lora_tdm_app_dispatch.h"
#include "lora_tdm_app.h"
#include "lora_tdm_app_cmds.h"
#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app_fcncodes.h"

static bool LORA_TDM_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t            ActualLength;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_MSG_GetMsgId(MsgPtr, &MsgId);
        CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);
        CFE_EVS_SendEvent(LORA_TDM_APP_CMD_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: Invalid cmd length MID=0x%04X CC=%u expected=%lu actual=%lu",
                          (unsigned)CFE_SB_MsgIdToValue(MsgId), (unsigned)FcnCode,
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }
    return true;
}

void LORA_TDM_APP_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t FcnCode = 0;

    CFE_MSG_GetFcnCode(CFE_MSG_PTR(SBBufPtr->Msg), &FcnCode);

    switch (FcnCode)
    {
        case LORA_TDM_APP_NOOP_CC:
            if (LORA_TDM_APP_VerifyCmdLength(CFE_MSG_PTR(SBBufPtr->Msg),
                                              sizeof(LORA_TDM_APP_NoopCmd_t)))
            {
                LORA_TDM_APP_Noop((const LORA_TDM_APP_NoopCmd_t *)SBBufPtr);
            }
            break;

        case LORA_TDM_APP_RESET_COUNTERS_CC:
            if (LORA_TDM_APP_VerifyCmdLength(CFE_MSG_PTR(SBBufPtr->Msg),
                                              sizeof(LORA_TDM_APP_ResetCountersCmd_t)))
            {
                LORA_TDM_APP_ResetCounters((const LORA_TDM_APP_ResetCountersCmd_t *)SBBufPtr);
            }
            break;

        default:
            LORA_TDM_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(LORA_TDM_APP_CC_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: Invalid command code %u", (unsigned)FcnCode);
            break;
    }
}

void LORA_TDM_APP_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(CFE_MSG_PTR(SBBufPtr->Msg), &MsgId);

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessGroundCommand(SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE)))
    {
        LORA_TDM_APP_ReportHousekeeping();
    }
    else
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: Unknown MID 0x%04X",
                          (unsigned)CFE_SB_MsgIdToValue(MsgId));
    }
}

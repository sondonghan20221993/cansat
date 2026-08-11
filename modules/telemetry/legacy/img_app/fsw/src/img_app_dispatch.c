#include "img_app_dispatch.h"
#include "img_app_cmds.h"
#include "img_app_eventids.h"
#include "img_app_fcncodes.h"

void IMG_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t       MsgId;
    CFE_MSG_FcnCode_t    FcnCode;
    CFE_Status_t         Status;
    CFE_MSG_Message_t   *MsgPtr;

    MsgPtr  = &SBBufPtr->Msg;
    Status  = CFE_MSG_GetMsgId(MsgPtr, &MsgId);
    if (Status != CFE_SUCCESS)
    {
        IMG_APP_Data.ErrCounter++;
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == IMG_APP_SEND_HK_MID_VALUE)
    {
        IMG_APP_ReportHousekeeping();
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) != IMG_APP_CMD_MID_VALUE)
    {
        IMG_APP_Data.ErrCounter++;
        return;
    }

    Status = CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);
    if (Status != CFE_SUCCESS)
    {
        IMG_APP_Data.ErrCounter++;
        return;
    }

    switch (FcnCode)
    {
        case IMG_APP_NOOP_CC:
            if (IMG_APP_VerifyCmdLength(MsgPtr, sizeof(IMG_APP_NoopCmd_t)))
            {
                IMG_APP_Noop((const IMG_APP_NoopCmd_t *)MsgPtr);
            }
            break;

        case IMG_APP_RESET_COUNTERS_CC:
            if (IMG_APP_VerifyCmdLength(MsgPtr, sizeof(IMG_APP_ResetCountersCmd_t)))
            {
                IMG_APP_ResetCounters((const IMG_APP_ResetCountersCmd_t *)MsgPtr);
            }
            break;

        default:
            IMG_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(IMG_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "IMG_APP: invalid command code %u", (unsigned int)FcnCode);
            break;
    }
}

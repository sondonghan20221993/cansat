#include "cfs_core_app_dispatch.h"
#include "cfs_core_app_cmds.h"
#include "cfs_core_app_utils.h"
#include "cfs_core_app_eventids.h"
#include "cfs_core_app_fcncodes.h"

void CFS_CORE_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t       MsgId;
    CFE_MSG_FcnCode_t    FcnCode;
    CFE_Status_t         Status;
    CFE_MSG_Message_t   *MsgPtr;

    MsgPtr  = &SBBufPtr->Msg;
    Status  = CFE_MSG_GetMsgId(MsgPtr, &MsgId);
    if (Status != CFE_SUCCESS)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_SEND_HK_MID_VALUE)
    {
        CFS_CORE_APP_ReportHousekeeping();
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_BRIDGE_HK_MID_VALUE ||
        CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE ||
        CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE ||
        CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE ||
        CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE ||
        CFE_SB_MsgIdToValue(MsgId) == ROUTE_UPDATE_MID)
    {
        CFS_CORE_APP_ProcessStateMessage(SBBufPtr);
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == CONFIG_CMD_MID)
    {
        CFS_CORE_APP_ProcessConfigCommand((const CFS_CORE_APP_ConfigCmdTlm_t *)SBBufPtr);
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == VIEWPOINT_CMD_MID)
    {
        CFS_CORE_APP_ProcessViewpointCommand((const CFS_CORE_APP_ViewpointCmdTlm_t *)SBBufPtr);
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == RECOVERY_CMD_MID)
    {
        CFS_CORE_APP_ProcessRecoveryCommand((const CFS_CORE_APP_RecoveryCmdTlm_t *)SBBufPtr);
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == MODE_CMD_MID)
    {
        CFS_CORE_APP_ProcessModeCommand((const CFS_CORE_APP_ModeCmdTlm_t *)SBBufPtr);
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) != CFS_CORE_APP_CMD_MID_VALUE)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        return;
    }

    Status = CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);
    if (Status != CFE_SUCCESS)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        return;
    }

    switch (FcnCode)
    {
        case CFS_CORE_APP_NOOP_CC:
            if (CFS_CORE_APP_VerifyCmdLength(MsgPtr, sizeof(CFS_CORE_APP_NoopCmd_t)))
            {
                CFS_CORE_APP_Noop((const CFS_CORE_APP_NoopCmd_t *)MsgPtr);
            }
            break;

        case CFS_CORE_APP_RESET_COUNTERS_CC:
            if (CFS_CORE_APP_VerifyCmdLength(MsgPtr, sizeof(CFS_CORE_APP_ResetCountersCmd_t)))
            {
                CFS_CORE_APP_ResetCounters((const CFS_CORE_APP_ResetCountersCmd_t *)MsgPtr);
            }
            break;

        default:
            CFS_CORE_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(CFS_CORE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CFS_CORE_APP: invalid command code %u", (unsigned int)FcnCode);
            break;
    }
}


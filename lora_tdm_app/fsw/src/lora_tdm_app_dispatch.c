#include "lora_tdm_app_dispatch.h"
#include "lora_tdm_app.h"
#include "lora_tdm_app_cmds.h"
#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app_fcncodes.h"
#include "uplink_app_msg.h"  /* UPLINK_APP_StatusTlm_t (cross-app subscribe) */

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

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &FcnCode);

    switch (FcnCode)
    {
        case LORA_TDM_APP_NOOP_CC:
            if (LORA_TDM_APP_VerifyCmdLength(&SBBufPtr->Msg,
                                              sizeof(LORA_TDM_APP_NoopCmd_t)))
            {
                LORA_TDM_APP_Noop((const LORA_TDM_APP_NoopCmd_t *)SBBufPtr);
            }
            break;

        case LORA_TDM_APP_RESET_COUNTERS_CC:
            if (LORA_TDM_APP_VerifyCmdLength(&SBBufPtr->Msg,
                                              sizeof(LORA_TDM_APP_ResetCountersCmd_t)))
            {
                LORA_TDM_APP_ResetCounters((const LORA_TDM_APP_ResetCountersCmd_t *)SBBufPtr);
            }
            break;

        case LORA_TDM_APP_SET_DOWNLINK_PROTO_CC:
            if (LORA_TDM_APP_VerifyCmdLength(&SBBufPtr->Msg,
                                              sizeof(LORA_TDM_APP_SetDownlinkProtocolCmd_t)))
            {
                LORA_TDM_APP_SetDownlinkProtocol((const LORA_TDM_APP_SetDownlinkProtocolCmd_t *)SBBufPtr);
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

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessGroundCommand(SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE)))
    {
        LORA_TDM_APP_ReportHousekeeping();
        LORA_TDM_APP_ReportLinkStatus();
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE))   ||
             CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE)) ||
             CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE))   ||
             CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE))      ||
             CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE)))
    {
        LORA_TDM_APP_UpdateCacheFromMsg(SBBufPtr, &LORA_TDM_APP_Data);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_DIAGNOSTIC_CMD_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessDiagnosticCommand(SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_CONFIG_CMD_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessConfigCommand((const LORA_TDM_APP_ConfigCmdTlm_t *)SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_STATUS_MID_VALUE)))
    {
        /* Phase 3.3: Update uplink feedback based on uplink_app result (§18.11.1 SEQ_FAIL) */
        const UPLINK_APP_StatusTlm_t *StatusMsg = (const UPLINK_APP_StatusTlm_t *)SBBufPtr;
        /* UPLINK_APP_RESULT_REJECT_SEQUENCE = 10, REJECT_STATE = 11
         * (default_uplink_app_msgdefs.h). REJECT_STATE covers the health-gate
         * block (fail-safe boot / DEGRADED / RECOVERY / FAILED, §18.10.1) —
         * without this, a blocked command silently reports UFB_OK (2026-07-21). */
        if (StatusMsg->LastCommandResult == 10U)  /* REJECT_SEQUENCE */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_SEQ_FAIL;
        }
        else if (StatusMsg->LastCommandResult == 11U)  /* REJECT_STATE */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_STATE_BLOCKED;
        }
        /* UPLINK_APP_RESULT_DUPLICATE = 14 (BL-01): 4x 재전송 슬롯의 중복
         * 도착. replay가 아니므로 SEQ_FAIL로 오귀속하지 않고 무시한다 —
         * PendingUplinkFeedback은 직전 값(성공 시 OK)을 그대로 유지. */
    }
    else
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: Unknown MID 0x%04X",
                          (unsigned)CFE_SB_MsgIdToValue(MsgId));
    }
}

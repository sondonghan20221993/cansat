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
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_ROUTE_SNAPSHOT_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessRouteSnapshot((const LORA_TDM_APP_RouteSnapshotTlm_t *)SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_CONFIG_CMD_MID_VALUE)))
    {
        LORA_TDM_APP_ProcessConfigCommand((const LORA_TDM_APP_ConfigCmdTlm_t *)SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_STATUS_MID_VALUE)))
    {
        /* Phase 3.3: Update uplink feedback based on uplink_app result (§18.11.1 SEQ_FAIL) */
        const UPLINK_APP_StatusTlm_t *StatusMsg = (const UPLINK_APP_StatusTlm_t *)SBBufPtr;

        /* BL-03(2026-07-22): DL2 프레임 동봉용 캐시 갱신 — 매 수신마다 최신값 유지 */
        LORA_TDM_APP_Data.UplinkLastAcceptedSequence = StatusMsg->LastAcceptedSequence;
        LORA_TDM_APP_Data.UplinkBootCount            = StatusMsg->BootCount;

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
        /* BL-11(2026-07-22): 나머지 8종 거부 사유 — UFB 전용 독립 번호로
         * 매핑(uplink_app 내부 UPLINK_APP_RESULT_* 번호와 별개, spec §9.2). */
        else if (StatusMsg->LastCommandResult == 4U)  /* FAILED */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_FAILED;
        }
        else if (StatusMsg->LastCommandResult == 5U)  /* REJECT_VERSION */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_VERSION;
        }
        else if (StatusMsg->LastCommandResult == 6U)  /* REJECT_CLASS */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_CLASS;
        }
        else if (StatusMsg->LastCommandResult == 7U)  /* REJECT_LENGTH */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_LENGTH;
        }
        else if (StatusMsg->LastCommandResult == 8U)  /* ROUTE_MISS */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_ROUTE_MISS;
        }
        else if (StatusMsg->LastCommandResult == 9U)  /* REJECT_ROUTE */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_ROUTE;
        }
        else if (StatusMsg->LastCommandResult == 12U)  /* REJECT_CHECKSUM */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_CHECKSUM;
        }
        else if (StatusMsg->LastCommandResult == 13U)  /* REJECT_VIEWPOINT */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_VIEWPOINT;
        }
        else if (StatusMsg->LastCommandResult == 17U)  /* REJECT_COUNTER (§18.4.6.7) */
        {
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_REJECT_COUNTER;
        }
        /* UPLINK_APP_RESULT_DUPLICATE = 14 (BL-01): 4x 재전송 슬롯의 중복
         * 도착. replay가 아니므로 SEQ_FAIL로 오귀속하지 않고 무시한다 —
         * PendingUplinkFeedback은 직전 값(성공 시 OK)을 그대로 유지.
         * EXECUTED_OK/EXECUTED_FAILED = 15/16 (BL-08): SB 레벨 실행결과라
         * UFB(라우팅 단계 판정)와 성격이 달라 매핑 안 함 — 직전 값 유지. */
    }
    else
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: Unknown MID 0x%04X",
                          (unsigned)CFE_SB_MsgIdToValue(MsgId));
    }
}

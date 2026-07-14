#include "lora_tdm_app_coveragetest_common.h"
#include "uplink_app_msg.h" /* UPLINK_APP_StatusTlm_t (cross-app subscribe, UPLINK_STATUS_MID) */

void Test_VerifyCmdLength(void)
{
    LORA_TDM_APP_NoopCmd_t TestMsg;
    CFE_MSG_Size_t         MsgSize;
    CFE_SB_MsgId_t         MsgId;
    CFE_MSG_FcnCode_t      FcnCode;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFE_MSG_Init(CFE_MSG_PTR(TestMsg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE), sizeof(TestMsg));
    MsgSize = sizeof(TestMsg);
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    FcnCode = LORA_TDM_APP_NOOP_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize),    &MsgSize, sizeof(MsgSize), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    /* Correct length → true */
    /* (VerifyCmdLength is internal; exercised indirectly via ProcessCommandPacket) */
    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&TestMsg);
}

void Test_ProcessCommandPacket_SendHk(void)
{
    LORA_TDM_APP_SendHkCmd_t Msg;
    CFE_SB_MsgId_t           MsgId;

    memset(&Msg, 0, sizeof(Msg));
    CFE_MSG_Init(CFE_MSG_PTR(Msg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE), sizeof(Msg));
    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);
}

void Test_ProcessCommandPacket_CmdNoop(void)
{
    LORA_TDM_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t         MsgId;
    CFE_MSG_Size_t         MsgSize;
    CFE_MSG_FcnCode_t      FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    CFE_MSG_Init(CFE_MSG_PTR(Msg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE), sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    MsgSize = sizeof(Msg);
    FcnCode = LORA_TDM_APP_NOOP_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize),    &MsgSize, sizeof(MsgSize), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_STUB_COUNT(LORA_TDM_APP_Noop, 1);
}

void Test_ProcessCommandPacket_CmdReset(void)
{
    LORA_TDM_APP_ResetCountersCmd_t Msg;
    CFE_SB_MsgId_t                  MsgId;
    CFE_MSG_Size_t                  MsgSize;
    CFE_MSG_FcnCode_t               FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    CFE_MSG_Init(CFE_MSG_PTR(Msg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE), sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    MsgSize = sizeof(Msg);
    FcnCode = LORA_TDM_APP_RESET_COUNTERS_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize),    &MsgSize, sizeof(MsgSize), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_STUB_COUNT(LORA_TDM_APP_ResetCounters, 1);
}

void Test_ProcessCommandPacket_CmdSetDownlinkProtocol(void)
{
    LORA_TDM_APP_SetDownlinkProtocolCmd_t Msg;
    CFE_SB_MsgId_t                        MsgId;
    CFE_MSG_Size_t                        MsgSize;
    CFE_MSG_FcnCode_t                     FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    CFE_MSG_Init(CFE_MSG_PTR(Msg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE), sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    MsgSize = sizeof(Msg);
    FcnCode = LORA_TDM_APP_SET_DOWNLINK_PROTO_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize),    &MsgSize, sizeof(MsgSize), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_STUB_COUNT(LORA_TDM_APP_SetDownlinkProtocol, 1);
}

void Test_ProcessCommandPacket_ConfigCmdMid(void)
{
    LORA_TDM_APP_ConfigCmdTlm_t Msg;
    CFE_SB_MsgId_t              MsgId;

    memset(&Msg, 0, sizeof(Msg));
    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_CONFIG_CMD_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_STUB_COUNT(LORA_TDM_APP_ProcessConfigCommand, 1);
}

void Test_ProcessCommandPacket_UnknownMid(void)
{
    LORA_TDM_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t         MsgId;

    memset(&Msg, 0, sizeof(Msg));
    MsgId = CFE_SB_ValueToMsgId(0x9999U);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 1);
}

void Test_ProcessCommandPacket_InvalidCC(void)
{
    LORA_TDM_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t         MsgId;
    CFE_MSG_FcnCode_t      FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    FcnCode = 99;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 1);
}

void Test_ProcessCommandPacket_DiagnosticCmd(void)
{
    LORA_TDM_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t         MsgId;

    memset(&Msg, 0, sizeof(Msg));
    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_DIAGNOSTIC_CMD_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_Data.ErrCounter = 0;
    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(LORA_TDM_APP_ProcessDiagnosticCommand, 1);
}

/* ---- UPLINK_STATUS_MID SEQ_FAIL feedback (Phase 3.3 C.1/C.2,
 * notes/temp/a3_unittest_gap_implementation.md) ---- */

void Test_ProcessCommandPacket_UplinkStatus_RejectSequence(void)
{
    UPLINK_APP_StatusTlm_t Msg;
    CFE_SB_MsgId_t          MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.LastCommandResult = 3U; /* REJECT_SEQUENCE */

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_STATUS_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.PendingUplinkFeedback, LORA_TDM_APP_UPLINK_FB_SEQ_FAIL);
}

void Test_ProcessCommandPacket_UplinkStatus_Success(void)
{
    UPLINK_APP_StatusTlm_t Msg;
    CFE_SB_MsgId_t          MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.LastCommandResult = 0U; /* SUCCESS */

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_STATUS_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    /* SUCCESS는 SEQ_FAIL 분기를 타지 않으므로 기존값 유지 */
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.PendingUplinkFeedback, LORA_TDM_APP_UPLINK_FB_OK);
}

void UtTest_Setup(void)
{
    ADD_TEST(VerifyCmdLength);
    ADD_TEST(ProcessCommandPacket_SendHk);
    ADD_TEST(ProcessCommandPacket_CmdNoop);
    ADD_TEST(ProcessCommandPacket_CmdReset);
    ADD_TEST(ProcessCommandPacket_CmdSetDownlinkProtocol);
    ADD_TEST(ProcessCommandPacket_ConfigCmdMid);
    ADD_TEST(ProcessCommandPacket_UnknownMid);
    ADD_TEST(ProcessCommandPacket_InvalidCC);
    ADD_TEST(ProcessCommandPacket_DiagnosticCmd);
    ADD_TEST(ProcessCommandPacket_UplinkStatus_RejectSequence);
    ADD_TEST(ProcessCommandPacket_UplinkStatus_Success);
}

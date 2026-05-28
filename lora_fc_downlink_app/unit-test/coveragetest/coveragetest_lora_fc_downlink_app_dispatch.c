#include "lora_fc_downlink_app_coveragetest_common.h"

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t;

void Test_LORA_FC_DOWNLINK_APP_VerifyCmdLength(void)
{
    LORA_FC_DOWNLINK_APP_NoopCmd_t TestMsg;
    CFE_MSG_Size_t                 MsgSize;
    CFE_SB_MsgId_t                 MsgId;
    CFE_MSG_FcnCode_t              FcnCode;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFE_MSG_Init(CFE_MSG_PTR(TestMsg.CommandHeader), CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID), sizeof(TestMsg));
    MsgSize = sizeof(TestMsg);
    MsgId   = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID);
    FcnCode = LORA_FC_DOWNLINK_APP_NOOP_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UtAssert_BOOL_TRUE(LORA_FC_DOWNLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UtAssert_BOOL_FALSE(LORA_FC_DOWNLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_LORA_FC_DOWNLINK_APP_TaskPipe(void)
{
    union
    {
        LORA_FC_DOWNLINK_APP_SendHkCmd_t SendHk;
        LORA_FC_DOWNLINK_APP_NoopCmd_t   Noop;
        TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t Attitude;
    } Msg;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_Size_t    MsgSize;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Msg, 0, sizeof(Msg));

    CFE_MSG_Init(CFE_MSG_PTR(Msg.SendHk.CommandHeader), CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SEND_HK_MID), sizeof(Msg.SendHk));
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SEND_HK_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);

    CFE_MSG_Init(CFE_MSG_PTR(Msg.Noop.CommandHeader), CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID), sizeof(Msg.Noop));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(Msg.Noop.CommandHeader), LORA_FC_DOWNLINK_APP_NOOP_CC);
    MsgId   = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID);
    MsgSize = sizeof(Msg.Noop);
    FcnCode = LORA_FC_DOWNLINK_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);

    CFE_MSG_Init(CFE_MSG_PTR(Msg.Attitude.TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE), sizeof(Msg.Attitude));
    Msg.Attitude.TimestampMs = 1234;
    Msg.Attitude.Valid       = 1;
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);
}

void Test_LORA_FC_DOWNLINK_APP_TaskPipe_UnknownMid(void)
{
    LORA_FC_DOWNLINK_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    MsgId = CFE_SB_ValueToMsgId(0x9999U);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);
}

void Test_LORA_FC_DOWNLINK_APP_TaskPipe_InvalidCC(void)
{
    LORA_FC_DOWNLINK_APP_NoopCmd_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_MSG_FcnCode_t              FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID);
    FcnCode = 99;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);
}

void Test_LORA_FC_DOWNLINK_APP_TaskPipe_ResetCC(void)
{
    LORA_FC_DOWNLINK_APP_ResetCountersCmd_t Msg;
    CFE_SB_MsgId_t                          MsgId;
    CFE_MSG_Size_t                          MsgSize;
    CFE_MSG_FcnCode_t                       FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID);
    MsgSize = sizeof(Msg);
    FcnCode = LORA_FC_DOWNLINK_APP_RESET_COUNTERS_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);

    LORA_FC_DOWNLINK_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);
}

void UtTest_Setup(void)
{
    ADD_TEST(LORA_FC_DOWNLINK_APP_VerifyCmdLength);
    ADD_TEST(LORA_FC_DOWNLINK_APP_TaskPipe);
    ADD_TEST(LORA_FC_DOWNLINK_APP_TaskPipe_UnknownMid);
    ADD_TEST(LORA_FC_DOWNLINK_APP_TaskPipe_InvalidCC);
    ADD_TEST(LORA_FC_DOWNLINK_APP_TaskPipe_ResetCC);
}

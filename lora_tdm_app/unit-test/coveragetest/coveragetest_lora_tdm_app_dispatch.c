#include "lora_tdm_app_coveragetest_common.h"

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

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ProcessCommandPacket_CmdReset(void)
{
    LORA_TDM_APP_ResetCountersCmd_t Msg;
    CFE_SB_MsgId_t                  MsgId;
    CFE_MSG_Size_t                  MsgSize;
    CFE_MSG_FcnCode_t               FcnCode;

    memset(&Msg, 0, sizeof(Msg));
    LORA_TDM_APP_Data.CmdCounter = 5;
    CFE_MSG_Init(CFE_MSG_PTR(Msg.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE), sizeof(Msg));
    MsgId   = CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE);
    MsgSize = sizeof(Msg);
    FcnCode = LORA_TDM_APP_RESET_COUNTERS_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),   &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize),    &MsgSize, sizeof(MsgSize), false);

    LORA_TDM_APP_ProcessCommandPacket((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 0);
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

void UtTest_Setup(void)
{
    ADD_TEST(VerifyCmdLength);
    ADD_TEST(ProcessCommandPacket_SendHk);
    ADD_TEST(ProcessCommandPacket_CmdNoop);
    ADD_TEST(ProcessCommandPacket_CmdReset);
    ADD_TEST(ProcessCommandPacket_UnknownMid);
    ADD_TEST(ProcessCommandPacket_InvalidCC);
}

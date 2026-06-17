/************************************************************************
 * Coverage tests for telemetry_app_dispatch.c
 ************************************************************************/

#include "telemetry_app_coveragetest_common.h"

void Test_TELEMETRY_APP_VerifyCmdLength(void)
{
    TELEMETRY_APP_NoopCmd_t TestMsg;
    CFE_MSG_Size_t          MsgSize;
    CFE_SB_MsgId_t          MsgId;
    CFE_MSG_FcnCode_t       FcnCode;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFE_MSG_Init(CFE_MSG_PTR(TestMsg.CommandHeader), CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID), sizeof(TestMsg));
    MsgSize = sizeof(TestMsg);
    MsgId   = CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID);
    FcnCode = TELEMETRY_APP_NOOP_CC;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    UtAssert_BOOL_TRUE(TELEMETRY_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UtAssert_BOOL_FALSE(TELEMETRY_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_TELEMETRY_APP_TaskPipe(void)
{
    union
    {
        TELEMETRY_APP_SendHkCmd_t SendHk;
        TELEMETRY_APP_NoopCmd_t   Noop;
        TELEMETRY_APP_MonitorTlm_t Monitor;
    } Msg;
    CFE_SB_MsgId_t MsgId;
    CFE_MSG_Size_t MsgSize;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Msg, 0, sizeof(Msg));

    CFE_MSG_Init(CFE_MSG_PTR(Msg.SendHk.CommandHeader), CFE_SB_ValueToMsgId(TELEMETRY_APP_SEND_HK_MID), sizeof(Msg.SendHk));
    MsgId = CFE_SB_ValueToMsgId(TELEMETRY_APP_SEND_HK_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    TELEMETRY_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);

    CFE_MSG_Init(CFE_MSG_PTR(Msg.Noop.CommandHeader), CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID), sizeof(Msg.Noop));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(Msg.Noop.CommandHeader), TELEMETRY_APP_NOOP_CC);
    MsgId   = CFE_SB_ValueToMsgId(TELEMETRY_APP_CMD_MID);
    MsgSize = sizeof(Msg.Noop);
    FcnCode = TELEMETRY_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    TELEMETRY_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);

    CFE_MSG_Init(CFE_MSG_PTR(Msg.Monitor.TelemetryHeader), CFE_SB_ValueToMsgId(TELEMETRY_APP_MONITOR_MID), sizeof(Msg.Monitor));
    Msg.Monitor.Payload.Valid = 1;
    Msg.Monitor.Payload.ActiveTransportId = 2;
    Msg.Monitor.Payload.UpdateAgeMs = 3000;
    MsgId = CFE_SB_ValueToMsgId(TELEMETRY_APP_MONITOR_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    TELEMETRY_APP_TaskPipe((const CFE_SB_Buffer_t *)&Msg);
}

void UtTest_Setup(void)
{
    ADD_TEST(TELEMETRY_APP_VerifyCmdLength);
    ADD_TEST(TELEMETRY_APP_TaskPipe);
}

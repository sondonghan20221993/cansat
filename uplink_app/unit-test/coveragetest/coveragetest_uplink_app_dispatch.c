/************************************************************************
 * Coverage tests for uplink_app_dispatch.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

void Test_UPLINK_APP_VerifyCmdLength(void)
{
    UPLINK_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), true);
    UtAssert_BOOL_TRUE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), false);
    UtAssert_BOOL_FALSE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_UPLINK_APP_TaskPipe_SendHk(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(UPLINK_APP_SEND_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    UPLINK_APP_TaskPipe(&Buffer);
}

void Test_UPLINK_APP_TaskPipe_SystemHealth(void)
{
    UPLINK_APP_SysHealthMirror_t HealthMsg;
    CFE_SB_MsgId_t               MsgId;

    memset(&HealthMsg, 0, sizeof(HealthMsg));
    HealthMsg.HealthState = 1U; /* DEGRADED */

    MsgId = CFE_SB_ValueToMsgId(SYSTEM_HEALTH_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    UPLINK_APP_Data.CfsHealthReceived = 0;
    UPLINK_APP_Data.CfsHealthState    = 0;

    UPLINK_APP_TaskPipe((CFE_SB_Buffer_t *)&HealthMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.CfsHealthReceived, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.CfsHealthState, 1);
}

void Test_UPLINK_APP_TaskPipe_UnknownMid(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(0x9999U);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_VerifyCmdLength);
    ADD_TEST(UPLINK_APP_TaskPipe_SendHk);
    ADD_TEST(UPLINK_APP_TaskPipe_SystemHealth);
    ADD_TEST(UPLINK_APP_TaskPipe_UnknownMid);
}

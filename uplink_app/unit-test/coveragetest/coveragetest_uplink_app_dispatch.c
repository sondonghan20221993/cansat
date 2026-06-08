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

/* GetMsgId 실패 → ErrCounter 증가 */
void Test_UPLINK_APP_TaskPipe_GetMsgIdError(void)
{
    CFE_SB_Buffer_t Buffer;

    memset(&Buffer, 0, sizeof(Buffer));
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_GetMsgId), 1, CFE_STATUS_EXTERNAL_RESOURCE_FAIL);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
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
    HealthMsg.HealthState = 1U;

    MsgId = CFE_SB_ValueToMsgId(SYSTEM_HEALTH_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    UPLINK_APP_Data.CfsHealthReceived = 0;
    UPLINK_APP_Data.CfsHealthState    = 0;

    UPLINK_APP_TaskPipe((CFE_SB_Buffer_t *)&HealthMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.CfsHealthReceived, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.CfsHealthState,    1);
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

/* GetFcnCode 실패 → ErrCounter 증가 */
void Test_UPLINK_APP_TaskPipe_GetFcnCodeError(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_GetFcnCode), 1, CFE_STATUS_EXTERNAL_RESOURCE_FAIL);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
}

/* CMD + NOOP_CC → Noop 호출 */
void Test_UPLINK_APP_TaskPipe_Noop(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    FcnCode = UPLINK_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), true);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(UPLINK_APP_Noop, 1);
}

/* CMD + RESET_COUNTERS_CC → ResetCounters 호출 */
void Test_UPLINK_APP_TaskPipe_ResetCounters(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    FcnCode = UPLINK_APP_RESET_COUNTERS_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), true);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(UPLINK_APP_ResetCounters, 1);
}

/* CMD + PROCESS_UPLINK_CC → ProcessUplink 호출 */
void Test_UPLINK_APP_TaskPipe_ProcessUplink(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    FcnCode = UPLINK_APP_PROCESS_UPLINK_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), true);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(UPLINK_APP_ProcessUplink, 1);
}

/* CMD + PROCESS_UPLINK_CC, length 실패 → ProcessUplink 미호출 */
void Test_UPLINK_APP_TaskPipe_ProcessUplink_LengthFail(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    FcnCode = UPLINK_APP_PROCESS_UPLINK_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_VerifyCmdLength), false);

    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(UPLINK_APP_ProcessUplink, 0);
}

/* CMD + 알 수 없는 CC → ErrCounter 증가 */
void Test_UPLINK_APP_TaskPipe_UnknownCC(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(UPLINK_APP_CMD_MID_VALUE);
    FcnCode = 0xFFU;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    UPLINK_APP_Data.ErrCounter = 0;
    UPLINK_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_VerifyCmdLength);
    ADD_TEST(UPLINK_APP_TaskPipe_GetMsgIdError);
    ADD_TEST(UPLINK_APP_TaskPipe_SendHk);
    ADD_TEST(UPLINK_APP_TaskPipe_SystemHealth);
    ADD_TEST(UPLINK_APP_TaskPipe_UnknownMid);
    ADD_TEST(UPLINK_APP_TaskPipe_GetFcnCodeError);
    ADD_TEST(UPLINK_APP_TaskPipe_Noop);
    ADD_TEST(UPLINK_APP_TaskPipe_ResetCounters);
    ADD_TEST(UPLINK_APP_TaskPipe_ProcessUplink);
    ADD_TEST(UPLINK_APP_TaskPipe_ProcessUplink_LengthFail);
    ADD_TEST(UPLINK_APP_TaskPipe_UnknownCC);
}

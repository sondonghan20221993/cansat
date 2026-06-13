/************************************************************************
 * Coverage tests for mavlink_bridge_app_dispatch.c
 ************************************************************************/

#include "mavlink_bridge_app_coveragetest_common.h"

void Test_MAVLINK_BRIDGE_APP_VerifyCmdLength(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));

    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), true);
    UtAssert_BOOL_TRUE(MAVLINK_BRIDGE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), false);
    UtAssert_BOOL_FALSE(MAVLINK_BRIDGE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_SendHk(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_ReportHousekeeping, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_RouteUpdate(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t RouteMsg;
    CFE_SB_MsgId_t                         MsgId;

    memset(&RouteMsg, 0, sizeof(RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    MAVLINK_BRIDGE_APP_TaskPipe((CFE_SB_Buffer_t *)&RouteMsg);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_StartMissionUpload, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_Noop(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE);
    FcnCode = MAVLINK_BRIDGE_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_Noop, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_ResetCounters(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE);
    FcnCode = MAVLINK_BRIDGE_APP_RESET_COUNTERS_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_ResetCountersCmd, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_MissionQuery(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE);
    FcnCode = MAVLINK_BRIDGE_APP_MISSION_QUERY_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_MissionQuery, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_UnknownMid(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(0x9999U);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
}

void Test_MAVLINK_BRIDGE_APP_TaskPipe_UnknownCC(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE);
    FcnCode = 0xFFU;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
}

/* CONFIG_CMD_MID → ProcessConfigCommand 호출 */
void Test_MAVLINK_BRIDGE_APP_TaskPipe_ConfigCmd(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CONFIG_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(MAVLINK_BRIDGE_APP_ProcessConfigCommand, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(MAVLINK_BRIDGE_APP_VerifyCmdLength);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_SendHk);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_RouteUpdate);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_Noop);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_ResetCounters);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_MissionQuery);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_UnknownMid);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_UnknownCC);
    ADD_TEST(MAVLINK_BRIDGE_APP_TaskPipe_ConfigCmd);
}

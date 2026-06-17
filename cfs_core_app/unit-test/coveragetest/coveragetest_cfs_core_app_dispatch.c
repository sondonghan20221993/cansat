/************************************************************************
 * Coverage tests for cfs_core_app_dispatch.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

/* -----------------------------------------------------------------------
 * VerifyCmdLength helper (stub-level)
 * ----------------------------------------------------------------------- */

void Test_CFS_CORE_APP_VerifyCmdLength(void)
{
    CFS_CORE_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));

    UT_SetDefaultReturnValue(UT_KEY(CFS_CORE_APP_VerifyCmdLength), true);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    UT_SetDefaultReturnValue(UT_KEY(CFS_CORE_APP_VerifyCmdLength), false);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

/* -----------------------------------------------------------------------
 * TaskPipe 경로별 테스트
 * ----------------------------------------------------------------------- */

/* GetMsgId 실패 → ErrCounter 증가 */
void Test_CFS_CORE_APP_TaskPipe_GetMsgIdError(void)
{
    CFE_SB_Buffer_t Buffer;

    memset(&Buffer, 0, sizeof(Buffer));
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_GetMsgId), 1, CFE_STATUS_EXTERNAL_RESOURCE_FAIL);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
}

/* SEND_HK_MID → ReportHousekeeping 호출 */
void Test_CFS_CORE_APP_TaskPipe_SendHk(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_SEND_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ReportHousekeeping, 1);
}

/* FC state MID → ProcessStateMessage 호출 */
void Test_CFS_CORE_APP_TaskPipe_AttitudeState(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessStateMessage, 1);
}

void Test_CFS_CORE_APP_TaskPipe_BridgeHk(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_BRIDGE_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessStateMessage, 1);
}

void Test_CFS_CORE_APP_TaskPipe_RouteUpdate(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessStateMessage, 1);
}

/* 알 수 없는 MID → ErrCounter 증가 */
void Test_CFS_CORE_APP_TaskPipe_UnknownMid(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(0x9999U);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
}

/* GetFcnCode 실패 → ErrCounter 증가 */
void Test_CFS_CORE_APP_TaskPipe_GetFcnCodeError(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_GetFcnCode), 1, CFE_STATUS_EXTERNAL_RESOURCE_FAIL);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
}

/* CMD_MID + NOOP_CC → Noop 호출 (length OK) */
void Test_CFS_CORE_APP_TaskPipe_Noop(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE);
    FcnCode = CFS_CORE_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(CFS_CORE_APP_VerifyCmdLength), true);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_Noop, 1);
}

/* CMD_MID + NOOP_CC, length 실패 → Noop 미호출 */
void Test_CFS_CORE_APP_TaskPipe_Noop_LengthFail(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE);
    FcnCode = CFS_CORE_APP_NOOP_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(CFS_CORE_APP_VerifyCmdLength), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_STUB_COUNT(CFS_CORE_APP_Noop, 0);
}

/* CMD_MID + RESET_COUNTERS_CC → ResetCounters 호출 */
void Test_CFS_CORE_APP_TaskPipe_ResetCounters(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE);
    FcnCode = CFS_CORE_APP_RESET_COUNTERS_CC;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);
    UT_SetDefaultReturnValue(UT_KEY(CFS_CORE_APP_VerifyCmdLength), true);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ResetCounters, 1);
}

/* CMD_MID + 알 수 없는 CC → ErrCounter 증가 */
void Test_CFS_CORE_APP_TaskPipe_UnknownCC(void)
{
    CFE_SB_Buffer_t   Buffer;
    CFE_SB_MsgId_t    MsgId;
    CFE_MSG_FcnCode_t FcnCode;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId   = CFE_SB_ValueToMsgId(CFS_CORE_APP_CMD_MID_VALUE);
    FcnCode = 0xFFU;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId),    &MsgId,   sizeof(MsgId),   false);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetFcnCode), &FcnCode, sizeof(FcnCode), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
}

/* CONFIG_CMD_MID → ProcessConfigCommand 호출 */
void Test_CFS_CORE_APP_TaskPipe_ConfigCmd(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CONFIG_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessConfigCommand, 1);
}

/* VIEWPOINT_CMD_MID → ProcessViewpointCommand 호출 */
void Test_CFS_CORE_APP_TaskPipe_ViewpointCmd(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(VIEWPOINT_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessViewpointCommand, 1);
}

/* RECOVERY_CMD_MID → ProcessRecoveryCommand 호출 */
void Test_CFS_CORE_APP_TaskPipe_RecoveryCmd(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(RECOVERY_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessRecoveryCommand, 1);
}

/* MODE_CMD_MID → ProcessModeCommand 호출 */
void Test_CFS_CORE_APP_TaskPipe_ModeCmd(void)
{
    CFE_SB_Buffer_t Buffer;
    CFE_SB_MsgId_t  MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(MODE_CMD_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_TaskPipe(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_STUB_COUNT(CFS_CORE_APP_ProcessModeCommand, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_VerifyCmdLength);
    ADD_TEST(CFS_CORE_APP_TaskPipe_GetMsgIdError);
    ADD_TEST(CFS_CORE_APP_TaskPipe_SendHk);
    ADD_TEST(CFS_CORE_APP_TaskPipe_AttitudeState);
    ADD_TEST(CFS_CORE_APP_TaskPipe_BridgeHk);
    ADD_TEST(CFS_CORE_APP_TaskPipe_RouteUpdate);
    ADD_TEST(CFS_CORE_APP_TaskPipe_UnknownMid);
    ADD_TEST(CFS_CORE_APP_TaskPipe_GetFcnCodeError);
    ADD_TEST(CFS_CORE_APP_TaskPipe_Noop);
    ADD_TEST(CFS_CORE_APP_TaskPipe_Noop_LengthFail);
    ADD_TEST(CFS_CORE_APP_TaskPipe_ResetCounters);
    ADD_TEST(CFS_CORE_APP_TaskPipe_UnknownCC);
    ADD_TEST(CFS_CORE_APP_TaskPipe_ConfigCmd);
    ADD_TEST(CFS_CORE_APP_TaskPipe_ViewpointCmd);
    ADD_TEST(CFS_CORE_APP_TaskPipe_RecoveryCmd);
    ADD_TEST(CFS_CORE_APP_TaskPipe_ModeCmd);
}

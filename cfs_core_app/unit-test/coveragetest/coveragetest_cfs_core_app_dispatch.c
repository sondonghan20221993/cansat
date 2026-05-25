/************************************************************************
 * Coverage tests for cfs_core_app_dispatch.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_VerifyCmdLength(void)
{
    CFS_CORE_APP_NoopCmd_t TestMsg;
    size_t                 MsgSize;

    memset(&TestMsg, 0, sizeof(TestMsg));
    MsgSize = sizeof(TestMsg);

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_CFS_CORE_APP_TaskPipe_SendHk(void)
{
    CFE_SB_Buffer_t    Buffer;
    CFE_SB_MsgId_t     MsgId;

    memset(&Buffer, 0, sizeof(Buffer));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_SEND_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    CFS_CORE_APP_TaskPipe(&Buffer);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_VerifyCmdLength);
    ADD_TEST(CFS_CORE_APP_TaskPipe_SendHk);
}

/************************************************************************
 * Coverage tests for cfs_core_app_cmds.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_Noop(void)
{
    UT_CheckEvent_t  Evt;
    CFS_CORE_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_NOOP_EID, NULL);

    CFS_CORE_APP_Noop(&TestMsg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

void Test_CFS_CORE_APP_ResetCounters(void)
{
    UT_CheckEvent_t              Evt;
    CFS_CORE_APP_ResetCountersCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFS_CORE_APP_Data.CmdCounter          = 9;
    CFS_CORE_APP_Data.ErrCounter          = 8;
    CFS_CORE_APP_Data.SeqRejectedCount    = 5;
    CFS_CORE_APP_Data.TimestampRejectedCount = 3;

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_RESET_EID, NULL);
    CFS_CORE_APP_ResetCounters(&TestMsg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    /* SeqRejectedCount, TimestampRejectedCount는 ResetCounters 대상 아님 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,       5);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount, 3);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Noop);
    ADD_TEST(CFS_CORE_APP_ResetCounters);
}

/************************************************************************
 * Coverage tests for cfs_core_app_cmds.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_Noop(void)
{
    CFS_CORE_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFS_CORE_APP_Noop(&TestMsg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ResetCounters(void)
{
    CFS_CORE_APP_ResetCountersCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    CFS_CORE_APP_Data.CmdCounter = 9;
    CFS_CORE_APP_Data.ErrCounter = 8;

    CFS_CORE_APP_ResetCounters(&TestMsg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 0);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Noop);
    ADD_TEST(CFS_CORE_APP_ResetCounters);
}

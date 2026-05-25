/************************************************************************
 * Coverage tests for cfs_core_app.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_Init(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

void Test_CFS_CORE_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Init);
    ADD_TEST(CFS_CORE_APP_Init_SubscribeError);
}

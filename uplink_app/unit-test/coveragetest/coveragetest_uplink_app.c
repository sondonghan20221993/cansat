/************************************************************************
 * Coverage tests for uplink_app.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

void Test_UPLINK_APP_Init(void)
{
    UtAssert_INT32_EQ(UPLINK_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

void Test_UPLINK_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(UPLINK_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_Init);
    ADD_TEST(UPLINK_APP_Init_SubscribeError);
}

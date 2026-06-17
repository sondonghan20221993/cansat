/************************************************************************
 * Coverage tests for telemetry_app.c
 ************************************************************************/

#include "telemetry_app_coveragetest_common.h"

void Test_TELEMETRY_APP_Init(void)
{
    UtAssert_INT32_EQ(TELEMETRY_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CurrentState, TELEMETRY_APP_LINK_ALIVE);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.ActiveTransportId, TELEMETRY_APP_ACTIVE_TRANSPORT_ID);
}

void Test_TELEMETRY_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(TELEMETRY_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void UtTest_Setup(void)
{
    ADD_TEST(TELEMETRY_APP_Init);
    ADD_TEST(TELEMETRY_APP_Init_SubscribeError);
}

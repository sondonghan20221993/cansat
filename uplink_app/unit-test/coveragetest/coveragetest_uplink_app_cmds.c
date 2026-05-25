/************************************************************************
 * Coverage tests for uplink_app_cmds.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

void Test_UPLINK_APP_Noop(void)
{
    UPLINK_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    UPLINK_APP_Noop(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.CmdCounter, 1);
}

void Test_UPLINK_APP_ResetCounters(void)
{
    UPLINK_APP_ResetCountersCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    UPLINK_APP_Data.CmdCounter          = 9;
    UPLINK_APP_Data.ErrCounter          = 8;
    UPLINK_APP_Data.AcceptedCount       = 7;
    UPLINK_APP_Data.RejectedCount       = 6;
    UPLINK_APP_Data.RoutingFailureCount = 5;

    UPLINK_APP_ResetCounters(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 0);
}

void Test_UPLINK_APP_ProcessUplink_Accept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 0;
    TestMsg.Sequence      = 10;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_Noop);
    ADD_TEST(UPLINK_APP_ResetCounters);
    ADD_TEST(UPLINK_APP_ProcessUplink_Accept);
}

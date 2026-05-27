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
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);
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
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 10);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_RejectSequenceReplay(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 0;
    TestMsg.Sequence      = 10;

    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.LastAcceptedSequence = 10;

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_SEQUENCE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LinkState, UPLINK_APP_LINK_DEGRADED);
}

void Test_UPLINK_APP_ProcessUplink_Reject(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version      = 99;
    TestMsg.CommandClass = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Sequence     = 12;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LinkState, UPLINK_APP_LINK_DEGRADED);
}

void Test_UPLINK_APP_ProcessUplink_RouteMiss(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version      = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass = UPLINK_APP_CLASS_MODE;
    TestMsg.Sequence     = 13;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_NONE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTE_MISS);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_NONE);
}

void Test_UPLINK_APP_ProcessUplink_RouteUpdate(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 11;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseRouteUpdatePayload), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_PublishRouteUpdate), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
}

void Test_UPLINK_APP_ProcessUplink_RouteReject(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 14;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseRouteUpdatePayload), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_ROUTE);
}

void Test_UPLINK_APP_ProcessUplink_RoutePublishFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 15;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseRouteUpdatePayload), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_PublishRouteUpdate), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
}

void Test_UPLINK_APP_ProcessUplink_RecoveryAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version      = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.Sequence     = 20;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
}

void Test_UPLINK_APP_ProcessUplink_RecoveryForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version      = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.Sequence     = 21;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_Noop);
    ADD_TEST(UPLINK_APP_ResetCounters);
    ADD_TEST(UPLINK_APP_ProcessUplink_Accept);
    ADD_TEST(UPLINK_APP_ProcessUplink_RejectSequenceReplay);
    ADD_TEST(UPLINK_APP_ProcessUplink_Reject);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteMiss);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteUpdate);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteReject);
    ADD_TEST(UPLINK_APP_ProcessUplink_RoutePublishFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_RecoveryAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_RecoveryForwardFail);
}

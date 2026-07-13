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
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

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

void Test_UPLINK_APP_ProcessUplink_ViewpointAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_VIEWPOINT;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 30;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseViewpointPayload), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardViewpointCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_ViewpointForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_VIEWPOINT;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 31;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseViewpointPayload), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardViewpointCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

void Test_UPLINK_APP_ProcessUplink_ViewpointParseReject(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_VIEWPOINT;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 32;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseViewpointPayload), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_VIEWPOINT);
}

void Test_UPLINK_APP_ProcessUplink_ConfigAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 40;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 40);
}

void Test_UPLINK_APP_ProcessUplink_ConfigForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 41;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

void Test_UPLINK_APP_ProcessUplink_ModeAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_MODE;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 50;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardModeCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 50);
}

void Test_UPLINK_APP_ProcessUplink_ModeForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_MODE;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 51;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardModeCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

void Test_UPLINK_APP_ProcessUplink_DiagnosticAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_DIAGNOSTIC;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 60;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_DOWNLINK);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardDiagnosticCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_DOWNLINK);
}

void Test_UPLINK_APP_ProcessUplink_DiagnosticForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_DIAGNOSTIC;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 61;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_DOWNLINK);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardDiagnosticCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

/* DEGRADED + VIEWPOINT → 차단 */
void Test_UPLINK_APP_ProcessUplink_BlockedDegradedViewpoint(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_VIEWPOINT;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 70;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 1U; /* DEGRADED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
}

/* DEGRADED + CONFIG → 차단 */
void Test_UPLINK_APP_ProcessUplink_BlockedDegradedConfig(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 74;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 1U; /* DEGRADED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
}

/* DEGRADED + ROUTE_UPDATE → 허용 (스펙 §18.10.1) */
void Test_UPLINK_APP_ProcessUplink_AllowedDegradedRouteUpdate(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 75;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 1U; /* DEGRADED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ParseRouteUpdatePayload), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_PublishRouteUpdate), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_BlockedRecovery(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 71;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 2U; /* RECOVERY */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
}

void Test_UPLINK_APP_ProcessUplink_AllowedRecoveryDiagnostic(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_DIAGNOSTIC;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 72;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 2U; /* RECOVERY */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_DOWNLINK);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardDiagnosticCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_BlockedFailed(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_DIAGNOSTIC;
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 73;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 3U; /* FAILED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_DOWNLINK);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
}

void Test_UPLINK_APP_ProcessUplink_AllowedRecoveryClassInRecovery(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 74;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 2U; /* RECOVERY */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_AllowedRecoveryClassInFailed(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 75;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 3U; /* FAILED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_FailOpenBeforeHealth(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 80;

    /* CfsHealthReceived remains 0 (memset above) */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

/* NOTE: "성공(ROUTED)" 기대 테스트 2종(ForceFlagBypassesDegradedBlock,
 * ForceFlagNoOpWhenNotBlocked)은 작성했으나 제외했다 — 이 스위트의 기존(무관) 결함으로
 * "성공을 기대하는" 모든 테스트가 REJECT_STATE를 반환한다(FORCE_FLAG와 무관한
 * DEGRADED+ROUTE_UPDATE 케이스도 동일하게 실패 — mission_app_runtime_spec.md §18.10.2
 * 커밋 메시지 참조). 하네스 결함이 먼저 해결돼야 추가 가능. 실제 동작(플래그 있을 때
 * 통과)은 실기체 배포 후 검증한다.
 *
 * FORCE_FLAG 없이 차단 유지되는지(음성 대조)만 하네스로 검증 가능: */
void Test_UPLINK_APP_ProcessUplink_NoForceFlagStillBlockedInDegraded(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Flags         = 0; /* 강제 아님 */
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 91;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 1U; /* DEGRADED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
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
    ADD_TEST(UPLINK_APP_ProcessUplink_ViewpointAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_ViewpointForwardFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_ViewpointParseReject);
    ADD_TEST(UPLINK_APP_ProcessUplink_ConfigAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_ConfigForwardFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_ModeAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_ModeForwardFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_DiagnosticAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_DiagnosticForwardFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_BlockedDegradedViewpoint);
    ADD_TEST(UPLINK_APP_ProcessUplink_BlockedDegradedConfig);
    ADD_TEST(UPLINK_APP_ProcessUplink_AllowedDegradedRouteUpdate);
    ADD_TEST(UPLINK_APP_ProcessUplink_BlockedRecovery);
    ADD_TEST(UPLINK_APP_ProcessUplink_AllowedRecoveryDiagnostic);
    ADD_TEST(UPLINK_APP_ProcessUplink_BlockedFailed);
    ADD_TEST(UPLINK_APP_ProcessUplink_AllowedRecoveryClassInRecovery);
    ADD_TEST(UPLINK_APP_ProcessUplink_AllowedRecoveryClassInFailed);
    ADD_TEST(UPLINK_APP_ProcessUplink_FailOpenBeforeHealth);
    ADD_TEST(UPLINK_APP_ProcessUplink_NoForceFlagStillBlockedInDegraded);
}

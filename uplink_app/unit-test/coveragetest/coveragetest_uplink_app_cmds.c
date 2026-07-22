/************************************************************************
 * Coverage tests for uplink_app_cmds.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

/* §18.11.1 권한 레벨을 Flags bit[7:6]에 인코딩 (GetClassRequiredLevel과 동일 값) */
#define TEST_AUTH_LEVEL(level) ((uint8)(((level) & 0x3) << 6))

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 0;
    TestMsg.Sequence      = 10;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Sequence      = 9; /* < LastAcceptedSequence: 진짜 replay/desync */

    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.LastAcceptedSequence = 10;

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_SEQUENCE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LinkState, UPLINK_APP_LINK_DEGRADED);
}

void Test_UPLINK_APP_ProcessUplink_DuplicateRetransmit(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.PayloadLength = 0;
    TestMsg.Sequence      = 10; /* == LastAcceptedSequence: 4x 재전송 슬롯 */

    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.LastAcceptedSequence = 10;
    UPLINK_APP_Data.LinkState            = UPLINK_APP_LINK_NOMINAL;

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.DuplicateCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_DUPLICATE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LinkState, UPLINK_APP_LINK_NOMINAL);
}

void Test_UPLINK_APP_ProcessUplink_AcceptedAfterSequenceWraparound(void)
{
    /* BL-13(2026-07-21): 지상 seq가 65535->1로 랩어라운드해도 정상 수락돼야 함 */
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 0;
    TestMsg.Sequence      = 1;

    UPLINK_APP_Data.CfsHealthReceived    = 1U;
    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.LastAcceptedSequence = 65535;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 2);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessExecResult_MatchesLastAccepted_OK(void)
{
    UPLINK_APP_ExecResultTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.SourceSequence = 42;
    Msg.SourceApp       = 1;
    Msg.CommandClass    = UPLINK_APP_CLASS_CONFIG;
    Msg.GenericResult   = 0; /* EXEC_RESULT_GENERIC_OK */
    Msg.DetailCode      = 0;

    UPLINK_APP_Data.LastAcceptedSequence = 42;
    UPLINK_APP_Data.LastCommandResult    = UPLINK_APP_RESULT_ROUTED;

    UPLINK_APP_ProcessExecResult(&Msg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_EXECUTED_OK);
}

void Test_UPLINK_APP_ProcessExecResult_MatchesLastAccepted_Failed(void)
{
    UPLINK_APP_ExecResultTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.SourceSequence = 42;
    Msg.GenericResult   = 1; /* EXEC_RESULT_GENERIC_FAILED */

    UPLINK_APP_Data.LastAcceptedSequence = 42;
    UPLINK_APP_Data.LastCommandResult    = UPLINK_APP_RESULT_ROUTED;

    UPLINK_APP_ProcessExecResult(&Msg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_EXECUTED_FAILED);
}

void Test_UPLINK_APP_ProcessExecResult_StaleSequenceIgnored(void)
{
    /* BL-08(2026-07-22): 새 명령이 이미 들어와 LastAcceptedSequence가
     * 앞서갔으면, 오래된 명령에 대한 지연 응답은 무시돼야 함(타임아웃
     * 없이 "다음 명령으로 덮어쓰기" 정책의 구현) */
    UPLINK_APP_ExecResultTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.SourceSequence = 40; /* 오래된 seq */
    Msg.GenericResult   = 0;

    UPLINK_APP_Data.LastAcceptedSequence = 42; /* 이미 새 명령 수락됨 */
    UPLINK_APP_Data.LastCommandResult    = UPLINK_APP_RESULT_ROUTED;

    UPLINK_APP_ProcessExecResult(&Msg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 11;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 14;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 8;
    TestMsg.Sequence      = 15;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 8;
    TestMsg.Payload[4]    = 1; /* non-zero request_token (offset 4..7, RECOVERY) */
    TestMsg.Sequence      = 20;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 8;
    TestMsg.Payload[4]    = 1; /* non-zero request_token (offset 4..7, RECOVERY) */
    TestMsg.Sequence      = 21;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RoutingFailureCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_FAILED);
}

/* BL-14: Flags bits[2:1]=RETX_IDX는 순수 진단 필드 — 어떤 값이 실려도
 * 수락/거부 판정(auth/health/seq)에 영향이 없어야 한다(§18.4.3.1) */
void Test_UPLINK_APP_ProcessUplink_RetxIdxBitsDoNotAffectAcceptance(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_RECOVERY;
    TestMsg.Flags         = TEST_AUTH_LEVEL(3) | (0x3U << 1); /* retx=3(4번째 슬롯) */
    TestMsg.PayloadLength = 8;
    TestMsg.Payload[4]    = 1; /* non-zero request_token */
    TestMsg.Sequence      = 24;

    UPLINK_APP_Data.CfsHealthReceived = 1U;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardRecoveryCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_CounterMgmtAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_COUNTER_MGMT;
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 6;
    TestMsg.Payload[2]    = 1; /* non-zero request_token (offset 2..5, COUNTER_MGMT) */
    TestMsg.Sequence      = 22;

    UPLINK_APP_Data.CfsHealthReceived = 1U;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_COUNTER_MGMT);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardCounterMgmtCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastRouteTarget, UPLINK_APP_ROUTE_COUNTER_MGMT);
}

void Test_UPLINK_APP_ProcessUplink_CounterMgmtForwardFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_COUNTER_MGMT;
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 6;
    TestMsg.Payload[2]    = 1;
    TestMsg.Sequence      = 23;

    UPLINK_APP_Data.CfsHealthReceived = 1U;

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_COUNTER_MGMT);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardCounterMgmtCommand), false);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_COUNTER);
}

void Test_UPLINK_APP_ProcessUplink_ViewpointAccept(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_VIEWPOINT;
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 30;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 31;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 32;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 40;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 41;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 6;
    TestMsg.Payload[2]    = 1; /* non-zero request_token (offset 2..5, MODE) */
    TestMsg.Sequence      = 50;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 6;
    TestMsg.Payload[2]    = 1; /* non-zero request_token (offset 2..5, MODE) */
    TestMsg.Sequence      = 51;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(1);
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 60;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(1);
    TestMsg.PayloadLength = 3;
    TestMsg.Sequence      = 61;

    UPLINK_APP_Data.CfsHealthReceived = 1U; /* fail-safe boot 게이트 통과 (§18.10.4) */

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
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
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
    TestMsg.Flags         = TEST_AUTH_LEVEL(1);
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

/* §18.10.4: 원래 CommandClass=DIAGNOSTIC로 작성돼 있었으나, DIAGNOSTIC은 FAILED에서도
 * 허용되는 클래스라 이 테스트의 의도("차단됨" 검증)와 반대다 — BlockedRecovery와
 * 동일하게 CONFIG로 교체 (실제로 FAILED에서 차단되는 클래스). */
void Test_UPLINK_APP_ProcessUplink_BlockedFailed(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
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
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 8;
    TestMsg.Payload[4]    = 1; /* non-zero request_token */
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
    TestMsg.Flags         = TEST_AUTH_LEVEL(3);
    TestMsg.PayloadLength = 8;
    TestMsg.Payload[4]    = 1; /* non-zero request_token */
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

/* §18.10.4: 테스트명/기대값이 "fail-open"(health 수신 전엔 통과)이던 시절 그대로였으나,
 * 커밋 1112351에서 정책이 의도적으로 "fail-closed"(health 수신 전엔 항상 차단, fail-safe
 * boot)로 뒤집혔다(uplink_app_cmds.c의 `if (!CfsHealthReceived)` 및 그 주석 참조).
 * 이 UT 스위트가 그 이후 한 번도 실행되지 않아 반영되지 않고 있었다 — 프로덕션 동작은
 * 의도된 대로이므로, 테스트 기대값만 현재 정책(차단)에 맞게 정정한다. */
void Test_UPLINK_APP_ProcessUplink_BlockedBeforeHealth(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Flags         = TEST_AUTH_LEVEL(2);
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 80;

    /* CfsHealthReceived remains 0 (memset above) */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_REJECT_STATE);
}

/* §18.10.4: 이 스위트 전체를 막고 있던 두 결함(CfsHealthReceived fail-closed 미반영,
 * §18.11.1 인증레벨 Flags 미설정)을 수정해 하네스가 정상화됐으므로, 원래 제외했던
 * "성공(ROUTED)" 기대 테스트 2종을 다시 추가한다. */
void Test_UPLINK_APP_ProcessUplink_ForceFlagBypassesDegradedBlock(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Flags         = TEST_AUTH_LEVEL(2) | UPLINK_APP_FORCE_FLAG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 90;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 1U; /* DEGRADED */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

void Test_UPLINK_APP_ProcessUplink_ForceFlagNoOpWhenNotBlocked(void)
{
    UPLINK_APP_ProcessUplinkCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.Version       = UPLINK_APP_PROTOCOL_VERSION;
    TestMsg.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    TestMsg.Flags         = TEST_AUTH_LEVEL(2) | UPLINK_APP_FORCE_FLAG;
    TestMsg.PayloadLength = 4;
    TestMsg.Sequence      = 92;

    UPLINK_APP_Data.CfsHealthReceived = 1U;
    UPLINK_APP_Data.CfsHealthState    = 0U; /* NOMINAL — 애초에 안 막힘 */

    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ValidateProxyCommand), true);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ResolveRouteTarget), UPLINK_APP_ROUTE_CORE);
    UT_SetDefaultReturnValue(UT_KEY(UPLINK_APP_ForwardConfigCommand), true);

    UPLINK_APP_ProcessUplink(&TestMsg);

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
}

/* FORCE_FLAG 없이 차단 유지되는지(음성 대조): */
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
    ADD_TEST(UPLINK_APP_ProcessUplink_DuplicateRetransmit);
    ADD_TEST(UPLINK_APP_ProcessUplink_AcceptedAfterSequenceWraparound);
    ADD_TEST(UPLINK_APP_ProcessUplink_Reject);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteMiss);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteUpdate);
    ADD_TEST(UPLINK_APP_ProcessUplink_RouteReject);
    ADD_TEST(UPLINK_APP_ProcessUplink_RoutePublishFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_RecoveryAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_RecoveryForwardFail);
    ADD_TEST(UPLINK_APP_ProcessUplink_RetxIdxBitsDoNotAffectAcceptance);
    ADD_TEST(UPLINK_APP_ProcessUplink_CounterMgmtAccept);
    ADD_TEST(UPLINK_APP_ProcessUplink_CounterMgmtForwardFail);
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
    ADD_TEST(UPLINK_APP_ProcessUplink_BlockedBeforeHealth);
    ADD_TEST(UPLINK_APP_ProcessUplink_ForceFlagBypassesDegradedBlock);
    ADD_TEST(UPLINK_APP_ProcessUplink_ForceFlagNoOpWhenNotBlocked);
    ADD_TEST(UPLINK_APP_ProcessUplink_NoForceFlagStillBlockedInDegraded);
    ADD_TEST(UPLINK_APP_ProcessExecResult_MatchesLastAccepted_OK);
    ADD_TEST(UPLINK_APP_ProcessExecResult_MatchesLastAccepted_Failed);
    ADD_TEST(UPLINK_APP_ProcessExecResult_StaleSequenceIgnored);
}

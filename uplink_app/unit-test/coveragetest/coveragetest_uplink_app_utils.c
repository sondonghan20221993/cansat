/************************************************************************
 * Coverage tests for uplink_app_utils.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"
#include <math.h>

void Test_UPLINK_APP_ValidateProxyCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_Result_t           Result;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Version       = UPLINK_APP_PROTOCOL_VERSION;
    Cmd.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    Cmd.PayloadLength = 1;

    UtAssert_BOOL_TRUE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_ACCEPT);

    Cmd.Version = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_VERSION);

    Cmd.Version      = UPLINK_APP_PROTOCOL_VERSION;
    Cmd.CommandClass = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_CLASS);

    Cmd.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    Cmd.PayloadLength = 0;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_LENGTH);

    Cmd.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    Cmd.PayloadLength = UPLINK_APP_MAX_PAYLOAD_LENGTH + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_LENGTH);
}

void Test_UPLINK_APP_VerifyCmdLength_Impl(void)
{
    UPLINK_APP_NoopCmd_t TestMsg;
    size_t               MsgSize;

    memset(&TestMsg, 0, sizeof(TestMsg));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_TRUE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_FALSE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_UPLINK_APP_ParseRouteUpdatePayload(void)
{
    UPLINK_APP_ProcessUplinkCmd_t    Cmd;
    UPLINK_APP_RouteUpdatePayload_t  Payload;
    UPLINK_APP_RouteUpdatePayload_t *PayloadSrc;

    memset(&Cmd, 0, sizeof(Cmd));
    PayloadSrc = (UPLINK_APP_RouteUpdatePayload_t *)Cmd.Payload;
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION;
    PayloadSrc->RouteVersion  = 1;
    PayloadSrc->WaypointCount = 2;
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
    PayloadSrc->Waypoints[1].X = 5.0f;
    PayloadSrc->Waypoints[1].Y = -12.0f;
    PayloadSrc->Waypoints[1].Z = 4.0f;
    Cmd.PayloadLength = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));

    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
    UtAssert_INT32_EQ(Payload.RouteType, UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION);
    UtAssert_INT32_EQ(Payload.WaypointCount, 2);

    PayloadSrc->Waypoints[1].Z = 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].Z = 4.0f;
    PayloadSrc->RouteType = UPLINK_APP_ROUTE_SEGMENT_NONE;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION;
    PayloadSrc->WaypointCount = 0;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = INFINITY;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[1].X = 0.1f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].X = 100.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 4.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
}

void Test_UPLINK_APP_ResolveRouteTarget(void)
{
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_CONFIG), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_ROUTE_UPDATE), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_VIEWPOINT), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_RECOVERY), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_MODE), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_DIAGNOSTIC), UPLINK_APP_ROUTE_DOWNLINK);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(99), UPLINK_APP_ROUTE_NONE);
}

void Test_UPLINK_APP_ReportHousekeeping(void)
{
    UPLINK_APP_Data.CmdCounter       = 2;
    UPLINK_APP_Data.ErrCounter       = 1;
    UPLINK_APP_Data.PublishCount     = 3;
    UPLINK_APP_Data.LastPublishTimeMs = 44;

    UPLINK_APP_ReportHousekeeping();
}

void Test_UPLINK_APP_UpdateStatusTelemetry(void)
{
    UPLINK_APP_Data.SequenceCounter     = 4;
    UPLINK_APP_Data.LastRxTimeMs        = 100;
    UPLINK_APP_Data.AcceptedCount       = 2;
    UPLINK_APP_Data.RejectedCount       = 1;
    UPLINK_APP_Data.RoutingFailureCount = 3;
    UPLINK_APP_Data.LastCommandCode     = UPLINK_APP_CLASS_ROUTE_UPDATE;
    UPLINK_APP_Data.LastCommandResult   = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LinkState           = UPLINK_APP_LINK_NOMINAL;
    UPLINK_APP_Data.LastRouteTarget     = UPLINK_APP_ROUTE_CORE;

    UPLINK_APP_UpdateStatusTelemetry(555);

    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.Seq, 5);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.TimestampMs, 555);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRxTimestampMs, 100);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.AcceptedCount, 2);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RoutingFailureCount, 3);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandCode, UPLINK_APP_CLASS_ROUTE_UPDATE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LinkState, UPLINK_APP_LINK_NOMINAL);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_ValidateProxyCommand);
    ADD_TEST(UPLINK_APP_VerifyCmdLength_Impl);
    ADD_TEST(UPLINK_APP_ParseRouteUpdatePayload);
    ADD_TEST(UPLINK_APP_ResolveRouteTarget);
    ADD_TEST(UPLINK_APP_ReportHousekeeping);
    ADD_TEST(UPLINK_APP_UpdateStatusTelemetry);
}

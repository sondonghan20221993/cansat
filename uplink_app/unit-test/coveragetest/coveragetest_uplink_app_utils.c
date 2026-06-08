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
    Cmd.Checksum      = UPLINK_APP_ComputeProxyCrc(&Cmd);

    UtAssert_BOOL_TRUE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_ACCEPT);

    /* bad checksum — all other fields valid */
    Cmd.Checksum ^= 0x5A5AU;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_CHECKSUM);
    Cmd.Checksum = UPLINK_APP_ComputeProxyCrc(&Cmd);

    /* bad version — short-circuits before CRC check */
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
    uint32                           Index;

    memset(&Cmd, 0, sizeof(Cmd));
    PayloadSrc = (UPLINK_APP_RouteUpdatePayload_t *)Cmd.Payload;
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION;
    PayloadSrc->RouteVersion  = 1;
    PayloadSrc->WaypointCount = 2;
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
    PayloadSrc->Waypoints[1].X = 2.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
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
    PayloadSrc->Waypoints[1].X = 1.99f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].X = 2.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].X = 2.01f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS;
    for (Index = 0; Index < PayloadSrc->WaypointCount; ++Index)
    {
        PayloadSrc->Waypoints[Index].X = (float)(Index * 2U);
        PayloadSrc->Waypoints[Index].Y = -10.0f;
        PayloadSrc->Waypoints[Index].Z = 3.0f;
    }
    Cmd.PayloadLength = (uint8)(4U + ((size_t)PayloadSrc->WaypointCount * sizeof(UPLINK_APP_Waypoint_t)));
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength         = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
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
    UPLINK_APP_Data.LastRxSequence      = 7;
    UPLINK_APP_Data.LastCommandResult   = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LinkState           = UPLINK_APP_LINK_NOMINAL;
    UPLINK_APP_Data.Valid               = 1;
    UPLINK_APP_Data.ActiveTransportId   = 0;
    UPLINK_APP_Data.LastRouteTarget     = UPLINK_APP_ROUTE_CORE;
    UPLINK_APP_Data.ConfigPendingState  = UPLINK_APP_CONFIG_IDLE;
    UPLINK_APP_Data.LastConfigResult    = 0;
    UPLINK_APP_Data.LastRollbackReason  = 0;

    UPLINK_APP_UpdateStatusTelemetry(555);

    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.Seq, 5);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.TimestampMs, 555);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRxTimestampMs, 100);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.AcceptedCount, 2);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RoutingFailureCount, 3);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandCode, UPLINK_APP_CLASS_ROUTE_UPDATE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandSequence, 7);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LinkState, UPLINK_APP_LINK_NOMINAL);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.Valid, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.ActiveTransportId, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.ConfigPendingState, UPLINK_APP_CONFIG_IDLE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastConfigResult, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRollbackReason, 0);
}

void Test_UPLINK_APP_LoadState_NoFile(void)
{
    /* File does not exist — LoadState must return silently without modifying state */
    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);
}

void Test_UPLINK_APP_SaveState_NoDir(void)
{
    /* /cf/ does not exist in test env — SaveState must return silently */
    UPLINK_APP_Data.LastAcceptedSequence = 99;
    UPLINK_APP_SaveState();
    /* No crash and state unchanged */
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 99);
}

void Test_UPLINK_APP_ForwardModeCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Sequence      = 50;
    Cmd.PayloadLength = 3;
    Cmd.Payload[0]    = 0x01;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardModeCommand(&Cmd));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardModeCommand(&Cmd));

    /* zero-length payload path */
    Cmd.PayloadLength = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardModeCommand(&Cmd));
}

void Test_UPLINK_APP_ForwardDiagnosticCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Sequence      = 60;
    Cmd.PayloadLength = 3;
    Cmd.Payload[0]    = 0x02;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));

    Cmd.PayloadLength = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));
}

void Test_UPLINK_APP_ParseViewpointPayload(void)
{
    UPLINK_APP_ProcessUplinkCmd_t  Cmd;
    UPLINK_APP_ViewpointPayload_t  Payload;
    UPLINK_APP_ViewpointPayload_t *Src;

    memset(&Cmd, 0, sizeof(Cmd));
    Src = (UPLINK_APP_ViewpointPayload_t *)Cmd.Payload;

    Cmd.PayloadLength       = (uint8)sizeof(UPLINK_APP_ViewpointPayload_t);
    Src->ViewpointType      = 0;
    Src->ViewpointVersion   = UPLINK_APP_VIEWPOINT_VERSION;
    Src->PositionFrame      = 0;
    Src->X                  = 0.0f;
    Src->Y                  = 0.0f;
    Src->Z                  = 4.0f;
    Src->Yaw                = 0.0f;
    Src->Pitch              = 0.0f;
    Src->HoldTimeMs         = 0;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));

    /* wrong payload size */
    Cmd.PayloadLength = (uint8)(sizeof(UPLINK_APP_ViewpointPayload_t) - 1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Cmd.PayloadLength = (uint8)sizeof(UPLINK_APP_ViewpointPayload_t);

    /* bad version */
    Src->ViewpointVersion = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->ViewpointVersion = UPLINK_APP_VIEWPOINT_VERSION;

    /* bad type */
    Src->ViewpointType = UPLINK_APP_VIEWPOINT_MAX_TYPE + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->ViewpointType = 0;

    /* bad frame */
    Src->PositionFrame = UPLINK_APP_VIEWPOINT_MAX_FRAME + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->PositionFrame = 0;

    /* X out of range / non-finite */
    Src->X = UPLINK_APP_VIEWPOINT_MAX_X_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = UPLINK_APP_VIEWPOINT_MIN_X_M - 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = INFINITY;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = 0.0f;

    /* Y out of range */
    Src->Y = UPLINK_APP_VIEWPOINT_MAX_Y_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Y = 0.0f;

    /* Z out of range */
    Src->Z = UPLINK_APP_VIEWPOINT_MAX_ALT_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Z = UPLINK_APP_VIEWPOINT_MIN_ALT_M - 0.5f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Z = 4.0f;

    /* Yaw out of range */
    Src->Yaw = UPLINK_APP_VIEWPOINT_MAX_YAW + 0.1f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Yaw = -(UPLINK_APP_VIEWPOINT_MAX_YAW + 0.1f);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Yaw = 0.0f;

    /* Pitch out of range */
    Src->Pitch = UPLINK_APP_VIEWPOINT_MAX_PITCH + 0.1f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Pitch = 0.0f;

    /* HoldTimeMs too large */
    Src->HoldTimeMs = UPLINK_APP_VIEWPOINT_MAX_HOLD_MS + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->HoldTimeMs = 0;

    /* boundary valid: max type and max hold */
    Src->ViewpointType = UPLINK_APP_VIEWPOINT_MAX_TYPE;
    Src->HoldTimeMs    = UPLINK_APP_VIEWPOINT_MAX_HOLD_MS;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
}

void Test_UPLINK_APP_ForwardViewpointCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_ViewpointPayload_t Payload;

    memset(&Cmd, 0, sizeof(Cmd));
    memset(&Payload, 0, sizeof(Payload));
    Cmd.Sequence          = 70;
    Payload.ViewpointType = 0;
    Payload.PositionFrame = 0;
    Payload.X             = 1.0f;
    Payload.Y             = -5.0f;
    Payload.Z             = 3.0f;
    Payload.Yaw           = 0.0f;
    Payload.Pitch         = 0.0f;
    Payload.HoldTimeMs    = 1000;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardViewpointCommand(&Cmd, &Payload));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardViewpointCommand(&Cmd, &Payload));
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_ValidateProxyCommand);
    ADD_TEST(UPLINK_APP_VerifyCmdLength_Impl);
    ADD_TEST(UPLINK_APP_ParseRouteUpdatePayload);
    ADD_TEST(UPLINK_APP_ResolveRouteTarget);
    ADD_TEST(UPLINK_APP_ReportHousekeeping);
    ADD_TEST(UPLINK_APP_UpdateStatusTelemetry);
    ADD_TEST(UPLINK_APP_LoadState_NoFile);
    ADD_TEST(UPLINK_APP_SaveState_NoDir);
    ADD_TEST(UPLINK_APP_ForwardModeCommand);
    ADD_TEST(UPLINK_APP_ForwardDiagnosticCommand);
    ADD_TEST(UPLINK_APP_ParseViewpointPayload);
    ADD_TEST(UPLINK_APP_ForwardViewpointCommand);
}

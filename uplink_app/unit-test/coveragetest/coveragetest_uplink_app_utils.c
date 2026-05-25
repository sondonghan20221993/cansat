/************************************************************************
 * Coverage tests for uplink_app_utils.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

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
}

void Test_UPLINK_APP_ResolveRouteTarget(void)
{
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_CONFIG), UPLINK_APP_ROUTE_CORE);
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

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_ValidateProxyCommand);
    ADD_TEST(UPLINK_APP_ParseRouteUpdatePayload);
    ADD_TEST(UPLINK_APP_ResolveRouteTarget);
    ADD_TEST(UPLINK_APP_ReportHousekeeping);
}

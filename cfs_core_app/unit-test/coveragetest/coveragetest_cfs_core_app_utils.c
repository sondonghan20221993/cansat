/************************************************************************
 * Coverage tests for cfs_core_app_utils.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_ReportHousekeeping(void)
{
    CFS_CORE_APP_Data.CmdCounter       = 2;
    CFS_CORE_APP_Data.ErrCounter       = 1;
    CFS_CORE_APP_Data.PublishCount     = 3;
    CFS_CORE_APP_Data.LastPublishTimeMs = 44;

    CFS_CORE_APP_ReportHousekeeping();
}

void Test_CFS_CORE_APP_UpdateHealth_Nominal(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid     = 1;
    CFS_CORE_APP_Data.LocalState.Received     = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs  = 4900;
    CFS_CORE_APP_Data.LocalState.Valid        = 1;
    CFS_CORE_APP_Data.GpsState.Received       = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs    = 4800;
    CFS_CORE_APP_Data.GpsState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received       = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs    = 4900;
    CFS_CORE_APP_Data.EkfState.Valid          = 1;
    CFS_CORE_APP_Data.BridgeState.Received    = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_NONE);
}

void Test_CFS_CORE_APP_UpdateHealth_Recovery(void)
{
    uint32 NowMs = 10000;

    memset(&CFS_CORE_APP_Data.SystemHealthTlm, 0, sizeof(CFS_CORE_APP_Data.SystemHealthTlm));
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
}

void Test_CFS_CORE_APP_ProcessStateMessage_RouteUpdate(void)
{
    CFE_SB_Buffer_t              Buffer;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(&Buffer, 0, sizeof(Buffer));
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)&Buffer.Msg;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    RouteMsg->TimestampMs   = 1234;
    RouteMsg->SourceSequence = 55;
    RouteMsg->RouteType     = 1;
    RouteMsg->RouteVersion  = 2;
    RouteMsg->WaypointCount = 2;
    RouteMsg->Waypoints[0].X = 1.0f;
    RouteMsg->Waypoints[0].Y = 2.0f;
    RouteMsg->Waypoints[0].Z = 3.0f;
    RouteMsg->Waypoints[1].X = 4.0f;
    RouteMsg->Waypoints[1].Y = 5.0f;
    RouteMsg->Waypoints[1].Z = 4.0f;

    CFS_CORE_APP_ProcessStateMessage(&Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.MissionRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.RouteVersion, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 2);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_ReportHousekeeping);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Nominal);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Recovery);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_RouteUpdate);
}

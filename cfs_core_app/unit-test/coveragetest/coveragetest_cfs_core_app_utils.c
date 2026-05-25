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

void Test_CFS_CORE_APP_VerifyCmdLength_Impl(void)
{
    CFS_CORE_APP_NoopCmd_t TestMsg;
    size_t                 MsgSize;

    memset(&TestMsg, 0, sizeof(TestMsg));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
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

void Test_CFS_CORE_APP_UpdateHealth_GpsStale(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Stale              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 1;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_GPS_STALE);
}

void Test_CFS_CORE_APP_UpdateHealth_EkfInvalid(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 0;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_EKF_INVALID);
}

void Test_CFS_CORE_APP_UpdateHealth_LocalTimeout(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 0;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 1;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_EKF_INVALID);
}

void Test_CFS_CORE_APP_UpdateHealth_AttitudeTimeout(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 0;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 1;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_EKF_INVALID);
}

void Test_CFS_CORE_APP_ProcessStateMessage_RouteUpdate(void)
{
    uint8                        Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t             *Buffer;
    CFE_SB_MsgId_t               MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
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

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.MissionRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.RouteVersion, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 2);
}

void Test_CFS_CORE_APP_ProcessStateMessage_LandingRouteUpdate(void)
{
    uint8                          Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t               *Buffer;
    CFE_SB_MsgId_t                 MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->TimestampMs    = 4321;
    RouteMsg->SourceSequence = 66;
    RouteMsg->RouteType      = CFS_CORE_APP_ROUTE_SEGMENT_LANDING;
    RouteMsg->RouteVersion   = 3;
    RouteMsg->WaypointCount  = 1;
    RouteMsg->Waypoints[0].X = 9.0f;
    RouteMsg->Waypoints[0].Y = 8.0f;
    RouteMsg->Waypoints[0].Z = 3.0f;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.LandingRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LandingRoute.RouteVersion, 3);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LandingRoute.WaypointCount, 1);
}

void Test_CFS_CORE_APP_ProcessStateMessage_BridgeHk(void)
{
    typedef struct
    {
        CFE_MSG_TelemetryHeader_t TelemetryHeader;
        uint8                     CommandCounter;
        uint8                     CommandErrorCounter;
        uint8                     LinkState;
        uint8                     LastErrorCode;
        uint32                    BytesReceived;
        uint32                    ReconnectAttemptCount;
        uint32                    ParseErrorCount;
        uint32                    LastRxTimestampMs;
    } TEST_CFS_CORE_APP_BridgeHk_t;

    uint8                      Storage[sizeof(TEST_CFS_CORE_APP_BridgeHk_t)];
    CFE_SB_Buffer_t           *Buffer;
    CFE_SB_MsgId_t             MsgId;
    TEST_CFS_CORE_APP_BridgeHk_t *BridgeMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer    = (CFE_SB_Buffer_t *)Storage;
    BridgeMsg = (TEST_CFS_CORE_APP_BridgeHk_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(BridgeMsg->TelemetryHeader), CFE_SB_ValueToMsgId(CFS_CORE_APP_BRIDGE_HK_MID_VALUE),
                 sizeof(*BridgeMsg));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_BRIDGE_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    BridgeMsg->CommandCounter   = 1;
    BridgeMsg->CommandErrorCounter = 0;
    BridgeMsg->LastRxTimestampMs = 777;
    BridgeMsg->LinkState         = 2;
    BridgeMsg->LastErrorCode     = 9;
    BridgeMsg->BytesReceived     = 100;
    BridgeMsg->ReconnectAttemptCount = 1;
    BridgeMsg->ParseErrorCount   = 0;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.BridgeState.Received);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs, 777);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LinkState, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LastErrorCode, 9);
}

void Test_CFS_CORE_APP_ServicePrototype(void)
{
    CFS_CORE_APP_Data.LastPublishTimeMs = 1000;
    CFS_CORE_APP_Data.PublishCount      = 0;

    CFS_CORE_APP_ServicePrototype();
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_ReportHousekeeping);
    ADD_TEST(CFS_CORE_APP_VerifyCmdLength_Impl);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Nominal);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Recovery);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_GpsStale);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_EkfInvalid);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_LocalTimeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_AttitudeTimeout);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_RouteUpdate);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_LandingRouteUpdate);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_BridgeHk);
    ADD_TEST(CFS_CORE_APP_ServicePrototype);
}

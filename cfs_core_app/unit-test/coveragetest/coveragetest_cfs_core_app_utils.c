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
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

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
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* GPS stale은 헬스에 영향 없음 (보고 전용) — 명세 §12.5 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_NONE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.Stale, 1);
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_LOCAL_TIMEOUT);
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
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
    uint8                      Storage[sizeof(BRIDGE_HK_TLM_t)];
    CFE_SB_Buffer_t           *Buffer;
    CFE_SB_MsgId_t             MsgId;
    BRIDGE_HK_TLM_t           *BridgeMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer    = (CFE_SB_Buffer_t *)Storage;
    BridgeMsg = (BRIDGE_HK_TLM_t *)Storage;
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
    BridgeMsg->NonFiniteValueCount = 0;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.BridgeState.Received);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs, 777);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LinkState, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeState.LastErrorCode, 9);
}

void Test_CFS_CORE_APP_UpdateHealth_NominalStabilization(void)
{
    uint32 NowMs = 20000;

    /* Fully healthy setup */
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    /* Simulate LastHealthState = DEGRADED (coming from a fault) */
    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;

    /* First call after fault clears: still DEGRADED (stabilizing) */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_NONE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NominalEligibleSince,        (int32)NowMs);

    /* 5 s later: still stabilizing */
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs + 5000 - 100;
    CFS_CORE_APP_UpdateHealth(NowMs + 5000, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);

    /* 10 s later: stable enough, transition to NOMINAL */
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs + 10001 - 100;
    CFS_CORE_APP_UpdateHealth(NowMs + 10001, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NominalEligibleSince,        0);
}

void Test_CFS_CORE_APP_UpdateHealth_InputStatus(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.AttitudeState.Stale         = 0;
    CFS_CORE_APP_Data.AttitudeState.ErrorCode     = 0;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Stale            = 0;
    CFS_CORE_APP_Data.LocalState.ErrorCode        = 7;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 0;
    CFS_CORE_APP_Data.GpsState.Stale              = 1;
    CFS_CORE_APP_Data.GpsState.ErrorCode          = 3;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Stale              = 0;
    CFS_CORE_APP_Data.EkfState.ErrorCode          = 0;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;
    CFS_CORE_APP_Data.BridgeState.LinkState       = 2;
    CFS_CORE_APP_Data.BridgeState.LastErrorCode   = 5;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.AttitudeStatus.Valid,    1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.AttitudeStatus.Stale,    0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.AttitudeStatus.TimedOut, 0);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.LocalStatus.Valid,     1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.LocalStatus.ErrorCode, 7);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.LocalStatus.TimedOut,  0);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.Valid,     0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.Stale,     1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.ErrorCode, 3);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.TimedOut,  1);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.EkfStatus.Valid,    1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.EkfStatus.TimedOut, 0);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.BridgeStatus.LinkState, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.BridgeStatus.ErrorCode, 5);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.BridgeStatus.TimedOut,  0);
}

void Test_CFS_CORE_APP_UpdateHealth_HealthTransition(void)
{
    UT_CheckEvent_t EventTest;
    uint32          NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = 4800;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = 4900;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    /* NOMINAL==0 equals initial LastHealthState==0: no transition event */
    UT_CHECKEVENT_SETUP(&EventTest, CFS_CORE_APP_HEALTH_TRANSITION_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(EventTest.MatchCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, CFS_CORE_APP_HEALTH_NOMINAL);

    /* NOMINAL->DEGRADED: transition event fires once (EKF invalid) */
    CFS_CORE_APP_Data.EkfState.Valid = 0;
    UT_CHECKEVENT_SETUP(&EventTest, CFS_CORE_APP_HEALTH_TRANSITION_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs + 100, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(EventTest.MatchCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, CFS_CORE_APP_HEALTH_DEGRADED);

    /* Still DEGRADED: no additional transition event */
    UT_CHECKEVENT_SETUP(&EventTest, CFS_CORE_APP_HEALTH_TRANSITION_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs + 200, true);
    UtAssert_INT32_EQ(EventTest.MatchCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, CFS_CORE_APP_HEALTH_DEGRADED);
}

void Test_CFS_CORE_APP_UpdateHealth_LocalInvalid(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 0; /* invalid, fresh timestamp */
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_LOCAL_TIMEOUT);
}

void Test_CFS_CORE_APP_UpdateHealth_LocalStale(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Stale            = 1; /* stale, fresh timestamp */
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_LOCAL_TIMEOUT);
}

void Test_CFS_CORE_APP_UpdateHealth_AttitudeInvalid(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 0; /* invalid, fresh timestamp */
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
}

void Test_CFS_CORE_APP_UpdateHealth_AttitudeStale(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received      = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.AttitudeState.Stale         = 1; /* stale, fresh timestamp */
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
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
}

/* bridge timeout 30s 경과 → FAILED 에스컬레이션 */
/* -----------------------------------------------------------------------
 * 타임스탬프 유효성 검사 테스트
 * ----------------------------------------------------------------------- */

/* 정상 타임스탬프 → 캐시 갱신 */
void Test_CFS_CORE_APP_TimestampCheck_Normal(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    uint32                         NowMs = 5000;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = NowMs - 100; /* 100ms 전 → 정상 */
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received          = false;
    CFS_CORE_APP_Data.TimestampRejectedCount          = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount,  0);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.AttitudeState.Received);
}

/* 미래 타임스탬프 (NowMs + TOLERANCE + 100000ms) → 반드시 거부 */
void Test_CFS_CORE_APP_TimestampCheck_FutureTooFar(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;
    UT_CheckEvent_t                Evt;

    /* CFE_TIME_GetTime가 Seconds=10을 반환하도록 설정 → NowMs = 10000 */
    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 10U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    /* NowMs = 10000, TOLERANCE = 5000 → 15001 이상은 거부 */
    Msg.TimestampMs = 10000U + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS + 1U; /* 15001 */
    Msg.Seq         = 10;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received     = false;
    CFS_CORE_APP_Data.TimestampRejectedCount     = 0;

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_TIMESTAMP_ERR_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount,    1);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.AttitudeState.Received); /* 캐시 불변 */
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* 허용 한계 경계: NowMs + TOLERANCE 정확히 → 허용 */
void Test_CFS_CORE_APP_TimestampCheck_FutureBoundary(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;

    /* NowMs = 10000 */
    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 10U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    /* NowMs=10000, TOLERANCE=5000 → ts=15000 정확히 경계 → 허용 */
    Msg.TimestampMs = 10000U + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS; /* 15000 */
    Msg.Seq         = 20;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received     = false;
    CFS_CORE_APP_Data.TimestampRejectedCount     = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount,  0);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.AttitudeState.Received);
}

/* -----------------------------------------------------------------------
 * 타임스탬프 체크 추가 케이스
 * ----------------------------------------------------------------------- */

/* GPS MID에도 타임스탬프 검사 적용 */
void Test_CFS_CORE_APP_TimestampCheck_GPS_Rejected(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 10U; /* NowMs = 10000 */
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 10000U + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS + 1U;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE);

    CFS_CORE_APP_Data.GpsState.Received      = false;
    CFS_CORE_APP_Data.TimestampRejectedCount = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount,  1);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.GpsState.Received);
}

/* EKF MID에도 타임스탬프 검사 적용 */
void Test_CFS_CORE_APP_TimestampCheck_EKF_Rejected(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 10U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 10000U + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS + 1U;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE);

    CFS_CORE_APP_Data.EkfState.Received      = false;
    CFS_CORE_APP_Data.TimestampRejectedCount = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount,  1);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.EkfState.Received);
}

/* 타임스탬프 거부가 시퀀스 검사보다 먼저 실행됨 확인 */
void Test_CFS_CORE_APP_TimestampCheck_BeforeSeqCheck(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 10U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 10000U + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS + 1U;
    Msg.Seq         = 5; /* 이전 seq=10보다 낮은 역행 seq */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.TimestampRejectedCount    = 0;
    CFS_CORE_APP_Data.SeqRejectedCount          = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    /* 타임스탬프 검사가 먼저 실행 → timestamp count만 증가, seq count는 0 유지 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimestampRejectedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,       0);
}

/* ForcePublish=false일 때 주기 미충족이면 게시 안 함 */
void Test_CFS_CORE_APP_UpdateHealth_PeriodicRateLimit(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = 4900;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = 4900;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = 4900;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;

    /* 첫 번째 호출 */
    CFS_CORE_APP_UpdateHealth(NowMs, false);
    uint32 PublishCount1 = CFS_CORE_APP_Data.PublishCount;

    /* 바로 다시 호출 (주기 미충족) → 게시 안 됨 */
    CFS_CORE_APP_UpdateHealth(NowMs + 100, false);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.PublishCount, (int32)PublishCount1);

    /* 1000ms 후 호출 (주기 충족) → 게시 됨 */
    CFS_CORE_APP_UpdateHealth(NowMs + CFS_CORE_APP_PROTOTYPE_PERIOD_MS, false);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.PublishCount, (int32)PublishCount1 + 1);
}

/* ProcessStateMessage GetMsgId 오류 경로 → ErrCounter 증가 */
void Test_CFS_CORE_APP_ProcessStateMessage_GetMsgIdError(void)
{
    CFE_SB_Buffer_t Buffer;

    memset(&Buffer, 0, sizeof(Buffer));
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_GetMsgId), 1, CFE_STATUS_EXTERNAL_RESOURCE_FAIL);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessStateMessage(&Buffer);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
}

/* ReportHousekeeping 필드 복사 확인 */
void Test_CFS_CORE_APP_ReportHousekeeping_Fields(void)
{
    CFS_CORE_APP_Data.CmdCounter                    = 7;
    CFS_CORE_APP_Data.ErrCounter                    = 3;
    CFS_CORE_APP_Data.PublishCount                  = 42;
    CFS_CORE_APP_Data.MissionRoute.WaypointCount    = 4;
    CFS_CORE_APP_Data.LandingRoute.WaypointCount    = 2;
    CFS_CORE_APP_Data.MissionRoute.UpdateCount      = 5;
    CFS_CORE_APP_Data.LandingRoute.UpdateCount      = 1;

    CFS_CORE_APP_ReportHousekeeping();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.CommandCounter,            7);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.CommandErrorCounter,       3);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.PublishCount,             42);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.MissionRouteWaypointCount, 4);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.LandingRouteWaypointCount, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.RouteUpdateCount,          6); /* 5+1 */
}

/* -----------------------------------------------------------------------
 * ProcessConfigCommand 테스트
 * ----------------------------------------------------------------------- */

static uint16 calc_config_checksum(uint8 scope, uint8 version, uint16 param_id,
                                    uint8 value_type, uint8 value_len,
                                    const uint8 *value_bytes)
{
    uint16 sum = 0;
    uint8  i;
    sum += scope;
    sum += version;
    sum += (uint16)(param_id & 0xFFU);
    sum += (uint16)((param_id >> 8U) & 0xFFU);
    sum += value_type;
    sum += value_len;
    for (i = 0; i < value_len; i++) { sum += value_bytes[i]; }
    return sum;
}

static void build_config_msg(CFS_CORE_APP_ConfigCmdTlm_t *Msg,
                              uint8 scope, uint8 version,
                              uint16 param_id, uint32 value)
{
    CFS_CORE_APP_ConfigPayloadHdr_t *Hdr;
    uint8 vbytes[4];
    memset(Msg, 0, sizeof(*Msg));
    Hdr = (CFS_CORE_APP_ConfigPayloadHdr_t *)Msg->Payload;
    Hdr->ConfigScope   = scope;
    Hdr->ConfigVersion = version;
    Hdr->ParameterId   = param_id;
    Hdr->ValueType     = 0;
    Hdr->ValueLength   = (uint8)sizeof(uint32);
    memcpy(Msg->Payload + sizeof(*Hdr), &value, sizeof(value));
    memcpy(vbytes, &value, sizeof(value));
    Hdr->Checksum = calc_config_checksum(scope, version, param_id, 0,
                                         (uint8)sizeof(uint32), vbytes);
    Msg->PayloadLength = (uint8)(sizeof(*Hdr) + sizeof(uint32));
}

/* 정상: AttitudeTimeoutMs 변경 — pending→active 흐름, PreviousConfig 백업 */
void Test_CFS_CORE_APP_ProcessConfig_AttitudeTimeout(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    uint32 OldValue = CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs;

    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 3000U);

    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    /* ActiveConfig에 새 값 반영 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, 3000);
    /* PreviousConfig에 이전 값 백업 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.PreviousConfig.AttitudeTimeoutMs, (int32)OldValue);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_OK);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigPendingState,
                      (int32)CFS_CORE_APP_CONFIG_PENDING_IDLE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigGeneration, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

/* 정상: PublishPeriodMs 변경 */
void Test_CFS_CORE_APP_ProcessConfig_PublishPeriod(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_PUBLISH_PERIOD_MS, 2000U);

    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs, 2000);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_OK);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigPendingState,
                      (int32)CFS_CORE_APP_CONFIG_PENDING_IDLE);
}

/* 잘못된 checksum → REJECTED, ActiveConfig 불변 */
void Test_CFS_CORE_APP_ProcessConfig_BadChecksum(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t      Msg;
    CFS_CORE_APP_ConfigPayloadHdr_t *Hdr;
    uint32 OldVal = CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs;

    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 3000U);

    /* checksum 임의 변조 */
    Hdr = (CFS_CORE_APP_ConfigPayloadHdr_t *)Msg.Payload;
    Hdr->Checksum ^= 0xDEADU;

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_CHECKSUM);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigPendingState,
                      (int32)CFS_CORE_APP_CONFIG_PENDING_REJECTED);
    /* ActiveConfig 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, (int32)OldVal);
}

/* 잘못된 scope → REJECTED, ActiveConfig 불변 */
void Test_CFS_CORE_APP_ProcessConfig_BadScope(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    uint32 OldTimeout = CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs;

    build_config_msg(&Msg, 0xFF, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 3000U);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_SCOPE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigPendingState,
                      (int32)CFS_CORE_APP_CONFIG_PENDING_REJECTED);
    /* ActiveConfig 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, (int32)OldTimeout);
}

/* 잘못된 version → 거부 */
void Test_CFS_CORE_APP_ProcessConfig_BadVersion(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, 0xFF,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 3000U);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_VERSION);
}

/* 알 수 없는 parameter_id → 거부 */
void Test_CFS_CORE_APP_ProcessConfig_BadParam(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     0xFFFF, 3000U);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_PARAM);
}

/* 범위 초과 값 → 거부 */
void Test_CFS_CORE_APP_ProcessConfig_BadValue(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    /* PARAM_MAX_MS 초과 값 */
    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS,
                     CFS_CORE_APP_PARAM_MAX_MS + 1U);

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_VALUE);
}

/* payload 너무 짧음 → 거부 */
void Test_CFS_CORE_APP_ProcessConfig_BadLength(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    memset(&Msg, 0, sizeof(Msg));
    Msg.PayloadLength = 2; /* header(6) 미만 */

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_LENGTH);
}

/* -----------------------------------------------------------------------
 * LoadState / SaveState 테스트
 * ----------------------------------------------------------------------- */

/* 파일 없음 → LoadState 무시, LastHealthState 불변 */
void Test_CFS_CORE_APP_LoadState_NoFile(void)
{
    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);
}

/* 파일 없어도 SaveState 충돌 없음 (/cf/ 미존재 환경) */
void Test_CFS_CORE_APP_SaveState_NoDir(void)
{
    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;

    CFS_CORE_APP_SaveState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_DEGRADED);
}

/* 상태 전이 시 SaveState 자동 호출 확인 */
void Test_CFS_CORE_APP_SaveState_OnTransition(void)
{
    uint32 NowMs = 10000;

    /* 전체 정상 입력 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    /* DEGRADED에서 시작 → NOMINAL로 전이 (즉시, LastHealthState가 NOMINAL이라면) */
    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    /* 전이 없으면 SaveState 미호출 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);

    /* EKF invalid 주입 → DEGRADED 전이 발생 → SaveState 호출됨 (파일 없어도 무시) */
    CFS_CORE_APP_Data.EkfState.Valid = 0;
    CFS_CORE_APP_UpdateHealth(NowMs + 100, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_DEGRADED);
}

/* -----------------------------------------------------------------------
 * Bridge 능동적 재시작 테스트
 * ----------------------------------------------------------------------- */

/* bridge timeout 후 인터벌 경과 시 RestartApp 호출 */
void Test_CFS_CORE_APP_BridgeRestart_FirstAttempt(void)
{
    UT_CheckEvent_t Evt;
    uint32          NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* bridge timed out */
    CFS_CORE_APP_Data.BridgeRestartCount            = 0;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = 0;
    CFS_CORE_APP_Data.RecoveryStartMs               = 0;

    /* CFE_ES_GetAppIDByName 성공으로 설정 */
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_BRIDGE_RESTART_EID, NULL);

    /* 첫 번째 호출: RecoveryStartMs 설정, NextBridgeRestartMs = NowMs+5000 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 아직 인터벌 미경과 → 재시작 안 됨 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 0);
    UtAssert_INT32_EQ(Evt.MatchCount, 0);

    /* 5초 후 → 재시작 실행 */
    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_BRIDGE_RESTART_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs + CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

/* GetAppIDByName 실패 시 재시작 카운터 증가 없음 */
void Test_CFS_CORE_APP_BridgeRestart_GetAppIdFail(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;
    CFS_CORE_APP_Data.BridgeRestartCount            = 0;
    CFS_CORE_APP_Data.RecoveryStartMs               = NowMs - CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS - 1U;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = NowMs - 1U; /* 인터벌 이미 경과 */

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_ES_ERR_NAME_NOT_FOUND);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* GetAppIDByName 실패 → RestartApp 미호출, 카운터 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 0);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
}

/* 최대 재시도 초과 시 더 이상 재시작 안 함 */
void Test_CFS_CORE_APP_BridgeRestart_MaxReached(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;
    CFS_CORE_APP_Data.BridgeRestartCount            = CFS_CORE_APP_BRIDGE_MAX_RESTARTS; /* 이미 최대치 */
    CFS_CORE_APP_Data.RecoveryStartMs               = NowMs - CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS - 1U;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 최대치 초과 → RestartApp 미호출 */
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, (int32)CFS_CORE_APP_BRIDGE_MAX_RESTARTS);
}

/* bridge 복구 시 재시작 카운터 리셋 */
void Test_CFS_CORE_APP_BridgeRestart_ResetOnRecovery(void)
{
    uint32 NowMs = 20000;

    /* bridge 복구 상태 + 모든 입력 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;
    CFS_CORE_APP_Data.LastHealthState                = CFS_CORE_APP_HEALTH_NOMINAL;

    /* 재시작 카운터가 남아 있었음 */
    CFS_CORE_APP_Data.BridgeRestartCount  = 2;
    CFS_CORE_APP_Data.NextBridgeRestartMs = NowMs + 1000;
    CFS_CORE_APP_Data.RecoveryStartMs     = NowMs - 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 복구 후 카운터 리셋 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount,  0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NextBridgeRestartMs, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RecoveryStartMs,     0);
}

/* -----------------------------------------------------------------------
 * Uplink/Lora 능동적 재시작 테스트 (Bridge와 동일 패턴, 2026-07-13 추가)
 * ----------------------------------------------------------------------- */

/* Bridge/Ekf/Local/Attitude를 NowMs 기준 "정상"으로 맞춰, 우선순위 높은 다른
 * TimedOut 분기가 먼저 걸리지 않게 한다 (전역 CFS_CORE_APP_Data가 테스트 간
 * 공유되므로 이전 테스트의 leftover 값이 언더플로/오탐을 유발할 수 있음). */
static void SetHealthyExceptUplinkLora(uint32 NowMs)
{
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
}

/* uplink timeout 후 인터벌 경과 시 RestartApp 호출 */
void Test_CFS_CORE_APP_UplinkRestart_FirstAttempt(void)
{
    UT_CheckEvent_t Evt;
    uint32          NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received  = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 1000; /* uplink timed out */
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = NowMs;
    CFS_CORE_APP_Data.UplinkRestartCount        = 0;
    CFS_CORE_APP_Data.NextUplinkRestartMs       = 0;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_UPLINK_RESTART_EID, NULL);

    /* 첫 번째 호출: NextUplinkRestartMs = NowMs+5000 설정, 아직 재시작 안 함 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 0);
    UtAssert_INT32_EQ(Evt.MatchCount, 0);

    /* 5초 후 → 재시작 실행 (Bridge/Ekf/Local/Attitude도 새 시각 기준으로 갱신) */
    SetHealthyExceptUplinkLora(NowMs + CFS_CORE_APP_UPLINK_RESTART_INTERVAL_MS);
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = NowMs + CFS_CORE_APP_UPLINK_RESTART_INTERVAL_MS;
    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_UPLINK_RESTART_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs + CFS_CORE_APP_UPLINK_RESTART_INTERVAL_MS, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

/* 최대 재시도 초과 시 uplink_app 더 이상 재시작 안 함 */
void Test_CFS_CORE_APP_UplinkRestart_MaxReached(void)
{
    uint32 NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 1000;
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = NowMs;
    CFS_CORE_APP_Data.UplinkRestartCount        = CFS_CORE_APP_UPLINK_MAX_RESTARTS;
    CFS_CORE_APP_Data.NextUplinkRestartMs       = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, (int32)CFS_CORE_APP_UPLINK_MAX_RESTARTS);
}

/* uplink 복구 시 재시작 카운터 리셋 */
void Test_CFS_CORE_APP_UplinkRestart_ResetOnRecovery(void)
{
    uint32 NowMs = 20000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received       = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received         = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs - 100;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_Data.UplinkRestartCount  = 2;
    CFS_CORE_APP_Data.NextUplinkRestartMs = NowMs + 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount,  0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NextUplinkRestartMs, 0);
}

/* lora timeout 후 인터벌 경과 시 RestartApp 호출 */
void Test_CFS_CORE_APP_LoraRestart_FirstAttempt(void)
{
    UT_CheckEvent_t Evt;
    uint32          NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs;
    CFS_CORE_APP_Data.LoraAppState.Received   = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = 1000; /* lora timed out */
    CFS_CORE_APP_Data.LoraRestartCount        = 0;
    CFS_CORE_APP_Data.NextLoraRestartMs       = 0;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_LORA_RESTART_EID, NULL);

    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 0);
    UtAssert_INT32_EQ(Evt.MatchCount, 0);

    SetHealthyExceptUplinkLora(NowMs + CFS_CORE_APP_LORA_RESTART_INTERVAL_MS);
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs + CFS_CORE_APP_LORA_RESTART_INTERVAL_MS;
    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_LORA_RESTART_EID, NULL);
    CFS_CORE_APP_UpdateHealth(NowMs + CFS_CORE_APP_LORA_RESTART_INTERVAL_MS, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

/* 최대 재시도 초과 시 lora_tdm_app 더 이상 재시작 안 함 */
void Test_CFS_CORE_APP_LoraRestart_MaxReached(void)
{
    uint32 NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs;
    CFS_CORE_APP_Data.LoraAppState.Received   = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = 1000;
    CFS_CORE_APP_Data.LoraRestartCount        = CFS_CORE_APP_LORA_MAX_RESTARTS;
    CFS_CORE_APP_Data.NextLoraRestartMs       = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, (int32)CFS_CORE_APP_LORA_MAX_RESTARTS);
}

/* GetAppIDByName 실패 시 lora_tdm_app 재시작 카운터 증가 없음 (Bridge/Uplink와 동일 패턴, 2026-07-20 보강) */
void Test_CFS_CORE_APP_LoraRestart_GetAppIdFail(void)
{
    uint32 NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs;
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = 1000; /* lora timed out */
    CFS_CORE_APP_Data.LoraRestartCount          = 0;
    CFS_CORE_APP_Data.RecoveryStartMs           = NowMs - CFS_CORE_APP_LORA_RESTART_INTERVAL_MS - 1U;
    CFS_CORE_APP_Data.NextLoraRestartMs         = NowMs - 1U; /* 인터벌 이미 경과 */

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_ES_ERR_NAME_NOT_FOUND);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* GetAppIDByName 실패 → RestartApp 미호출, 카운터 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 0);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
}

/* lora_tdm_app 복구 시 재시작 카운터 리셋 (Bridge/Uplink와 동일 패턴, 2026-07-20 보강) */
void Test_CFS_CORE_APP_LoraRestart_ResetOnRecovery(void)
{
    uint32 NowMs = 20000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received       = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received         = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs - 100;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_Data.LoraRestartCount  = 2;
    CFS_CORE_APP_Data.NextLoraRestartMs = NowMs + 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount,  0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NextLoraRestartMs, 0);
}

/* -----------------------------------------------------------------------
 * Spec §19.2 CORE-RUN-004: GPS 타임아웃 (stale 플래그 아닌 timestamp 만료)
 * ----------------------------------------------------------------------- */
void Test_CFS_CORE_APP_UpdateHealth_GPS_Timeout(void)
{
    uint32 NowMs = 10000;

    /* bridge 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 9900;

    /* EKF, Local, Attitude 정상 */
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;

    /* GPS: 이전에 수신됐으나 타임스탬프 만료 (stale 플래그는 0) */
    CFS_CORE_APP_Data.GpsState.Received    = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 5000; /* NowMs - 5000 > 3000ms timeout */
    CFS_CORE_APP_Data.GpsState.Valid       = 1;
    CFS_CORE_APP_Data.GpsState.Stale       = 0; /* stale 플래그 없음 — 순수 timeout */
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* GPS 타임아웃은 헬스에 영향 없음 (보고 전용) — 명세 §12.5 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_NONE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.GpsStatus.TimedOut, 1);
}

/* Spec §19.2 CORE-RUN-009: 복구 후 정상 — RECOVERY에서 신선한 입력 재개 시 NOMINAL 복귀 */
void Test_CFS_CORE_APP_UpdateHealth_RecoveryToNominal(void)
{
    uint32 NowMs = 50000;

    /* 전체 입력 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    /* RECOVERY에서 복구 시작 */
    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_RECOVERY;

    /* 첫 평가: 안정화 구간 진입 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_NONE);

    /* 5초 후: 아직 안정화 중 */
    uint32 T5 = NowMs + 5000;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = T5 - 100;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = T5 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = T5 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = T5 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = T5 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs      = T5 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs        = T5 - 100;
    CFS_CORE_APP_UpdateHealth(T5, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);

    /* 10초 후: NOMINAL 복귀 */
    uint32 T10 = NowMs + 10001;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = T10 - 100;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = T10 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = T10 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = T10 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = T10 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs      = T10 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs        = T10 - 100;
    CFS_CORE_APP_UpdateHealth(T10, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
}

/* -----------------------------------------------------------------------
 * Spec §19.2 CORE-RUN-008: 우선순위 — bridge timeout + GPS stale 동시 발생
 * bridge timeout이 GPS stale보다 높은 우선순위 → RECOVERY
 * ----------------------------------------------------------------------- */
void Test_CFS_CORE_APP_UpdateHealth_Priority_BridgeOverGps(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* 9초 경과 → timeout */

    CFS_CORE_APP_Data.GpsState.Received    = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 9900;
    CFS_CORE_APP_Data.GpsState.Valid       = 1;
    CFS_CORE_APP_Data.GpsState.Stale       = 1; /* GPS stale도 동시 발생 */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received       = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs    = 9900;
    CFS_CORE_APP_Data.LocalState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received         = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs      = 9900;
    CFS_CORE_APP_Data.EkfState.Valid            = 1;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
}

/* Spec §19.2 CORE-RUN-010: 시작 워밍업 — 첫 bridge HK 수신 전 → RECOVERY */
void Test_CFS_CORE_APP_UpdateHealth_Startup_NoBridge(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.BridgeState.Received = false;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);
}

/* Spec §12: EKF(우선순위 2) > Local(우선순위 3) — 둘 다 무효일 때 EKF fault 반환 */
void Test_CFS_CORE_APP_UpdateHealth_Priority_EkfOverLocal(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 9900;

    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 0; /* EKF invalid */

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 0; /* Local도 invalid */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    CFS_CORE_APP_Data.GpsState.Received         = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs      = 9800;
    CFS_CORE_APP_Data.GpsState.Valid            = 1;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_EKF_INVALID);
}

/* Spec §12: Local(우선순위 3) > Attitude(우선순위 4) */
void Test_CFS_CORE_APP_UpdateHealth_Priority_LocalOverAttitude(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 9900;

    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 0; /* Local invalid */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 0; /* Attitude도 invalid */

    CFS_CORE_APP_Data.GpsState.Received    = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 9800;
    CFS_CORE_APP_Data.GpsState.Valid       = 1;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_LOCAL_TIMEOUT);
}

/* Spec §12: Attitude(우선순위 4) > GPS(우선순위 5) */
void Test_CFS_CORE_APP_UpdateHealth_Priority_AttitudeOverGps(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 9900;

    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 0; /* Attitude invalid */

    CFS_CORE_APP_Data.GpsState.Received = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 9800;
    CFS_CORE_APP_Data.GpsState.Valid    = 0; /* GPS도 invalid */

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
}

/* Spec §14.3: 안정화 타이머 중 오류 재발 → 타이머 리셋 */
void Test_CFS_CORE_APP_UpdateHealth_StabilityTimerReset(void)
{
    uint32 NowMs = 20000;

    /* 전체 정상 입력 설정 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;
    CFS_CORE_APP_Data.LastHealthState                = CFS_CORE_APP_HEALTH_DEGRADED;

    /* 안정화 시작 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NominalEligibleSince, (int32)NowMs);

    /* 5초 후 uplink timeout 발생 → 안정화 타이머 리셋 */
    /* UplinkAppState.LastHkRxMs을 갱신하지 않아서 5100ms 경과 → timeout */
    CFS_CORE_APP_Data.GpsState.Stale = 1;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_UpdateHealth(NowMs + 5000, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.NominalEligibleSince, 0); /* 리셋됨 */
}

/* -----------------------------------------------------------------------
 * 시퀀스 중복/역행 감지 테스트
 * ----------------------------------------------------------------------- */

/* 정상: 첫 수신 → 캐시 갱신, seq +1 증가 → 정상 갱신 (갭 없음) */
void Test_CFS_CORE_APP_SeqCheck_Normal(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 10;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received = false;
    CFS_CORE_APP_Data.SeqRejectedCount       = 0;
    CFS_CORE_APP_Data.SeqGapCount            = 0;

    /* 첫 수신 */
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,  0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqGapCount,       0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.Seq, 10);

    /* seq +1 증가 → 갭 없음, 정상 갱신 */
    Msg.Seq         = 11;
    Msg.TimestampMs = 4950;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,  0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqGapCount,       0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.Seq, 11);
}

/* seq 갭 → SeqGapCount 증가, 캐시는 갱신 (거부 아님), DEBUG 이벤트 */
void Test_CFS_CORE_APP_SeqCheck_Gap(void)
{
    UT_CheckEvent_t                Evt;
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 20; /* 이전 seq=10 대비 9개 갭 */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.SeqRejectedCount          = 0;
    CFS_CORE_APP_Data.SeqGapCount               = 0;

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_SEQ_GAP_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,              0); /* 거부 아님 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqGapCount,                   1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.Seq,            20); /* 캐시 갱신됨 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.TimestampMs, 4900); /* 캐시 갱신됨 */
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* 첫 수신 시 seq 갭 체크 없음 */
void Test_CFS_CORE_APP_SeqCheck_Gap_FirstReceive(void)
{
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 100; /* 임의의 높은 seq */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received = false;
    CFS_CORE_APP_Data.SeqGapCount            = 0;

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqGapCount,               0); /* 첫 수신 → 갭 없음 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.Seq,       100);
    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.AttitudeState.Received);
}

/* 중복 seq → 캐시 불변, SeqRejectedCount 증가 */
void Test_CFS_CORE_APP_SeqCheck_Duplicate(void)
{
    UT_CheckEvent_t                Evt;
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 10;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.SeqRejectedCount          = 0;

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_SEQ_ERR_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,              1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.TimestampMs, 4800); /* 불변 */
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* seq 역행 → 캐시 불변, SeqRejectedCount 증가, ERROR 이벤트 */
void Test_CFS_CORE_APP_SeqCheck_Regression(void)
{
    UT_CheckEvent_t                Evt;
    CFS_CORE_APP_GenericStateTlm_t Msg;
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 5; /* 이전 seq=10 보다 낮음 */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.SeqRejectedCount          = 0;

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_SEQ_ERR_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SeqRejectedCount,  1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.Seq, 10); /* 불변 */
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

void Test_CFS_CORE_APP_UpdateHealth_Failed(void)
{
    uint32 NowMs = 50000;

    /* bridge timed out, RecoveryStartMs already 30s+ ago */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* very old */
    CFS_CORE_APP_Data.RecoveryStartMs               = NowMs - 30001; /* >30s ago */

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState,       CFS_CORE_APP_HEALTH_FAILED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,         CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
}

/* RECOVERY → FAILED 에스컬레이션 타이머 동작 확인 */
void Test_CFS_CORE_APP_UpdateHealth_FailedRecovery(void)
{
    uint32 NowMs = 50000;

    /* First call: bridge timeout, RecoveryStartMs not set yet */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;
    CFS_CORE_APP_Data.RecoveryStartMs               = 0;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* Still under 30s → RECOVERY */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RecoveryStartMs,             (int32)NowMs);

    /* 30s later → FAILED */
    CFS_CORE_APP_UpdateHealth(NowMs + 30001, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_FAILED);

    /* Bridge recovers → RecoveryStartMs reset */
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs + 30001;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_FAILED;

    CFS_CORE_APP_UpdateHealth(NowMs + 30001, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RecoveryStartMs, 0);
}

void Test_CFS_CORE_APP_ServicePrototype(void)
{
    CFS_CORE_APP_Data.LastPublishTimeMs = 1000;
    CFS_CORE_APP_Data.PublishCount      = 0;

    CFS_CORE_APP_ServicePrototype();
}

void Test_CFS_CORE_APP_ProcessViewpointCommand(void)
{
    CFS_CORE_APP_ViewpointCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs    = 12345;
    Msg.SourceSequence = 7;
    Msg.ViewpointType  = 1;
    Msg.PositionFrame  = 0;
    Msg.X              = 5.0f;
    Msg.Y              = -3.0f;
    Msg.Z              = 4.0f;
    Msg.Yaw            = 1.0f;
    Msg.Pitch          = 0.5f;
    Msg.HoldTimeMs     = 2000;

    CFS_CORE_APP_Data.ViewpointCmd.Valid = false;

    CFS_CORE_APP_ProcessViewpointCommand(&Msg);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.ViewpointCmd.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ViewpointCmd.TimestampMs, 12345);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ViewpointCmd.SourceSequence, 7);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ViewpointCmd.ViewpointType, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ViewpointCmd.HoldTimeMs, 2000);
}

/* uplink_app HK 수신 → UplinkAppState.Received=true, LastHkRxMs 갱신 */
void Test_CFS_CORE_APP_ProcessStateMessage_UplinkHk(void)
{
    typedef struct
    {
        CFE_MSG_TelemetryHeader_t TelemetryHeader;
        uint8                     Pad[4];
    } TEST_UplinkHk_t;

    uint8              Storage[sizeof(TEST_UplinkHk_t)];
    CFE_SB_Buffer_t   *Buffer;
    CFE_SB_MsgId_t     MsgId;
    TEST_UplinkHk_t   *HkMsg;
    CFE_TIME_SysTime_t FakeTime;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    HkMsg  = (TEST_UplinkHk_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(HkMsg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(CFS_CORE_APP_UPLINK_HK_MID_VALUE),
                 sizeof(*HkMsg));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_UPLINK_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    /* CFE_TIME_GetTime stub → Seconds=5 → NowMs=5000 */
    FakeTime.Seconds    = 5;
    FakeTime.Subseconds = 0;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    CFS_CORE_APP_Data.UplinkAppState.Received   = false;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 0;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.UplinkAppState.Received);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs, 5000);
}

/* uplink timeout → DEGRADED + FAULT_UPLINK_TIMEOUT (우선순위 5) */
void Test_CFS_CORE_APP_UpdateHealth_UplinkTimeout(void)
{
    uint32 NowMs = 10000;

    /* bridge 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    /* EKF, Local, Attitude 정상 */
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    /* uplink HK 미수신 → timeout */
    CFS_CORE_APP_Data.UplinkAppState.Received   = false;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 0;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_UPLINK_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.UplinkStatus.TimedOut, 1);
}

/* 우선순위: attitude timeout이 uplink timeout보다 높음 */
void Test_CFS_CORE_APP_UpdateHealth_Priority_AttitudeOverUplink(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    /* attitude timed out */
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 0; /* expired */
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    /* uplink also timed out */
    CFS_CORE_APP_Data.UplinkAppState.Received   = false;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
}

/* lora_tdm HK 수신 → LoraAppState.Received=true, LastHkRxMs 갱신 */
void Test_CFS_CORE_APP_ProcessStateMessage_LoraHk(void)
{
    typedef struct
    {
        CFE_MSG_TelemetryHeader_t TelemetryHeader;
        uint8                     Pad[4];
    } TEST_LoraHk_t;

    uint8              Storage[sizeof(TEST_LoraHk_t)];
    CFE_SB_Buffer_t   *Buffer;
    CFE_SB_MsgId_t     MsgId;
    TEST_LoraHk_t     *HkMsg;
    CFE_TIME_SysTime_t FakeTime;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    HkMsg  = (TEST_LoraHk_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(HkMsg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(CFS_CORE_APP_LORA_HK_MID_VALUE),
                 sizeof(*HkMsg));
    MsgId = CFE_SB_ValueToMsgId(CFS_CORE_APP_LORA_HK_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);

    FakeTime.Seconds    = 7;
    FakeTime.Subseconds = 0;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    CFS_CORE_APP_Data.LoraAppState.Received   = false;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = 0;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.LoraAppState.Received);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraAppState.LastHkRxMs, 7000);
}

/* lora timeout → DEGRADED + FAULT_LORA_TIMEOUT (우선순위 6) */
void Test_CFS_CORE_APP_UpdateHealth_LoraTimeout(void)
{
    uint32 NowMs = 10000;

    /* bridge 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    /* EKF, Local, Attitude 정상 */
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    /* uplink 정상 */
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs - 100;
    /* lora HK 미수신 → timeout */
    CFS_CORE_APP_Data.LoraAppState.Received   = false;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = 0;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode,   CFS_CORE_APP_FAULT_LORA_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.LoraStatus.TimedOut, 1);
}

/* 우선순위: uplink timeout이 lora timeout보다 높음 */
void Test_CFS_CORE_APP_UpdateHealth_Priority_UplinkOverLora(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    /* uplink timed out */
    CFS_CORE_APP_Data.UplinkAppState.Received   = false;
    /* lora also timed out */
    CFS_CORE_APP_Data.LoraAppState.Received     = false;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_UPLINK_TIMEOUT);
}

/* ---- ProcessRecoveryCommand (A-3.1, notes/temp/a3_unittest_gap_implementation.md) ---- */

void Test_CFS_CORE_APP_ProcessRecoveryCommand_ResetCounter(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.SourceSequence  = 1;
    Msg.RecoveryAction  = CFS_CORE_APP_RECOVERY_ACTION_RESET_COUNTER;
    Msg.TargetComponent = 1;
    Msg.RequestToken    = 0x12345678;

    CFS_CORE_APP_Data.RecoveryRequestedCount        = 0;
    CFS_CORE_APP_Data.CmdCounter                    = 0;
    CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested = 0;
    CFS_CORE_APP_Data.BridgeRestartCount            = 5;

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.BridgeRestartCount, 0);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge(void)
{
    /* BL-09(2026-07-21): 이제 실제로 CFE_ES_RestartApp()을 호출해야 함 */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction  = CFS_CORE_APP_RECOVERY_ACTION_RESTART_BRIDGE;
    Msg.TargetComponent = 2;
    Msg.RequestToken    = 0xAABBCCDD;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge_AppNotFound(void)
{
    /* 앱을 못 찾으면 RestartApp을 호출하지 않고 조용히 넘어가야 함(크래시 없음) */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_RESTART_BRIDGE;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_ES_ERR_NAME_NOT_FOUND);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartUplink(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_RESTART_UPLINK;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartLora(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_RESTART_LORA;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_ParserReset(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_PARSER_RESET;
    Msg.RequestToken   = 0;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_SerialReconnect(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_SERIAL_RECONNECT;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_UnknownAction(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = 0xFF;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;

    /* switch에 매칭 case 없어도 크래시 없이 default(ERROR 이벤트)로 빠져야 함 */
    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

/* ---- ProcessModeCommand (A-3.2, notes/temp/a3_unittest_gap_implementation.md) ---- */

void Test_CFS_CORE_APP_ProcessModeCommand_EnterRecovery(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_ENTER;
    Msg.RequestedState = CFS_CORE_APP_MODE_STATE_RECOVERY;
    Msg.RequestToken   = 0x1111;

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_NORMAL;
    CFS_CORE_APP_Data.CmdCounter       = 0;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_RECOVERY);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.LastModeRequestToken, 0x1111);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ProcessModeCommand_ExitRecovery(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_EXIT;
    Msg.RequestedState = CFS_CORE_APP_MODE_STATE_NORMAL;

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_RECOVERY;
    CFS_CORE_APP_Data.CmdCounter       = 0;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_NORMAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ProcessModeCommand_InvalidTransition(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_ENTER;
    Msg.RequestedState = CFS_CORE_APP_MODE_STATE_NORMAL; /* NORMAL->NORMAL, 정의되지 않은 전이 */

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_NORMAL;
    CFS_CORE_APP_Data.CmdCounter       = 0;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    /* 전이 불허 -> 상태 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_NORMAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
}

void Test_CFS_CORE_APP_ProcessModeCommand_UnknownState(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_ENTER;
    Msg.RequestedState = 0xFF; /* 정의되지 않은 상태값 */

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_NORMAL;
    CFS_CORE_APP_Data.CmdCounter       = 0;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    /* 매칭되는 분기 없음 -> 상태 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_NORMAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
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
    ADD_TEST(CFS_CORE_APP_UpdateHealth_LocalInvalid);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_LocalStale);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_AttitudeTimeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_AttitudeInvalid);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_AttitudeStale);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_NominalStabilization);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_InputStatus);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_HealthTransition);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_Normal);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_AttitudeTimeout);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadChecksum);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_PublishPeriod);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadScope);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadVersion);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadParam);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadValue);
    ADD_TEST(CFS_CORE_APP_ProcessConfig_BadLength);
    ADD_TEST(CFS_CORE_APP_LoadState_NoFile);
    ADD_TEST(CFS_CORE_APP_SaveState_NoDir);
    ADD_TEST(CFS_CORE_APP_SaveState_OnTransition);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_GetAppIdFail);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_MaxReached);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_ResetOnRecovery);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_MaxReached);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_ResetOnRecovery);
    ADD_TEST(CFS_CORE_APP_LoraRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_LoraRestart_MaxReached);
    ADD_TEST(CFS_CORE_APP_LoraRestart_GetAppIdFail);
    ADD_TEST(CFS_CORE_APP_LoraRestart_ResetOnRecovery);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_GPS_Timeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_RecoveryToNominal);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_GPS_Rejected);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_EKF_Rejected);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_BeforeSeqCheck);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_PeriodicRateLimit);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_GetMsgIdError);
    ADD_TEST(CFS_CORE_APP_ReportHousekeeping_Fields);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_FutureTooFar);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_FutureBoundary);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_BridgeOverGps);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Startup_NoBridge);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_EkfOverLocal);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_LocalOverAttitude);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_AttitudeOverGps);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_StabilityTimerReset);
    ADD_TEST(CFS_CORE_APP_SeqCheck_Normal);
    ADD_TEST(CFS_CORE_APP_SeqCheck_Gap);
    ADD_TEST(CFS_CORE_APP_SeqCheck_Gap_FirstReceive);
    ADD_TEST(CFS_CORE_APP_SeqCheck_Duplicate);
    ADD_TEST(CFS_CORE_APP_SeqCheck_Regression);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Failed);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_FailedRecovery);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_RouteUpdate);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_LandingRouteUpdate);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_BridgeHk);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_UplinkHk);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_UplinkTimeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_AttitudeOverUplink);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_LoraHk);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_LoraTimeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Priority_UplinkOverLora);
    ADD_TEST(CFS_CORE_APP_ServicePrototype);
    ADD_TEST(CFS_CORE_APP_ProcessViewpointCommand);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_ResetCounter);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge_AppNotFound);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_RestartUplink);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_RestartLora);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_ParserReset);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_SerialReconnect);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_UnknownAction);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_EnterRecovery);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_ExitRecovery);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_InvalidTransition);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_UnknownState);
}

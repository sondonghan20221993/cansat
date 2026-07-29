/************************************************************************
 * Coverage tests for cfs_core_app_utils.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>

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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid     = 1;
    CFS_CORE_APP_Data.LocalState.Received     = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs  = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs  = 4900;
    CFS_CORE_APP_Data.LocalState.Valid        = 1;
    CFS_CORE_APP_Data.GpsState.Received       = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs    = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs    = 4800;
    CFS_CORE_APP_Data.GpsState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received       = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs    = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs    = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Stale              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 0;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 0;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 0;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
    CFS_CORE_APP_Data.EkfState.Valid              = 1;
    CFS_CORE_APP_Data.BridgeState.Received        = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT);
}

/* route_op=REPLACE(1): payload가 전체 목록이므로 ROUTE_UPDATE_MID 수신 즉시
 * MissionRoute 캐시와 HK 카운터를 함께 갱신한다(spec §18.4.6.2 캐시 갱신 정책). */
void Test_CFS_CORE_APP_ProcessStateMessage_RouteUpdate(void)
{
    uint8                        Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t             *Buffer;
    CFE_SB_MsgId_t               MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;
    uint32                         PrevUpdateCount;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    memset(&CFS_CORE_APP_Data.MissionRoute, 0, sizeof(CFS_CORE_APP_Data.MissionRoute));
    PrevUpdateCount = CFS_CORE_APP_Data.MissionRoute.UpdateCount;
    RouteMsg->TimestampMs   = 1234;
    RouteMsg->SourceSequence = 55;
    RouteMsg->RouteType     = 1; /* route_op REPLACE */
    RouteMsg->RouteVersion  = 2;
    RouteMsg->WaypointCount = 2;
    RouteMsg->Waypoints[0].CmdType = 16;
    RouteMsg->Waypoints[0].LatE7 = 100;
    RouteMsg->Waypoints[0].LonE7 = 200;
    RouteMsg->Waypoints[0].Z = 3.0f;
    RouteMsg->Waypoints[1].CmdType = 16;
    RouteMsg->Waypoints[1].LatE7 = 400;
    RouteMsg->Waypoints[1].LonE7 = 500;
    RouteMsg->Waypoints[1].Z = 4.0f;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.MissionRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.RouteVersion, 2);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 2);
    UtAssert_True(CFS_CORE_APP_Data.MissionRoute.Waypoints[0].LatE7 == 100, "wp0.LatE7 from REPLACE");
    UtAssert_True(CFS_CORE_APP_Data.MissionRoute.Waypoints[1].LonE7 == 500, "wp1.LonE7 from REPLACE");
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.UpdateCount, (int32)PrevUpdateCount + 1);
    UtAssert_INT32_EQ((int32)CFS_CORE_APP_Data.MissionRoute.TimestampMs, 1234);
}

/* BL-41 route: FC_MISSION_READBACK_MID(0x1914) 수신 → MissionRoute 캐시
 * 고정 갱신 (RouteType 검사 없음 — 출처가 FC readback, spec §16 2채널).
 * TDD red: FC_MISSION_READBACK_MID define + ProcessStateMessage 분기 요구 */
void Test_CFS_CORE_APP_ProcessStateMessage_FcReadback_UpdatesMissionRoute(void)
{
    uint8                          Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t               *Buffer;
    CFE_SB_MsgId_t                 MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->TimestampMs    = 7777;
    RouteMsg->SourceSequence = 0;   /* FC readback은 지상 시퀀스 없음 */
    RouteMsg->RouteType      = 1;   /* MISSION 고정 */
    RouteMsg->RouteVersion   = 0;
    RouteMsg->WaypointCount  = 2;
    RouteMsg->Waypoints[0].CmdType = 16;
    RouteMsg->Waypoints[0].LatE7 = 10;
    RouteMsg->Waypoints[0].LonE7 = 20;
    RouteMsg->Waypoints[0].Z = 5.0f;
    RouteMsg->Waypoints[1].CmdType = 16;
    RouteMsg->Waypoints[1].LatE7 = 30;
    RouteMsg->Waypoints[1].LonE7 = 40;
    RouteMsg->Waypoints[1].Z = 5.0f;

    memset(&CFS_CORE_APP_Data.MissionRoute, 0, sizeof(CFS_CORE_APP_Data.MissionRoute));

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.MissionRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 2);
    UtAssert_True(CFS_CORE_APP_Data.MissionRoute.Waypoints[0].LatE7 == 10, "wp0.LatE7 from FC readback");
    UtAssert_True(CFS_CORE_APP_Data.MissionRoute.Waypoints[1].LonE7 == 40, "wp1.LonE7 from FC readback");
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.UpdateCount, 1);
}

/* FC readback은 LandingRoute를 건드리지 않는다 (RouteType 무관 MissionRoute 고정) */
void Test_CFS_CORE_APP_ProcessStateMessage_FcReadback_LandingUntouched(void)
{
    uint8                          Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t               *Buffer;
    CFE_SB_MsgId_t                 MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->RouteType     = CFS_CORE_APP_ROUTE_SEGMENT_LANDING; /* 악의적/오류 값이라도 */
    RouteMsg->WaypointCount = 1;
    RouteMsg->Waypoints[0].LatE7 = 99;

    memset(&CFS_CORE_APP_Data.MissionRoute, 0, sizeof(CFS_CORE_APP_Data.MissionRoute));
    memset(&CFS_CORE_APP_Data.LandingRoute, 0, sizeof(CFS_CORE_APP_Data.LandingRoute));

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.MissionRoute.Valid);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.LandingRoute.Valid);
}

/* BL-61(2026-07-25): route_op=ADD(2) — payload가 신규분뿐(전체 목록이 아님)이라
 * ROUTE_UPDATE_MID 수신 즉시로는 MissionRoute.Waypoints/WaypointCount, HK 카운터
 * 모두 건드리지 않아야 한다(spec §18.4.6.2 캐시 갱신 정책). route_op=2는 옛
 * CFS_CORE_APP_ROUTE_SEGMENT_LANDING(=2)과 값이 겹치므로, 이 테스트는 그 회귀도
 * 함께 잡는다 — 옛 코드였다면 이 payload가 LandingRoute로 오분류돼 반영됐을 것. */
void Test_CFS_CORE_APP_ProcessStateMessage_RouteAdd_DoesNotMutateCache(void)
{
    uint8                          Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t               *Buffer;
    CFE_SB_MsgId_t                 MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;
    uint32                         PrevWaypointCount;
    uint32                         PrevUpdateCount;
    uint32                         PrevMissionTs;

    memset(&CFS_CORE_APP_Data.MissionRoute, 0, sizeof(CFS_CORE_APP_Data.MissionRoute));
    memset(&CFS_CORE_APP_Data.LandingRoute, 0, sizeof(CFS_CORE_APP_Data.LandingRoute));
    CFS_CORE_APP_Data.MissionRoute.WaypointCount = 3; /* 기존 캐시(3개짜리) */
    CFS_CORE_APP_Data.MissionRoute.UpdateCount   = 5;
    CFS_CORE_APP_Data.MissionRoute.TimestampMs   = 111;
    PrevWaypointCount = CFS_CORE_APP_Data.MissionRoute.WaypointCount;
    PrevUpdateCount   = CFS_CORE_APP_Data.MissionRoute.UpdateCount;
    PrevMissionTs     = CFS_CORE_APP_Data.MissionRoute.TimestampMs;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->TimestampMs    = 4321;
    RouteMsg->SourceSequence = 66;
    RouteMsg->RouteType      = 2; /* route_op ADD */
    RouteMsg->RouteVersion   = 2;
    RouteMsg->WaypointCount  = 1; /* 신규분 1개(전체 목록 아님) */
    RouteMsg->Waypoints[0].CmdType = 16;
    RouteMsg->Waypoints[0].LatE7 = 9;
    RouteMsg->Waypoints[0].LonE7 = 8;
    RouteMsg->Waypoints[0].Z = 3.0f;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, (int32)PrevWaypointCount);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.UpdateCount, (int32)PrevUpdateCount);
    UtAssert_INT32_EQ((int32)CFS_CORE_APP_Data.MissionRoute.TimestampMs, (int32)PrevMissionTs);
    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.LandingRoute.Valid);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.RouteUpdateCount, 0);
}

/* ADD 후 뒤이은 FC_MISSION_READBACK_MID가 최종 상태로 캐시와 HK 카운터를
 * 함께 확정 갱신한다(spec §18.4.6.2). */
void Test_CFS_CORE_APP_ProcessStateMessage_RouteAdd_ThenFcReadback_Confirms(void)
{
    uint8                          Storage[sizeof(CFS_CORE_APP_RouteUpdateTlm_t)];
    CFE_SB_Buffer_t               *Buffer;
    CFE_SB_MsgId_t                 MsgId;
    CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg;

    memset(&CFS_CORE_APP_Data.MissionRoute, 0, sizeof(CFS_CORE_APP_Data.MissionRoute));
    CFS_CORE_APP_Data.MissionRoute.WaypointCount = 3;

    /* 1) ADD 수신 — 캐시 미변경(위 테스트와 동일 전제) */
    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    RouteMsg = (CFS_CORE_APP_RouteUpdateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->RouteType     = 2; /* route_op ADD */
    RouteMsg->WaypointCount = 1;
    RouteMsg->Waypoints[0].LatE7 = 9;
    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 3);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.UpdateCount, 0);

    /* 2) 뒤이은 FC_MISSION_READBACK_MID — 4개짜리 확정 최종 상태로 갱신 */
    memset(Storage, 0, sizeof(Storage));
    CFE_MSG_Init(CFE_MSG_PTR(RouteMsg->TelemetryHeader), CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID), sizeof(*RouteMsg));
    MsgId = CFE_SB_ValueToMsgId(FC_MISSION_READBACK_MID);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    RouteMsg->TimestampMs   = 9999;
    RouteMsg->RouteType     = 1;
    RouteMsg->WaypointCount = 4;
    RouteMsg->Waypoints[3].LatE7 = 9;

    CFS_CORE_APP_ProcessStateMessage(Buffer);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.WaypointCount, 4);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.MissionRoute.UpdateCount, 1);
    UtAssert_True(CFS_CORE_APP_Data.MissionRoute.Waypoints[3].LatE7 == 9, "wp3.LatE7 confirmed via readback");

    CFS_CORE_APP_ReportHousekeeping();
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.RouteUpdateCount, 1);
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs - 100;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs + 5000 - 100;
    CFS_CORE_APP_UpdateHealth(NowMs + 5000, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);

    /* 10 s later: stable enough, transition to NOMINAL */
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 10001 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs + 10001 - 100;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.AttitudeState.Stale         = 0;
    CFS_CORE_APP_Data.AttitudeState.ErrorCode     = 0;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Stale            = 0;
    CFS_CORE_APP_Data.LocalState.ErrorCode        = 7;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 0;
    CFS_CORE_APP_Data.GpsState.Stale              = 1;
    CFS_CORE_APP_Data.GpsState.ErrorCode          = 3;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = 4900;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = 4800;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 0; /* invalid, fresh timestamp */
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Stale            = 1; /* stale, fresh timestamp */
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 0; /* invalid, fresh timestamp */
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid         = 1;
    CFS_CORE_APP_Data.AttitudeState.Stale         = 1; /* stale, fresh timestamp */
    CFS_CORE_APP_Data.LocalState.Received         = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.LocalState.Valid            = 1;
    CFS_CORE_APP_Data.GpsState.Received           = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs        = 4800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs        = 4800;
    CFS_CORE_APP_Data.GpsState.Valid              = 1;
    CFS_CORE_APP_Data.EkfState.Received           = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs        = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs        = 4900;
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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

/* BL-59(2026-07-25): Attitude 필드에 NaN이 섞이면 Valid=false로 강제되고
 * NONFINITE_VALUE_EID가 발생해야 한다. */
void Test_CFS_CORE_APP_NonFinite_Attitude_MarksInvalid(void)
{
    FC_ATTITUDE_TLM_t Msg;
    CFE_SB_MsgId_t     MsgId;
    UT_CheckEvent_t    Evt;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 100;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    Msg.RollRad     = NAN; /* 비정상 값 주입 */
    Msg.PitchRad    = 0.1f;
    Msg.YawRad      = 0.2f;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    memset(&CFS_CORE_APP_Data.AttitudeState, 0, sizeof(CFS_CORE_APP_Data.AttitudeState));

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_NONFINITE_VALUE_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.AttitudeState.Valid);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* Inf도 동일하게 걸러야 한다(속도 필드) */
void Test_CFS_CORE_APP_NonFinite_Attitude_Inf_MarksInvalid(void)
{
    FC_ATTITUDE_TLM_t Msg;
    CFE_SB_MsgId_t     MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs   = 100;
    Msg.Seq           = 1;
    Msg.Valid         = 1;
    Msg.YawspeedRps   = INFINITY;
    MsgId             = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    memset(&CFS_CORE_APP_Data.AttitudeState, 0, sizeof(CFS_CORE_APP_Data.AttitudeState));

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.AttitudeState.Valid);
}

/* 전부 유한값이면 Valid는 메시지의 Valid(1)를 그대로 유지 */
void Test_CFS_CORE_APP_Finite_Attitude_KeepsValid(void)
{
    FC_ATTITUDE_TLM_t Msg;
    CFE_SB_MsgId_t     MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 100;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    Msg.RollRad     = 0.05f;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    memset(&CFS_CORE_APP_Data.AttitudeState, 0, sizeof(CFS_CORE_APP_Data.AttitudeState));

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.AttitudeState.Valid);
}

/* BL-59: EkfLocal 필드에 NaN이 섞이면 Valid=false로 강제되어야 한다 */
void Test_CFS_CORE_APP_NonFinite_EkfLocal_MarksInvalid(void)
{
    FC_EKF_LOCAL_TLM_t Msg;
    CFE_SB_MsgId_t      MsgId;
    UT_CheckEvent_t     Evt;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 100;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    Msg.X_m         = 1.0f;
    Msg.Vz_mps      = NAN; /* 비정상 값 주입 */
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE);

    memset(&CFS_CORE_APP_Data.LocalState, 0, sizeof(CFS_CORE_APP_Data.LocalState));

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_NONFINITE_VALUE_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.LocalState.Valid);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* 전부 유한값이면 EkfLocal Valid는 그대로 유지 */
void Test_CFS_CORE_APP_Finite_EkfLocal_KeepsValid(void)
{
    FC_EKF_LOCAL_TLM_t Msg;
    CFE_SB_MsgId_t      MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 100;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    Msg.X_m         = 1.0f;
    Msg.Y_m         = 2.0f;
    Msg.Z_m         = -3.0f;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE);

    memset(&CFS_CORE_APP_Data.LocalState, 0, sizeof(CFS_CORE_APP_Data.LocalState));

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.LocalState.Valid);
}

/* BL-59: 헬스 체크는 !Valid || Stale 로직을 재사용하므로, NaN으로 Valid=false가
 * 되면 UpdateHealth가 자연히 해당 상태를 무효로 취급해야 한다(회귀 확인). */
void Test_CFS_CORE_APP_NonFinite_Attitude_ReflectsInHealthCheck(void)
{
    FC_ATTITUDE_TLM_t Msg;
    CFE_SB_MsgId_t     MsgId;

    memset(&CFS_CORE_APP_Data.GpsState, 0, sizeof(CFS_CORE_APP_Data.GpsState));
    memset(&CFS_CORE_APP_Data.EkfState, 0, sizeof(CFS_CORE_APP_Data.EkfState));
    memset(&CFS_CORE_APP_Data.LocalState, 0, sizeof(CFS_CORE_APP_Data.LocalState));
    memset(&CFS_CORE_APP_Data.AttitudeState, 0, sizeof(CFS_CORE_APP_Data.AttitudeState));

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 100;
    Msg.Seq         = 1;
    Msg.Valid       = 1;
    Msg.RollRad     = NAN;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_BOOL_FALSE(CFS_CORE_APP_Data.AttitudeState.Valid);
}

/* ForcePublish=false일 때 주기 미충족이면 게시 안 함 */
void Test_CFS_CORE_APP_UpdateHealth_PeriodicRateLimit(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = 4900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = 4900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = 4900;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = 4900;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = 4900;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = 4900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = 4900;
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
    Msg.SourceSequence = 77; /* BL-08 */

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
    /* BL-08(2026-07-22): EXEC_RESULT 회신 확인 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 77);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceApp, (int32)EXEC_RESULT_SOURCE_CFS_CORE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
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
    Msg.SourceSequence = 88; /* BL-08 */

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
    /* BL-08(2026-07-22): EXEC_RESULT FAILED 회신 확인 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 88);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.DetailCode, (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_CHECKSUM);
}

/* 잘못된 scope → REJECTED, ActiveConfig 불변 */
void Test_CFS_CORE_APP_ProcessConfig_BadScope(void)
{
    CFS_CORE_APP_ConfigCmdTlm_t Msg;
    uint32 OldTimeout = CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs;

    build_config_msg(&Msg, 0xFF, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 3000U);
    Msg.SourceSequence = 99; /* BL-08 */

    CFS_CORE_APP_Data.ErrCounter = 0;
    CFS_CORE_APP_Data.ExecResultTlm.SourceSequence = 0; /* 이전 값과 구분되게 초기화 */
    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult,
                      (int32)CFS_CORE_APP_CONFIG_RESULT_BAD_SCOPE);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigPendingState,
                      (int32)CFS_CORE_APP_CONFIG_PENDING_REJECTED);
    /* ActiveConfig 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, (int32)OldTimeout);
    /* BL-08(2026-07-22): 다른 앱 대상 브로드캐스트라 EXEC_RESULT를 보내면
     * 안 됨(발행됐다면 SourceSequence가 99로 갱신됐을 것) */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 0);
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

/* BL-41(2026-07-23): SaveState → LoadState 왕복으로 ActiveConfig(6필드)+
 * LastHealthState가 그대로 복원되는지 확인 — env var로 /tmp 경로 주입 */
void Test_CFS_CORE_APP_SaveLoadState_RoundTrip(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_roundtrip.bin";

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_RECOVERY;
    CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs = 1111;
    CFS_CORE_APP_Data.ActiveConfig.LocalTimeoutMs    = 2222;
    CFS_CORE_APP_Data.ActiveConfig.GpsTimeoutMs      = 3333;
    CFS_CORE_APP_Data.ActiveConfig.EkfTimeoutMs      = 4444;
    CFS_CORE_APP_Data.ActiveConfig.BridgeTimeoutMs   = 5555;
    CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs   = 6666;

    CFS_CORE_APP_SaveState();

    /* 복원 여부를 실제로 확인하려면 메모리 값을 지워야 함 */
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;
    CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs = 0;
    CFS_CORE_APP_Data.ActiveConfig.LocalTimeoutMs    = 0;
    CFS_CORE_APP_Data.ActiveConfig.GpsTimeoutMs      = 0;
    CFS_CORE_APP_Data.ActiveConfig.EkfTimeoutMs      = 0;
    CFS_CORE_APP_Data.ActiveConfig.BridgeTimeoutMs   = 0;
    CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs   = 0;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, 1111);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.LocalTimeoutMs, 2222);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.GpsTimeoutMs, 3333);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.EkfTimeoutMs, 4444);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.BridgeTimeoutMs, 5555);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs, 6666);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

void Test_CFS_CORE_APP_LoadState_Truncated(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_truncated.bin";
    int         Fd;
    uint8       Short[5] = {1, 2, 3, 4, 5};

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Short, sizeof(Short));
    close(Fd);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

void Test_CFS_CORE_APP_LoadState_BadMagic(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_badmagic.bin";
    int         Fd;
    uint32      Garbage[16] = {0xDEADBEEFU, 0}; /* BL-43 확장(52B) 수용 */

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Garbage, sizeof(CFS_CORE_APP_PersistentState_t));
    close(Fd);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* 매직/체크섬은 맞지만 ConfigVersion만 다른 구버전 파일 → 전체 폴백 */
void Test_CFS_CORE_APP_LoadState_ConfigVersionMismatch(void)
{
    const char                     *Path = "/tmp/cfs_core_app_ut_state_badversion.bin";
    int                              Fd;
    CFS_CORE_APP_PersistentState_t  State;

    memset(&State, 0, sizeof(State));
    State.Magic             = CFS_CORE_APP_STATE_MAGIC;
    State.LastHealthState   = CFS_CORE_APP_HEALTH_FAILED;
    State.ConfigVersion     = (uint8)(CFS_CORE_APP_CONFIG_VERSION + 1U); /* 불일치 */
    State.AttitudeTimeoutMs = 9999;
    State.Checksum          = State.Magic + (uint32)State.LastHealthState +
                              (uint32)State.ConfigVersion + State.AttitudeTimeoutMs;

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, &State, sizeof(State));
    close(Fd);

    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;
    CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs = 100;

    CFS_CORE_APP_LoadState();

    /* 버전 불일치 → 파일 값 대신 기존(호출 전) 메모리 값 그대로 유지 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, 100);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

void Test_CFS_CORE_APP_LoadState_ChecksumMismatch(void)
{
    const char                     *Path = "/tmp/cfs_core_app_ut_state_badcrc.bin";
    int                              Fd;
    CFS_CORE_APP_PersistentState_t  State;

    memset(&State, 0, sizeof(State));
    State.Magic           = CFS_CORE_APP_STATE_MAGIC;
    State.LastHealthState = CFS_CORE_APP_HEALTH_FAILED;
    State.ConfigVersion   = CFS_CORE_APP_CONFIG_VERSION;
    State.Checksum        = 0; /* 틀린 체크섬 */

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, &State, sizeof(State));
    close(Fd);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

void Test_CFS_CORE_APP_LoadState_OpenErrorNotEnoent(void)
{
    const char *RegularFile = "/tmp/cfs_core_app_ut_not_a_dir.bin";
    const char *BogusPath   = "/tmp/cfs_core_app_ut_not_a_dir.bin/x";
    int         Fd;

    Fd = open(RegularFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    close(Fd);

    setenv("CFS_CORE_APP_STATE_FILE_PATH", BogusPath, 1);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastHealthState, (int32)CFS_CORE_APP_HEALTH_NOMINAL);

    unlink(RegularFile);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* write() 실패 분기 — RLIMIT_FSIZE로 강제 EFBIG, SIGXFSZ는 무시 처리 */
void Test_CFS_CORE_APP_SaveState_WriteFail(void)
{
    const char   *Path = "/tmp/cfs_core_app_ut_state_writefail.bin";
    struct rlimit OldLimit, NewLimit;
    void        (*OldHandler)(int);

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    getrlimit(RLIMIT_FSIZE, &OldLimit);
    NewLimit.rlim_cur = 1;
    NewLimit.rlim_max = OldLimit.rlim_max;
    setrlimit(RLIMIT_FSIZE, &NewLimit);
    OldHandler = signal(SIGXFSZ, SIG_IGN);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;
    CFS_CORE_APP_SaveState();

    signal(SIGXFSZ, OldHandler);
    setrlimit(RLIMIT_FSIZE, &OldLimit);

    /* write 실패해도 크래시 없이 조용히 리턴 — 파일은 최종본으로 rename 안 됨 */
    UtAssert_True(access(Path, F_OK) != 0, "write 실패 시 최종 상태파일 생성 안 됨");

    unlink("/tmp/cfs_core_app_ut_state_writefail.bin.tmp");
    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* rename() 실패 분기 — 목적지가 이미 디렉터리라 rename()이 EISDIR로 실패 */
void Test_CFS_CORE_APP_SaveState_RenameFail(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_renamefail_dir";

    mkdir(Path, 0755);
    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;
    CFS_CORE_APP_SaveState();

    /* rename 실패해도 크래시 없음, 목적지는 여전히 디렉터리 그대로 */
    UtAssert_True(access(Path, F_OK) == 0, "목적지 경로 존재(디렉터리 그대로)");

    unlink("/tmp/cfs_core_app_ut_state_renamefail_dir.tmp");
    rmdir(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* ProcessConfigCommand가 CONFIG 적용 성공 시 실제로 SaveState()를 호출해
 * 값을 영속화하는지 배선 자체를 검증 (부수효과를 LoadState로 재확인) */
void Test_CFS_CORE_APP_ProcessConfigCommand_PersistsOnSuccess(void)
{
    const char                  *Path = "/tmp/cfs_core_app_ut_state_configwire.bin";
    CFS_CORE_APP_ConfigCmdTlm_t  Msg;

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    build_config_msg(&Msg, CFS_CORE_APP_CONFIG_SCOPE, CFS_CORE_APP_CONFIG_VERSION,
                     CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS, 4242U);
    Msg.SourceSequence = 1;

    CFS_CORE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastConfigResult, (int32)CFS_CORE_APP_CONFIG_RESULT_OK);

    /* 메모리 값을 지우고 LoadState로 실제 파일 반영 여부 확인 */
    CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs = 0;
    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs, 4242);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* BL-18 패턴(부모 디렉터리 fsync) 대상 — StatePath에 '/'가 없는 경우
 * (bare filename) 디렉터리 fsync 단계를 건너뛰어야 하며 크래시 없이
 * 저장은 정상 완료돼야 한다. */
void Test_CFS_CORE_APP_SaveState_DirFsync_NoSlashInPath(void)
{
    const char *Path = "cfs_core_app_ut_bare_state.bin";

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;
    CFS_CORE_APP_SaveState();

    UtAssert_True(access(Path, F_OK) == 0, "슬래시 없는 경로에서도 저장 완료");

    unlink("cfs_core_app_ut_bare_state.bin.tmp");
    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* BL-18 패턴 대상 — 부모 디렉터리를 열 수 없어도(fsync 실패) 파일 저장
 * 자체(rename까지)는 이미 끝난 뒤이므로 크래시 없이 조용히 넘어가야 함.
 * 디렉터리 권한을 write+exec만 남기고 read 제거(0300)해 open(O_RDONLY)만
 * 실패하도록 구성 — 파일 생성/rename엔 read 권한이 불필요해 그 전 단계는
 * 정상 통과한다. */
void Test_CFS_CORE_APP_SaveState_DirFsync_ParentOpenFail(void)
{
    const char *Dir  = "/tmp/cfs_core_app_ut_dirfsync_noread";
    char        Path[256];

    mkdir(Dir, 0755);
    chmod(Dir, 0300);
    snprintf(Path, sizeof(Path), "%s/state.bin", Dir);

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    CFS_CORE_APP_Data.LastHealthState = CFS_CORE_APP_HEALTH_DEGRADED;
    CFS_CORE_APP_SaveState();

    chmod(Dir, 0755); /* 정리 위해 read 권한 복구 */
    UtAssert_True(access(Path, F_OK) == 0, "디렉터리 fsync 실패해도 최종 파일은 저장됨");

    unlink(Path);
    {
        char TmpPath[280];
        snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", Path);
        unlink(TmpPath);
    }
    rmdir(Dir);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = NowMs - 100;
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
/* -----------------------------------------------------------------------
 * BL-43(2026-07-23): 앱 상태 영속화 — runtime spec §12.3 ② 계약 검증.
 * 재시작 카운터 3종 + LastFaultCode를 상태파일에 저장/복원, HK 노출.
 * TDD red 요구: PersistentState_t.{BridgeRestartCount,UplinkRestartCount,
 * LoraRestartCount,LastFaultCode}, HkTlm 동명 4필드.
 * ----------------------------------------------------------------------- */

/* 재시작 카운터 3종 + LastFaultCode 저장/복원 왕복 */
void Test_CFS_CORE_APP_SaveLoadState_RestartCounters_RoundTrip(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_restartcnt.bin";

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    CFS_CORE_APP_Data.BridgeRestartCount = 11;
    CFS_CORE_APP_Data.UplinkRestartCount = 22;
    CFS_CORE_APP_Data.LoraRestartCount   = 33;
    CFS_CORE_APP_Data.LastFaultCode      = CFS_CORE_APP_FAULT_EKF_INVALID;

    CFS_CORE_APP_SaveState();

    CFS_CORE_APP_Data.BridgeRestartCount = 0;
    CFS_CORE_APP_Data.UplinkRestartCount = 0;
    CFS_CORE_APP_Data.LoraRestartCount   = 0;
    CFS_CORE_APP_Data.LastFaultCode      = 0;

    CFS_CORE_APP_LoadState();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 11);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 22);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 33);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastFaultCode, (int32)CFS_CORE_APP_FAULT_EKF_INVALID);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* bridge 재시작 발행 직후 카운터가 파일에 영속화됨 (spec §12.3 ⓐ) */
void Test_CFS_CORE_APP_BridgeRestart_PersistsCounter(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_bridgewire.bin";
    uint32      NowMs = 10000;

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* timed out */
    CFS_CORE_APP_Data.BridgeRestartCount            = 0;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = NowMs - 1U; /* 인터벌 경과 */
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 1);

    /* 메모리 지우고 파일에서 복원 → 발행 직후 저장됐음 증명 */
    CFS_CORE_APP_Data.BridgeRestartCount = 0;
    CFS_CORE_APP_LoadState();
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 1);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* RECOVERY RESET_COUNTER → 리셋값이 파일에도 동기화됨 (spec §12.3 ⓑ) */
void Test_CFS_CORE_APP_ResetCounter_PersistsZero(void)
{
    const char                   *Path = "/tmp/cfs_core_app_ut_state_resetwire.bin";
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);

    /* 파일에 3개 앱 카운터 전부 저장해 둠 (BL-65: 대칭 리셋 검증) */
    CFS_CORE_APP_Data.BridgeRestartCount = 7;
    CFS_CORE_APP_Data.UplinkRestartCount = 3;
    CFS_CORE_APP_Data.LoraRestartCount   = 5;
    CFS_CORE_APP_SaveState();

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_RESET_COUNTER;

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 0);

    /* 파일도 0으로 갱신됐는지 — 값을 다시 넣고 로드해 0이 나와야 함 */
    CFS_CORE_APP_Data.BridgeRestartCount = 7;
    CFS_CORE_APP_Data.UplinkRestartCount = 3;
    CFS_CORE_APP_Data.LoraRestartCount   = 5;
    CFS_CORE_APP_LoadState();
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 0);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 0);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* health 전이 시 LastFaultCode가 기존 SaveState에 동승 (spec §12.3 ⓒ) */
void Test_CFS_CORE_APP_HealthTransition_PersistsFaultCode(void)
{
    const char *Path = "/tmp/cfs_core_app_ut_state_faultwire.bin";
    uint32      NowMs = 10000;

    setenv("CFS_CORE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    /* bridge만 타임아웃 → RECOVERY 전이(FAULT_BRIDGE_TIMEOUT) 발생 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    CFS_CORE_APP_Data.LastFaultCode = 0;
    CFS_CORE_APP_LoadState();
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LastFaultCode, (int32)CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);

    unlink(Path);
    unsetenv("CFS_CORE_APP_STATE_FILE_PATH");
}

/* HK에 재시작 카운터 3종 + LastFaultCode 노출 (spec §12.3 — 종전 RAM 전용) */
void Test_CFS_CORE_APP_ReportHousekeeping_ExposesRestartCounters(void)
{
    CFS_CORE_APP_Data.BridgeRestartCount = 3;
    CFS_CORE_APP_Data.UplinkRestartCount = 4;
    CFS_CORE_APP_Data.LoraRestartCount   = 5;
    CFS_CORE_APP_Data.LastFaultCode      = CFS_CORE_APP_FAULT_LORA_TIMEOUT;

    CFS_CORE_APP_ReportHousekeeping();

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.BridgeRestartCount, 3);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.UplinkRestartCount, 4);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.LoraRestartCount, 5);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.HkTlm.LastFaultCode, (int32)CFS_CORE_APP_FAULT_LORA_TIMEOUT);
}

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

/* BL-38(2026-07-23): 무한 재시도 확정 — MAX_RESTARTS 제거, attempt=4 이상도 계속 발행 */
void Test_CFS_CORE_APP_BridgeRestart_InfiniteRetry(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;
    CFS_CORE_APP_Data.BridgeRestartCount            = 5; /* 이전 세션 5회 시도 누적 */
    CFS_CORE_APP_Data.RecoveryStartMs               = NowMs - CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS - 1U;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 과거 시도 횟수와 무관하게 계속 재시작 발행, 카운터 단조 증가 */
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 6);
}

/* bridge 복구 시 쿨다운 타이머만 리셋, 재시도 카운터는 보존(관측용, 리셋 개념 소멸) */
void Test_CFS_CORE_APP_BridgeRestart_CooldownClearOnRecovery(void)
{
    uint32 NowMs = 20000;

    /* bridge 복구 상태 + 모든 입력 정상 */
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received         = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                 = 1;
    CFS_CORE_APP_Data.LastHealthState                = CFS_CORE_APP_HEALTH_NOMINAL;

    /* 재시작 카운터가 남아 있었음 — 복구 후에도 보존되어야 함(관측용) */
    CFS_CORE_APP_Data.BridgeRestartCount  = 2;
    CFS_CORE_APP_Data.NextBridgeRestartMs = NowMs + 1000;
    CFS_CORE_APP_Data.RecoveryStartMs     = NowMs - 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 쿨다운 타이머만 리셋(다음 장애 시 새 인터벌 시작), 카운터는 보존 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount,  2);
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
}

/* BL38-UT-1(2026-07-23): BL-38 핵심 회귀 — EKF fault(상위) 지속 중에도
 * uplink 재시작이 독립 발동해야 한다. 어제 실기(RT-CORE-003)에서 실측된
 * FAIL을 재현: 실내(GPS 없음)라 EkfTimedOut 상시 참인 상황. */
void Test_CFS_CORE_APP_UplinkRestart_DuringEkfFault(void)
{
    UT_CheckEvent_t Evt;
    uint32          NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs;
    /* EKF 고의로 무효(fault=3 EKF_INVALID) — 상위 fault 지속 상태 재현 */
    CFS_CORE_APP_Data.EkfState.Received  = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.EkfState.Valid       = 0;
    CFS_CORE_APP_Data.LocalState.Received  = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.LocalState.Valid     = 1;
    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.AttitudeState.Valid  = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 1000; /* uplink timed out */
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = NowMs;
    CFS_CORE_APP_Data.UplinkRestartCount        = 0;
    CFS_CORE_APP_Data.NextUplinkRestartMs       = NowMs - 1U; /* 쿨다운 이미 경과 */

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);
    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_UPLINK_RESTART_EID, NULL);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* 재시작은 발동(체인 비종속) */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
    /* 그러나 보고되는 FaultCode는 여전히 상위(EKF)=3 — 보고 체인 불변(확정 6번) */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_EKF_INVALID);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.UplinkStatus.TimedOut, 1);
}

/* BL38-UT-2/3(2026-07-23): 동시 다중 사망 시 사이클당 1건만, 우선순위 bridge>uplink>lora.
 * 스킵된 앱은 다음 사이클(별도 UpdateHealth 호출)에서 처리된다. */
void Test_CFS_CORE_APP_CheckAppRestarts_OnePerCyclePriority(void)
{
    uint32 NowMs = 10000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* bridge timed out */
    CFS_CORE_APP_Data.EkfState.Received    = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received  = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.LocalState.Valid     = 1;
    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.AttitudeState.Valid  = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 1000; /* uplink도 timed out */
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = NowMs;
    CFS_CORE_APP_Data.NextBridgeRestartMs = NowMs - 1U;
    CFS_CORE_APP_Data.NextUplinkRestartMs = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    /* 사이클 1: bridge만 재시작(우선순위 1위), uplink는 스킵 */
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 0);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);

    /* 사이클 2: bridge는 쿨다운(5초), uplink 쿨다운은 그대로 경과 상태 → uplink 재시작 */
    NowMs += 1000;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000; /* 여전히 timed out */
    CFS_CORE_APP_Data.EkfState.TimestampMs    = NowMs;
    CFS_CORE_APP_Data.EkfState.ArrivalMs    = NowMs;
    CFS_CORE_APP_Data.LocalState.TimestampMs  = NowMs;
    CFS_CORE_APP_Data.LocalState.ArrivalMs  = NowMs;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs;
    CFS_CORE_APP_Data.NextUplinkRestartMs = NowMs - 1U;
    CFS_CORE_APP_UpdateHealth(NowMs, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.BridgeRestartCount, 1); /* 쿨다운 중이라 불변 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 2);
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

/* BL-38(2026-07-23): 무한 재시도 — uplink도 과거 시도 횟수와 무관하게 계속 발행 */
void Test_CFS_CORE_APP_UplinkRestart_InfiniteRetry(void)
{
    uint32 NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = 1000;
    CFS_CORE_APP_Data.LoraAppState.Received     = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs   = NowMs;
    CFS_CORE_APP_Data.UplinkRestartCount        = 5;
    CFS_CORE_APP_Data.NextUplinkRestartMs       = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount, 6);
}

/* uplink 복구 시 쿨다운 타이머만 리셋, 카운터는 보존 */
void Test_CFS_CORE_APP_UplinkRestart_CooldownClearOnRecovery(void)
{
    uint32 NowMs = 20000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received       = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received         = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs - 100;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_Data.UplinkRestartCount  = 2;
    CFS_CORE_APP_Data.NextUplinkRestartMs = NowMs + 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.UplinkRestartCount,  2);
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

/* BL-38(2026-07-23): 무한 재시도 — lora도 과거 시도 횟수와 무관하게 계속 발행 */
void Test_CFS_CORE_APP_LoraRestart_InfiniteRetry(void)
{
    uint32 NowMs = 10000;

    SetHealthyExceptUplinkLora(NowMs);
    CFS_CORE_APP_Data.UplinkAppState.Received = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs;
    CFS_CORE_APP_Data.LoraAppState.Received   = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs = 1000;
    CFS_CORE_APP_Data.LoraRestartCount        = 5;
    CFS_CORE_APP_Data.NextLoraRestartMs       = NowMs - 1U;

    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount, 6);
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

/* lora_tdm_app 복구 시 쿨다운 타이머만 리셋, 카운터는 보존 (Bridge/Uplink와 동일 패턴) */
void Test_CFS_CORE_APP_LoraRestart_CooldownClearOnRecovery(void)
{
    uint32 NowMs = 20000;

    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Received        = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid                = 1;
    CFS_CORE_APP_Data.UplinkAppState.Received       = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs     = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received         = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs       = NowMs - 100;
    CFS_CORE_APP_Data.LastHealthState               = CFS_CORE_APP_HEALTH_NOMINAL;

    CFS_CORE_APP_Data.LoraRestartCount  = 2;
    CFS_CORE_APP_Data.NextLoraRestartMs = NowMs + 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.LoraRestartCount,  2);
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 9900;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = NowMs - 100;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = T5 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = T5 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = T5 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = T5 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = T5 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = T5 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = T5 - 100;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs      = T5 - 100;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs        = T5 - 100;
    CFS_CORE_APP_UpdateHealth(T5, true);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_DEGRADED);

    /* 10초 후: NOMINAL 복귀 */
    uint32 T10 = NowMs + 10001;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = T10 - 100;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs      = T10 - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = T10 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = T10 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = T10 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = T10 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = T10 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = T10 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = T10 - 100;
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
    CFS_CORE_APP_Data.GpsState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.GpsState.Valid       = 1;
    CFS_CORE_APP_Data.GpsState.Stale       = 1; /* GPS stale도 동시 발생 */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received       = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs    = 9900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs    = 9900;
    CFS_CORE_APP_Data.LocalState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received         = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs      = 9900;
    CFS_CORE_APP_Data.EkfState.ArrivalMs      = 9900;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 0; /* EKF invalid */

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 0; /* Local도 invalid */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 1;
    CFS_CORE_APP_Data.GpsState.Received         = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs      = 9800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs      = 9800;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 0; /* Local invalid */

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 0; /* Attitude도 invalid */

    CFS_CORE_APP_Data.GpsState.Received    = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 9800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs = 9800;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;

    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = 9900;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 9900;
    CFS_CORE_APP_Data.AttitudeState.Valid       = 0; /* Attitude invalid */

    CFS_CORE_APP_Data.GpsState.Received = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs = 9800;
    CFS_CORE_APP_Data.GpsState.ArrivalMs = 9800;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs      = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid            = 1;
    CFS_CORE_APP_Data.LocalState.Received            = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs         = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid               = 1;
    CFS_CORE_APP_Data.GpsState.Received              = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs           = NowMs - 100;
    CFS_CORE_APP_Data.GpsState.Valid                 = 1;
    CFS_CORE_APP_Data.EkfState.Received              = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs           = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs           = NowMs - 100;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 5000 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs + 5000 - 100;
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 20; /* 이전 seq=10 대비 9개 갭 */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 4800;
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 10;
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 4800;
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
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
    CFE_SB_MsgId_t                 MsgId;

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 4900;
    Msg.Seq         = 5; /* 이전 seq=10 보다 낮음 */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 10;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4800;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = 4800;
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
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs     = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.AttitudeState.Valid           = 1;
    CFS_CORE_APP_Data.LocalState.Received           = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs        = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs        = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.LocalState.Valid              = 1;
    CFS_CORE_APP_Data.GpsState.Received             = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs          = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.GpsState.ArrivalMs          = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.GpsState.Valid                = 1;
    CFS_CORE_APP_Data.EkfState.Received             = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs          = NowMs + 30001 - 100;
    CFS_CORE_APP_Data.EkfState.ArrivalMs          = NowMs + 30001 - 100;
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
    /* BL-82(2026-07-28 감사) 회귀: 캐시 저장만 하고 EXEC_RESULT를 발행하지 않아
     * uplink_app이 영원히 ROUTED에 머물던 dead-end. BL-10(짐벌 미탑재로 범위
     * 제외)에 따라 실제 실행 대신 명시적 FAILED 회신만 확인한다. */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 7);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs - 100;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs - 100;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs - 100;
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
    CFS_CORE_APP_Data.EkfState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.EkfState.Valid       = 1;
    CFS_CORE_APP_Data.LocalState.Received    = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.ArrivalMs = NowMs - 100;
    CFS_CORE_APP_Data.LocalState.Valid       = 1;
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = NowMs - 100;
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs = NowMs - 100;
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
    /* BL-65: Uplink/Lora 카운터+대기 타이머도 대칭적으로 리셋돼야 함 */
    CFS_CORE_APP_Data.UplinkRestartCount            = 4;
    CFS_CORE_APP_Data.LoraRestartCount              = 6;
    CFS_CORE_APP_Data.NextBridgeRestartMs           = 1000;
    CFS_CORE_APP_Data.NextUplinkRestartMs           = 2000;
    CFS_CORE_APP_Data.NextLoraRestartMs             = 3000;

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.BridgeRestartCount, 0);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.UplinkRestartCount, 0);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.LoraRestartCount, 0);
    UtAssert_UINT32_EQ(CFS_CORE_APP_Data.NextBridgeRestartMs, 0);
    UtAssert_UINT32_EQ(CFS_CORE_APP_Data.NextUplinkRestartMs, 0);
    UtAssert_UINT32_EQ(CFS_CORE_APP_Data.NextLoraRestartMs, 0);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge(void)
{
    /* BL-09(2026-07-21): 이제 실제로 CFE_ES_RestartApp()을 호출해야 함 */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction  = CFS_CORE_APP_RECOVERY_ACTION_RESTART_BRIDGE;
    Msg.TargetComponent = 2;
    Msg.RequestToken    = 0xAABBCCDD;
    Msg.SourceSequence  = 55; /* BL-08 */

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 1);
    /* BL-08(2026-07-22): EXEC_RESULT OK 회신 확인 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 55);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.DetailCode, (int32)CFS_CORE_APP_RECOVERY_ACTION_RESTART_BRIDGE);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_RestartBridge_AppNotFound(void)
{
    /* 앱을 못 찾으면 RestartApp을 호출하지 않고 조용히 넘어가야 함(크래시 없음) */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_RESTART_BRIDGE;
    Msg.SourceSequence = 56; /* BL-08 */

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_ES_GetAppIDByName), CFE_ES_ERR_NAME_NOT_FOUND);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_STUB_COUNT(CFE_ES_RestartApp, 0);
    /* BL-08(2026-07-22): 앱을 못 찾았으니 EXEC_RESULT는 FAILED여야 함 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 56);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
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
    /* P1-a(2026-07-22): 이제 실제로 mavlink_bridge_app CMD_MID로 발행해야 함 */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_PARSER_RESET;
    Msg.RequestToken   = 0;
    Msg.SourceSequence = 57;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_MSG_SetFcnCode), CFE_SUCCESS);
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_MSG_SetFcnCode, 1);
    /* EXEC_RESULT OK 회신 확인 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 57);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_SerialReconnect(void)
{
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_SERIAL_RECONNECT;
    Msg.SourceSequence = 58;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    CFS_CORE_APP_Data.CmdCounter             = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_MSG_SetFcnCode), CFE_SUCCESS);
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.RecoveryRequestedCount, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_STUB_COUNT(CFE_MSG_SetFcnCode, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 58);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

void Test_CFS_CORE_APP_ProcessRecoveryCommand_ParserReset_SendFails(void)
{
    /* FcnCode 설정 실패 시 mavlink_bridge_app으로 발행 안 되고 FAILED 회신 */
    CFS_CORE_APP_RecoveryCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RecoveryAction = CFS_CORE_APP_RECOVERY_ACTION_PARSER_RESET;
    Msg.SourceSequence = 59;

    CFS_CORE_APP_Data.RecoveryRequestedCount = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_MSG_SetFcnCode), -1);

    CFS_CORE_APP_ProcessRecoveryCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 59);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
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
    Msg.SourceSequence                 = 61;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_RECOVERY);
    UtAssert_INT32_EQ((int)CFS_CORE_APP_Data.LastModeRequestToken, 0x1111);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    /* BL-81(2026-07-28 감사) 회귀: 전이 성공 시 EXEC_RESULT가 실제로 발행돼야
     * uplink_app이 ROUTED에서 벗어나 EXECUTED_OK를 받을 수 있음 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 61);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

void Test_CFS_CORE_APP_ProcessModeCommand_ExitRecovery(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_EXIT;
    Msg.RequestedState = CFS_CORE_APP_MODE_STATE_NORMAL;

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_RECOVERY;
    CFS_CORE_APP_Data.CmdCounter       = 0;
    Msg.SourceSequence                 = 62;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_NORMAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 62);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

void Test_CFS_CORE_APP_ProcessModeCommand_InvalidTransition(void)
{
    CFS_CORE_APP_ModeCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.ModeAction     = CFS_CORE_APP_MODE_ACTION_ENTER;
    Msg.RequestedState = CFS_CORE_APP_MODE_STATE_NORMAL; /* NORMAL->NORMAL, 정의되지 않은 전이 */

    CFS_CORE_APP_Data.CurrentModeState = CFS_CORE_APP_MODE_STATE_NORMAL;
    CFS_CORE_APP_Data.CmdCounter       = 0;
    CFS_CORE_APP_Data.ErrCounter       = 0;
    Msg.SourceSequence                 = 63;

    CFS_CORE_APP_ProcessModeCommand(&Msg);

    /* 전이 불허 -> 상태 불변. BL-81 회귀: 거부돼도 EXEC_RESULT는 발행돼야
     * uplink_app이 ROUTED에 무기한 머물지 않고(FAILED 회신), ErrCounter도
     * 증가해야 함 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CurrentModeState, CFS_CORE_APP_MODE_STATE_NORMAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.SourceSequence, 63);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
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

/* waypoint readback(2026-07-23, spec §4.3) */

void Test_CFS_CORE_APP_ProcessDiagnosticCommand_RouteReadback(void)
{
    CFS_CORE_APP_DiagnosticCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.DiagTarget     = CFS_CORE_APP_DIAG_TARGET_CFS_CORE;
    Msg.DiagAction     = CFS_CORE_APP_DIAG_ACTION_ROUTE_READBACK_REQUEST;
    Msg.SourceSequence = 42;
    Msg.RequestToken   = 0xABCD;

    memset(&CFS_CORE_APP_Data, 0, sizeof(CFS_CORE_APP_Data));
    CFS_CORE_APP_Data.MissionRoute.WaypointCount = 3;
    CFS_CORE_APP_Data.MissionRoute.Waypoints[0].LatE7 = 1;
    CFS_CORE_APP_Data.MissionRoute.Waypoints[1].LonE7 = 2;
    CFS_CORE_APP_Data.MissionRoute.Waypoints[2].Z = 3.0f;

    CFS_CORE_APP_ProcessDiagnosticCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 1);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RouteSnapshotTlm.WaypointCount, 3);
    /* BL-89(2026-07-28 감사) 회귀: RouteType이 route_op REPLACE(=1)와 겹치던
     * MISSION_EXTENSION 대신 route_op 어디와도 안 겹치는 NONE(0)이어야 함 —
     * 그래야 수신측(lora_tdm_app/지상)이 readback을 REPLACE 연산으로 오독 안 함 */
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RouteSnapshotTlm.RouteType,
                      (int32)CFS_CORE_APP_ROUTE_SEGMENT_NONE);
    UtAssert_True(CFS_CORE_APP_Data.RouteSnapshotTlm.Waypoints[0].LatE7 == 1, "waypoint 0 LatE7 복사됨");
    UtAssert_True(CFS_CORE_APP_Data.RouteSnapshotTlm.Waypoints[2].Z == 3.0f, "waypoint 2 Z 복사됨");
}

/* DiagTarget이 자기(cfs_core) 대상이 아니면(lora_tdm 대상 등) 조용히 무시 */
void Test_CFS_CORE_APP_ProcessDiagnosticCommand_TargetNotSelf_Ignored(void)
{
    CFS_CORE_APP_DiagnosticCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.DiagTarget = CFS_CORE_APP_DIAG_TARGET_LORA_TDM;
    Msg.DiagAction = CFS_CORE_APP_DIAG_ACTION_ROUTE_READBACK_REQUEST;

    memset(&CFS_CORE_APP_Data, 0, sizeof(CFS_CORE_APP_Data));

    CFS_CORE_APP_ProcessDiagnosticCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 0);
}

/* 대상은 맞지만 미지원 DiagAction이면 에러 이벤트만, 카운터 불변 */
void Test_CFS_CORE_APP_ProcessDiagnosticCommand_UnknownAction(void)
{
    CFS_CORE_APP_DiagnosticCmdTlm_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.DiagTarget = CFS_CORE_APP_DIAG_TARGET_CFS_CORE;
    Msg.DiagAction = 0xFFU;

    memset(&CFS_CORE_APP_Data, 0, sizeof(CFS_CORE_APP_Data));

    CFS_CORE_APP_ProcessDiagnosticCommand(&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.CmdCounter, 0);
}

/* -----------------------------------------------------------------------
 * BL-42(2026-07-24): time base 검증 — 만료는 Pi 도착시각(ArrivalMs) 기준,
 * FC 재부팅(TimestampMs 역행)은 감지만.
 * ----------------------------------------------------------------------- */

/* 만료 판정이 FC TimestampMs가 아니라 Pi 도착시각(ArrivalMs)을 쓴다.
 * TimestampMs=0(구 계약에선 만료)이라도 ArrivalMs가 신선하면 NOMINAL. */
void Test_CFS_CORE_APP_Expiry_UsesArrivalMs_NotTimestamp(void)
{
    uint32 NowMs = 5000;

    memset(&CFS_CORE_APP_Data.SystemHealthTlm, 0, sizeof(CFS_CORE_APP_Data.SystemHealthTlm));
    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 0;      /* FC 기준값은 만료처럼 보임 */
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 4900;   /* 실제로는 100ms 전 도착 */
    CFS_CORE_APP_Data.AttitudeState.Valid     = 1;
    CFS_CORE_APP_Data.LocalState.Received     = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs  = 0;
    CFS_CORE_APP_Data.LocalState.ArrivalMs    = 4900;
    CFS_CORE_APP_Data.LocalState.Valid        = 1;
    CFS_CORE_APP_Data.GpsState.Received       = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs    = 0;
    CFS_CORE_APP_Data.GpsState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.GpsState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received       = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs    = 0;
    CFS_CORE_APP_Data.EkfState.ArrivalMs      = 4900;
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

/* 반대: ArrivalMs가 오래됐으면 TimestampMs가 신선해도 만료로 판정. */
void Test_CFS_CORE_APP_Expiry_ArrivalStale_Expires(void)
{
    uint32 NowMs = 5000;

    memset(&CFS_CORE_APP_Data.SystemHealthTlm, 0, sizeof(CFS_CORE_APP_Data.SystemHealthTlm));
    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4950;   /* FC 기준값은 신선처럼 보임 */
    CFS_CORE_APP_Data.AttitudeState.ArrivalMs   = 0;      /* 실제로는 5000ms 전 도착 → 만료 */
    CFS_CORE_APP_Data.AttitudeState.Valid     = 1;
    CFS_CORE_APP_Data.LocalState.Received     = true;
    CFS_CORE_APP_Data.LocalState.ArrivalMs    = 4900;
    CFS_CORE_APP_Data.LocalState.Valid        = 1;
    CFS_CORE_APP_Data.GpsState.Received       = true;
    CFS_CORE_APP_Data.GpsState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.GpsState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received       = true;
    CFS_CORE_APP_Data.EkfState.ArrivalMs      = 4900;
    CFS_CORE_APP_Data.EkfState.Valid          = 1;
    CFS_CORE_APP_Data.BridgeState.Received    = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;
    CFS_CORE_APP_Data.UplinkAppState.Received    = true;
    CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs  = NowMs - 100;
    CFS_CORE_APP_Data.LoraAppState.Received      = true;
    CFS_CORE_APP_Data.LoraAppState.LastHkRxMs    = NowMs - 100;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    /* Attitude 만료 → NOMINAL 아님 */
    UtAssert_INT32_NEQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
}

/* FC 재부팅: TimestampMs가 크게 역행하면 TIMEBASE_SHIFT_EID + 카운터,
 * 메시지는 거부하지 않고 새 기준으로 캐시 갱신. */
void Test_CFS_CORE_APP_TimebaseShift_DetectsFcReboot(void)
{
    FC_ATTITUDE_TLM_t Msg; /* BL-59: full type — dispatch re-casts to check isfinite() */
    CFE_SB_MsgId_t                 MsgId;
    CFE_TIME_SysTime_t             FakeTime;
    UT_CheckEvent_t                Evt;

    /* 직전 캐시: TimestampMs=100000, Seq=1 수신 상태 */
    memset(&CFS_CORE_APP_Data.AttitudeState, 0, sizeof(CFS_CORE_APP_Data.AttitudeState));
    CFS_CORE_APP_Data.AttitudeState.Received    = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 100000U;
    CFS_CORE_APP_Data.AttitudeState.Seq         = 1;
    CFS_CORE_APP_Data.TimebaseShiftCount        = 0;

    /* NowMs 충분히 크게(미래거부 회피): Seconds=200 → 200000ms */
    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 200U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    memset(&Msg, 0, sizeof(Msg));
    Msg.TimestampMs = 500U;   /* FC 재부팅 → time_boot_ms 리셋(10s 이상 역행) */
    Msg.Seq         = 2;       /* Pi측 seq는 계속 증가 */
    Msg.Valid       = 1;
    MsgId           = CFE_SB_ValueToMsgId(CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE);

    UT_CHECKEVENT_SETUP(&Evt, CFS_CORE_APP_TIMEBASE_SHIFT_EID, NULL);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    CFS_CORE_APP_ProcessStateMessage((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.TimebaseShiftCount, 1);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    /* 거부하지 않고 새 기준으로 갱신 */
    UtAssert_BOOL_TRUE(CFS_CORE_APP_Data.AttitudeState.Received);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.AttitudeState.TimestampMs, 500);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Expiry_UsesArrivalMs_NotTimestamp);
    ADD_TEST(CFS_CORE_APP_Expiry_ArrivalStale_Expires);
    ADD_TEST(CFS_CORE_APP_TimebaseShift_DetectsFcReboot);
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
    ADD_TEST(CFS_CORE_APP_SaveLoadState_RoundTrip);
    ADD_TEST(CFS_CORE_APP_LoadState_Truncated);
    ADD_TEST(CFS_CORE_APP_LoadState_BadMagic);
    ADD_TEST(CFS_CORE_APP_LoadState_ConfigVersionMismatch);
    ADD_TEST(CFS_CORE_APP_LoadState_ChecksumMismatch);
    ADD_TEST(CFS_CORE_APP_LoadState_OpenErrorNotEnoent);
    ADD_TEST(CFS_CORE_APP_SaveState_WriteFail);
    ADD_TEST(CFS_CORE_APP_SaveState_RenameFail);
    ADD_TEST(CFS_CORE_APP_ProcessConfigCommand_PersistsOnSuccess);
    ADD_TEST(CFS_CORE_APP_SaveState_DirFsync_NoSlashInPath);
    ADD_TEST(CFS_CORE_APP_SaveState_DirFsync_ParentOpenFail);
    ADD_TEST(CFS_CORE_APP_SaveLoadState_RestartCounters_RoundTrip);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_PersistsCounter);
    ADD_TEST(CFS_CORE_APP_ResetCounter_PersistsZero);
    ADD_TEST(CFS_CORE_APP_HealthTransition_PersistsFaultCode);
    ADD_TEST(CFS_CORE_APP_ReportHousekeeping_ExposesRestartCounters);
    ADD_TEST(CFS_CORE_APP_SaveState_OnTransition);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_GetAppIdFail);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_InfiniteRetry);
    ADD_TEST(CFS_CORE_APP_BridgeRestart_CooldownClearOnRecovery);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_DuringEkfFault);
    ADD_TEST(CFS_CORE_APP_CheckAppRestarts_OnePerCyclePriority);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_InfiniteRetry);
    ADD_TEST(CFS_CORE_APP_UplinkRestart_CooldownClearOnRecovery);
    ADD_TEST(CFS_CORE_APP_LoraRestart_FirstAttempt);
    ADD_TEST(CFS_CORE_APP_LoraRestart_InfiniteRetry);
    ADD_TEST(CFS_CORE_APP_LoraRestart_GetAppIdFail);
    ADD_TEST(CFS_CORE_APP_LoraRestart_CooldownClearOnRecovery);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_GPS_Timeout);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_RecoveryToNominal);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_GPS_Rejected);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_EKF_Rejected);
    ADD_TEST(CFS_CORE_APP_TimestampCheck_BeforeSeqCheck);
    ADD_TEST(CFS_CORE_APP_NonFinite_Attitude_MarksInvalid);
    ADD_TEST(CFS_CORE_APP_NonFinite_Attitude_Inf_MarksInvalid);
    ADD_TEST(CFS_CORE_APP_Finite_Attitude_KeepsValid);
    ADD_TEST(CFS_CORE_APP_NonFinite_EkfLocal_MarksInvalid);
    ADD_TEST(CFS_CORE_APP_Finite_EkfLocal_KeepsValid);
    ADD_TEST(CFS_CORE_APP_NonFinite_Attitude_ReflectsInHealthCheck);
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
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_RouteAdd_DoesNotMutateCache);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_RouteAdd_ThenFcReadback_Confirms);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_FcReadback_UpdatesMissionRoute);
    ADD_TEST(CFS_CORE_APP_ProcessStateMessage_FcReadback_LandingUntouched);
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
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_ParserReset_SendFails);
    ADD_TEST(CFS_CORE_APP_ProcessRecoveryCommand_UnknownAction);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_EnterRecovery);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_ExitRecovery);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_InvalidTransition);
    ADD_TEST(CFS_CORE_APP_ProcessModeCommand_UnknownState);
    ADD_TEST(CFS_CORE_APP_ProcessDiagnosticCommand_RouteReadback);
    ADD_TEST(CFS_CORE_APP_ProcessDiagnosticCommand_TargetNotSelf_Ignored);
    ADD_TEST(CFS_CORE_APP_ProcessDiagnosticCommand_UnknownAction);
}

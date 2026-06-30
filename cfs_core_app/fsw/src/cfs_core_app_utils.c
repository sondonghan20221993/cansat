#include "cfs_core_app_utils.h"
#include "cfs_core_app_eventids.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static uint32 CFS_CORE_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

static void CFS_CORE_APP_UpdateStateCache(CFS_CORE_APP_StateCache_t *Cache,
                                          const CFS_CORE_APP_GenericStateTlm_t *Msg,
                                          uint32 NowMs)
{
    /* 타임스탬프 유효성 검사: 미래 타임스탬프 거부 */
    if (Msg->TimestampMs > NowMs + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS)
    {
        CFS_CORE_APP_Data.TimestampRejectedCount++;
        CFE_EVS_SendEvent(CFS_CORE_APP_TIMESTAMP_ERR_EID, CFE_EVS_EventType_ERROR,
                          "CFS_CORE_APP: future timestamp ts=%lu now=%lu rejected",
                          (unsigned long)Msg->TimestampMs, (unsigned long)NowMs);
        return;
    }

    /* 시퀀스 중복/역행/갭 검사 */
    if (Cache->Received)
    {
        if (Msg->Seq == Cache->Seq)
        {
            CFS_CORE_APP_Data.SeqRejectedCount++;
            CFE_EVS_SendEvent(CFS_CORE_APP_SEQ_ERR_EID, CFE_EVS_EventType_DEBUG,
                              "CFS_CORE_APP: duplicate seq=%lu rejected", (unsigned long)Msg->Seq);
            return;
        }
        if (Msg->Seq < Cache->Seq)
        {
            CFS_CORE_APP_Data.SeqRejectedCount++;
            CFE_EVS_SendEvent(CFS_CORE_APP_SEQ_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CFS_CORE_APP: seq regression %lu->%lu rejected",
                              (unsigned long)Cache->Seq, (unsigned long)Msg->Seq);
            return;
        }
        if (Msg->Seq > Cache->Seq + 1U)
        {
            CFS_CORE_APP_Data.SeqGapCount++;
            CFE_EVS_SendEvent(CFS_CORE_APP_SEQ_GAP_EID, CFE_EVS_EventType_DEBUG,
                              "CFS_CORE_APP: seq gap %lu->%lu (%lu missed)",
                              (unsigned long)Cache->Seq, (unsigned long)Msg->Seq,
                              (unsigned long)(Msg->Seq - Cache->Seq - 1U));
        }
    }

    Cache->TimestampMs = Msg->TimestampMs;
    Cache->Seq         = Msg->Seq;
    Cache->Valid       = Msg->Valid;
    Cache->Stale       = Msg->Stale;
    Cache->ErrorCode   = Msg->ErrorCode;
    Cache->Received    = true;
}

static bool CFS_CORE_APP_StateExpired(const CFS_CORE_APP_StateCache_t *Cache, uint32 NowMs, uint32 TimeoutMs)
{
    if (!Cache->Received)
    {
        return true;
    }

    return (NowMs - Cache->TimestampMs) > TimeoutMs;
}

static void CFS_CORE_APP_UpdateRouteCache(CFS_CORE_APP_RouteCache_t *Cache, const CFS_CORE_APP_RouteUpdateTlm_t *Msg)
{
    Cache->TimestampMs   = Msg->TimestampMs;
    Cache->SourceSequence = Msg->SourceSequence;
    Cache->UpdateCount++;
    Cache->RouteType     = Msg->RouteType;
    Cache->RouteVersion  = Msg->RouteVersion;
    Cache->WaypointCount = Msg->WaypointCount;
    Cache->Valid         = true;
    memcpy(Cache->Waypoints, Msg->Waypoints, sizeof(Cache->Waypoints));
}

void CFS_CORE_APP_ReportHousekeeping(void)
{
    CFS_CORE_APP_Data.HkTlm.CommandCounter       = CFS_CORE_APP_Data.CmdCounter;
    CFS_CORE_APP_Data.HkTlm.CommandErrorCounter  = CFS_CORE_APP_Data.ErrCounter;
    CFS_CORE_APP_Data.HkTlm.MissionRouteWaypointCount = CFS_CORE_APP_Data.MissionRoute.WaypointCount;
    CFS_CORE_APP_Data.HkTlm.LandingRouteWaypointCount = CFS_CORE_APP_Data.LandingRoute.WaypointCount;
    CFS_CORE_APP_Data.HkTlm.PublishCount         = CFS_CORE_APP_Data.PublishCount;
    CFS_CORE_APP_Data.HkTlm.LastPublishTimestampMs = CFS_CORE_APP_Data.LastPublishTimeMs;
    CFS_CORE_APP_Data.HkTlm.LastRouteUpdateTimestampMs =
        (CFS_CORE_APP_Data.MissionRoute.TimestampMs > CFS_CORE_APP_Data.LandingRoute.TimestampMs) ?
            CFS_CORE_APP_Data.MissionRoute.TimestampMs :
            CFS_CORE_APP_Data.LandingRoute.TimestampMs;
    CFS_CORE_APP_Data.HkTlm.RouteUpdateCount = CFS_CORE_APP_Data.MissionRoute.UpdateCount +
                                               CFS_CORE_APP_Data.LandingRoute.UpdateCount;

    CFE_EVS_SendEvent(CFS_CORE_APP_HK_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP HK: mission_wp=%u landing_wp=%u route_updates=%lu last_route_ts=%lu",
                      (unsigned int)CFS_CORE_APP_Data.HkTlm.MissionRouteWaypointCount,
                      (unsigned int)CFS_CORE_APP_Data.HkTlm.LandingRouteWaypointCount,
                      (unsigned long)CFS_CORE_APP_Data.HkTlm.RouteUpdateCount,
                      (unsigned long)CFS_CORE_APP_Data.HkTlm.LastRouteUpdateTimestampMs);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(CFS_CORE_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(CFS_CORE_APP_Data.HkTlm.TelemetryHeader), true);
}

bool CFS_CORE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(CFS_CORE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "CFS_CORE_APP: Invalid cmd length expected=%lu actual=%lu",
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }

    return true;
}

void CFS_CORE_APP_ProcessStateMessage(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t       MsgId;
    CFE_Status_t         Status;
    CFE_MSG_Message_t   *MsgPtr;
    uint32               NowMs;

    NowMs  = CFS_CORE_APP_GetTimeMs();
    MsgPtr = &SBBufPtr->Msg;
    Status = CFE_MSG_GetMsgId(MsgPtr, &MsgId);
    if (Status != CFE_SUCCESS)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        return;
    }

    if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_BRIDGE_HK_MID_VALUE)
    {
        const CFS_CORE_APP_BridgeHkMirror_t *BridgeHk = (const CFS_CORE_APP_BridgeHkMirror_t *)MsgPtr;

        CFS_CORE_APP_Data.BridgeState.LinkState         = BridgeHk->LinkState;
        CFS_CORE_APP_Data.BridgeState.LastErrorCode     = BridgeHk->LastErrorCode;
        CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = BridgeHk->LastRxTimestampMs;
        CFS_CORE_APP_Data.BridgeState.Received          = true;
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_UPLINK_HK_MID_VALUE)
    {
        CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs = NowMs;
        CFS_CORE_APP_Data.UplinkAppState.Received   = true;
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.AttitudeState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr, NowMs);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.LocalState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr, NowMs);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.GpsState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr, NowMs);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.EkfState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr, NowMs);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == ROUTE_UPDATE_MID)
    {
        const CFS_CORE_APP_RouteUpdateTlm_t *RouteMsg = (const CFS_CORE_APP_RouteUpdateTlm_t *)MsgPtr;

        if (RouteMsg->RouteType == CFS_CORE_APP_ROUTE_SEGMENT_MISSION_EXTENSION)
        {
            CFS_CORE_APP_UpdateRouteCache(&CFS_CORE_APP_Data.MissionRoute, RouteMsg);
        }
        else if (RouteMsg->RouteType == CFS_CORE_APP_ROUTE_SEGMENT_LANDING)
        {
            CFS_CORE_APP_UpdateRouteCache(&CFS_CORE_APP_Data.LandingRoute, RouteMsg);
        }

        CFE_EVS_SendEvent(CFS_CORE_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                          "CFS_CORE_APP: route updated type=%u version=%u count=%u src_seq=%lu",
                          (unsigned int)RouteMsg->RouteType, (unsigned int)RouteMsg->RouteVersion,
                          (unsigned int)RouteMsg->WaypointCount, (unsigned long)RouteMsg->SourceSequence);
    }

    CFS_CORE_APP_UpdateHealth(NowMs, true);
}

void CFS_CORE_APP_UpdateHealth(uint32 NowMs, bool ForcePublish)
{
    CFS_CORE_APP_SystemHealthTlm_t *Tlm;
    uint32                          LastValidInputTimestampMs;
    bool                            BridgeTimedOut;
    bool                            GpsUnavailable;
    bool                            EkfTimedOut;
    bool                            LocalTimedOut;
    bool                            AttitudeTimedOut;
    bool                            UplinkTimedOut;

    if (!ForcePublish && (NowMs - CFS_CORE_APP_Data.LastPublishTimeMs) < CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs)
    {
        return;
    }

    LastValidInputTimestampMs = 0;
    if (CFS_CORE_APP_Data.AttitudeState.Received && CFS_CORE_APP_Data.AttitudeState.TimestampMs > LastValidInputTimestampMs)
    {
        LastValidInputTimestampMs = CFS_CORE_APP_Data.AttitudeState.TimestampMs;
    }
    if (CFS_CORE_APP_Data.LocalState.Received && CFS_CORE_APP_Data.LocalState.TimestampMs > LastValidInputTimestampMs)
    {
        LastValidInputTimestampMs = CFS_CORE_APP_Data.LocalState.TimestampMs;
    }
    if (CFS_CORE_APP_Data.GpsState.Received && CFS_CORE_APP_Data.GpsState.TimestampMs > LastValidInputTimestampMs)
    {
        LastValidInputTimestampMs = CFS_CORE_APP_Data.GpsState.TimestampMs;
    }
    if (CFS_CORE_APP_Data.EkfState.Received && CFS_CORE_APP_Data.EkfState.TimestampMs > LastValidInputTimestampMs)
    {
        LastValidInputTimestampMs = CFS_CORE_APP_Data.EkfState.TimestampMs;
    }
    if (LastValidInputTimestampMs == 0)
    {
        LastValidInputTimestampMs = NowMs;
    }

    BridgeTimedOut   = !CFS_CORE_APP_Data.BridgeState.Received ||
                       (NowMs - CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs) > CFS_CORE_APP_Data.ActiveConfig.BridgeTimeoutMs;
    GpsUnavailable   = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.GpsState, NowMs, CFS_CORE_APP_Data.ActiveConfig.GpsTimeoutMs) ||
                       !CFS_CORE_APP_Data.GpsState.Valid || (CFS_CORE_APP_Data.GpsState.Stale != 0);
    EkfTimedOut      = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.EkfState, NowMs, CFS_CORE_APP_Data.ActiveConfig.EkfTimeoutMs) ||
                       !CFS_CORE_APP_Data.EkfState.Valid || (CFS_CORE_APP_Data.EkfState.Stale != 0);
    LocalTimedOut    = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.LocalState, NowMs, CFS_CORE_APP_Data.ActiveConfig.LocalTimeoutMs) ||
                       !CFS_CORE_APP_Data.LocalState.Valid || (CFS_CORE_APP_Data.LocalState.Stale != 0);
    AttitudeTimedOut = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.AttitudeState, NowMs, CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs) ||
                       !CFS_CORE_APP_Data.AttitudeState.Valid || (CFS_CORE_APP_Data.AttitudeState.Stale != 0);
    UplinkTimedOut   = !CFS_CORE_APP_Data.UplinkAppState.Received ||
                       (NowMs - CFS_CORE_APP_Data.UplinkAppState.LastHkRxMs) > CFS_CORE_APP_UPLINK_TIMEOUT_MS;

    Tlm = &CFS_CORE_APP_Data.SystemHealthTlm;
    CFS_CORE_APP_Data.LastPublishTimeMs = NowMs;
    CFS_CORE_APP_Data.PublishCount++;
    CFS_CORE_APP_Data.SequenceCounter++;

    memset(Tlm, 0, sizeof(*Tlm));
    CFE_MSG_Init(CFE_MSG_PTR(Tlm->TelemetryHeader), CFE_SB_ValueToMsgId(SYSTEM_HEALTH_MID), sizeof(*Tlm));
    Tlm->Seq                       = CFS_CORE_APP_Data.SequenceCounter;
    Tlm->TimestampMs               = NowMs;
    Tlm->LastValidInputTimestampMs = LastValidInputTimestampMs;

    if (BridgeTimedOut)
    {
        if (CFS_CORE_APP_Data.RecoveryStartMs == 0)
        {
            CFS_CORE_APP_Data.RecoveryStartMs     = NowMs;
            CFS_CORE_APP_Data.NextBridgeRestartMs = NowMs + CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS;
        }

        /* 재시작 인터벌 경과 + 최대 재시도 미초과 시 bridge app 재시작 시도 */
        if (NowMs >= CFS_CORE_APP_Data.NextBridgeRestartMs &&
            CFS_CORE_APP_Data.BridgeRestartCount < CFS_CORE_APP_BRIDGE_MAX_RESTARTS)
        {
            CFE_ES_AppId_t AppId;
            if (CFE_ES_GetAppIDByName(&AppId, CFS_CORE_APP_BRIDGE_APP_NAME) == CFE_SUCCESS)
            {
                CFE_ES_RestartApp(AppId);
                CFS_CORE_APP_Data.BridgeRestartCount++;
                CFS_CORE_APP_Data.NextBridgeRestartMs = NowMs + CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS;
                CFE_EVS_SendEvent(CFS_CORE_APP_BRIDGE_RESTART_EID, CFE_EVS_EventType_INFORMATION,
                                  "CFS_CORE_APP: bridge restart attempt=%u",
                                  (unsigned int)CFS_CORE_APP_Data.BridgeRestartCount);
            }
        }

        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT;
        Tlm->RecoveryRequested = 1;
        if ((NowMs - CFS_CORE_APP_Data.RecoveryStartMs) >= CFS_CORE_APP_FAILED_ESCALATION_MS)
        {
            Tlm->HealthState = CFS_CORE_APP_HEALTH_FAILED;
        }
        else
        {
            Tlm->HealthState = CFS_CORE_APP_HEALTH_RECOVERY;
        }
    }
    else if (EkfTimedOut)
    {
        CFS_CORE_APP_Data.RecoveryStartMs      = 0;
        CFS_CORE_APP_Data.BridgeRestartCount   = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs  = 0;
        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_EKF_INVALID;
        Tlm->RecoveryRequested = 0;
    }
    else if (LocalTimedOut)
    {
        CFS_CORE_APP_Data.RecoveryStartMs      = 0;
        CFS_CORE_APP_Data.BridgeRestartCount   = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs  = 0;
        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_LOCAL_TIMEOUT;
        Tlm->RecoveryRequested = 0;
    }
    else if (AttitudeTimedOut)
    {
        CFS_CORE_APP_Data.RecoveryStartMs      = 0;
        CFS_CORE_APP_Data.BridgeRestartCount   = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs  = 0;
        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT;
        Tlm->RecoveryRequested = 0;
    }
    else if (UplinkTimedOut)
    {
        CFS_CORE_APP_Data.RecoveryStartMs      = 0;
        CFS_CORE_APP_Data.BridgeRestartCount   = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs  = 0;
        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_UPLINK_TIMEOUT;
        Tlm->RecoveryRequested = 0;
    }
    /* GPS 가용성은 헬스를 저하시키지 않는다 (보고 전용) — 명세 §12.5.
       GpsUnavailable은 아래 GpsStatus.TimedOut 보고 필드용으로만 계산·사용한다. */
    else if (CFS_CORE_APP_Data.LastHealthState == CFS_CORE_APP_HEALTH_NOMINAL)
    {
        /* Already nominal: stay nominal immediately, no timer needed */
        CFS_CORE_APP_Data.RecoveryStartMs      = 0;
        CFS_CORE_APP_Data.BridgeRestartCount   = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs  = 0;
        CFS_CORE_APP_Data.NominalEligibleSince = 0;
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_NOMINAL;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_NONE;
        Tlm->RecoveryRequested = 0;
    }
    else
    {
        /* Recovering from non-nominal: require 10 s of consecutive clear conditions */
        CFS_CORE_APP_Data.RecoveryStartMs     = 0;
        CFS_CORE_APP_Data.BridgeRestartCount  = 0;
        CFS_CORE_APP_Data.NextBridgeRestartMs = 0;
        if (CFS_CORE_APP_Data.NominalEligibleSince == 0)
        {
            CFS_CORE_APP_Data.NominalEligibleSince = NowMs;
        }
        if ((NowMs - CFS_CORE_APP_Data.NominalEligibleSince) >= CFS_CORE_APP_NOMINAL_STABILITY_MS)
        {
            CFS_CORE_APP_Data.NominalEligibleSince = 0;
            Tlm->HealthState = CFS_CORE_APP_HEALTH_NOMINAL;
            Tlm->FaultCode   = CFS_CORE_APP_FAULT_NONE;
        }
        else
        {
            Tlm->HealthState = CFS_CORE_APP_HEALTH_DEGRADED;
            Tlm->FaultCode   = CFS_CORE_APP_FAULT_NONE;
        }
        Tlm->RecoveryRequested = 0;
    }

    Tlm->AttitudeStatus.Valid     = CFS_CORE_APP_Data.AttitudeState.Valid;
    Tlm->AttitudeStatus.Stale     = CFS_CORE_APP_Data.AttitudeState.Stale;
    Tlm->AttitudeStatus.ErrorCode = CFS_CORE_APP_Data.AttitudeState.ErrorCode;
    Tlm->AttitudeStatus.TimedOut  = (uint8)AttitudeTimedOut;

    Tlm->LocalStatus.Valid     = CFS_CORE_APP_Data.LocalState.Valid;
    Tlm->LocalStatus.Stale     = CFS_CORE_APP_Data.LocalState.Stale;
    Tlm->LocalStatus.ErrorCode = CFS_CORE_APP_Data.LocalState.ErrorCode;
    Tlm->LocalStatus.TimedOut  = (uint8)LocalTimedOut;

    Tlm->GpsStatus.Valid     = CFS_CORE_APP_Data.GpsState.Valid;
    Tlm->GpsStatus.Stale     = CFS_CORE_APP_Data.GpsState.Stale;
    Tlm->GpsStatus.ErrorCode = CFS_CORE_APP_Data.GpsState.ErrorCode;
    Tlm->GpsStatus.TimedOut  = (uint8)GpsUnavailable;

    Tlm->EkfStatus.Valid     = CFS_CORE_APP_Data.EkfState.Valid;
    Tlm->EkfStatus.Stale     = CFS_CORE_APP_Data.EkfState.Stale;
    Tlm->EkfStatus.ErrorCode = CFS_CORE_APP_Data.EkfState.ErrorCode;
    Tlm->EkfStatus.TimedOut  = (uint8)EkfTimedOut;

    Tlm->BridgeStatus.LinkState  = CFS_CORE_APP_Data.BridgeState.LinkState;
    Tlm->BridgeStatus.ErrorCode  = CFS_CORE_APP_Data.BridgeState.LastErrorCode;
    Tlm->BridgeStatus.TimedOut   = (uint8)BridgeTimedOut;

    Tlm->UplinkStatus.TimedOut   = (uint8)UplinkTimedOut;

    if (Tlm->HealthState != CFS_CORE_APP_Data.LastHealthState)
    {
        CFE_EVS_SendEvent(CFS_CORE_APP_HEALTH_TRANSITION_EID, CFE_EVS_EventType_INFORMATION,
                          "CFS_CORE_APP: health %u->%u fault=%u",
                          (unsigned int)CFS_CORE_APP_Data.LastHealthState,
                          (unsigned int)Tlm->HealthState,
                          (unsigned int)Tlm->FaultCode);
        CFS_CORE_APP_Data.LastHealthState = Tlm->HealthState;
        CFS_CORE_APP_SaveState();
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
}

static uint16 CFS_CORE_APP_ConfigChecksum(const CFS_CORE_APP_ConfigPayloadHdr_t *Hdr,
                                           const uint8 *ValueBytes, uint8 ValueLength)
{
    uint16 Sum = 0;
    uint8  i;

    Sum += (uint16)Hdr->ConfigScope;
    Sum += (uint16)Hdr->ConfigVersion;
    Sum += (uint16)(Hdr->ParameterId & 0xFFU);
    Sum += (uint16)((Hdr->ParameterId >> 8U) & 0xFFU);
    Sum += (uint16)Hdr->ValueType;
    Sum += (uint16)Hdr->ValueLength;
    for (i = 0; i < ValueLength; i++)
    {
        Sum += (uint16)ValueBytes[i];
    }
    return Sum;
}

void CFS_CORE_APP_ProcessConfigCommand(const CFS_CORE_APP_ConfigCmdTlm_t *Msg)
{
    const CFS_CORE_APP_ConfigPayloadHdr_t *Hdr;
    uint32                                 Value;
    CFS_CORE_APP_ConfigParams_t            Candidate;

    /* ── 1. 기본 포맷 검증 ── */
    if (Msg->PayloadLength < (uint8)sizeof(CFS_CORE_APP_ConfigPayloadHdr_t))
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_LENGTH;
        CFE_EVS_SendEvent(CFS_CORE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "CFS_CORE_APP: config payload too short len=%u",
                          (unsigned int)Msg->PayloadLength);
        return;
    }

    Hdr = (const CFS_CORE_APP_ConfigPayloadHdr_t *)Msg->Payload;

    if (Hdr->ConfigScope != CFS_CORE_APP_CONFIG_SCOPE)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_SCOPE;
        return;
    }

    if (Hdr->ConfigVersion != CFS_CORE_APP_CONFIG_VERSION)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_VERSION;
        return;
    }

    if (Hdr->ValueLength != sizeof(uint32) ||
        (uint8)(sizeof(*Hdr) + Hdr->ValueLength) > Msg->PayloadLength)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_LENGTH;
        return;
    }

    /* ── Checksum 검증 ── */
    {
        const uint8 *ValueBytes = Msg->Payload + sizeof(*Hdr);
        uint16       Expected   = CFS_CORE_APP_ConfigChecksum(Hdr, ValueBytes, Hdr->ValueLength);
        if (Hdr->Checksum != Expected)
        {
            CFS_CORE_APP_Data.ErrCounter++;
            CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
            CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_CHECKSUM;
            CFE_EVS_SendEvent(CFS_CORE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CFS_CORE_APP: config checksum mismatch got=0x%04X expected=0x%04X",
                              (unsigned int)Hdr->Checksum, (unsigned int)Expected);
            return;
        }
    }

    memcpy(&Value, Msg->Payload + sizeof(*Hdr), sizeof(Value));

    if (Value < CFS_CORE_APP_PARAM_MIN_MS || Value > CFS_CORE_APP_PARAM_MAX_MS)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_VALUE;
        return;
    }

    /* ── 2. PendingConfig에 새 값 기록 (ActiveConfig 기반으로 시작) ── */
    CFS_CORE_APP_Data.PendingConfig      = CFS_CORE_APP_Data.ActiveConfig;
    CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_PENDING;

    switch ((CFS_CORE_APP_ParamId_t)Hdr->ParameterId)
    {
        case CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS:
            CFS_CORE_APP_Data.PendingConfig.AttitudeTimeoutMs = Value;  break;
        case CFS_CORE_APP_PARAM_LOCAL_TIMEOUT_MS:
            CFS_CORE_APP_Data.PendingConfig.LocalTimeoutMs    = Value;  break;
        case CFS_CORE_APP_PARAM_GPS_TIMEOUT_MS:
            CFS_CORE_APP_Data.PendingConfig.GpsTimeoutMs      = Value;  break;
        case CFS_CORE_APP_PARAM_EKF_TIMEOUT_MS:
            CFS_CORE_APP_Data.PendingConfig.EkfTimeoutMs      = Value;  break;
        case CFS_CORE_APP_PARAM_BRIDGE_TIMEOUT_MS:
            CFS_CORE_APP_Data.PendingConfig.BridgeTimeoutMs   = Value;  break;
        case CFS_CORE_APP_PARAM_PUBLISH_PERIOD_MS:
            CFS_CORE_APP_Data.PendingConfig.PublishPeriodMs   = Value;  break;
        default:
            CFS_CORE_APP_Data.ErrCounter++;
            CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
            CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_PARAM;
            return;
    }

    /* ── 3. PendingConfig 전체 교차 검증 (상호 일관성) ── */
    Candidate = CFS_CORE_APP_Data.PendingConfig;
    if (Candidate.AttitudeTimeoutMs < CFS_CORE_APP_PARAM_MIN_MS ||
        Candidate.LocalTimeoutMs    < CFS_CORE_APP_PARAM_MIN_MS ||
        Candidate.GpsTimeoutMs      < CFS_CORE_APP_PARAM_MIN_MS ||
        Candidate.EkfTimeoutMs      < CFS_CORE_APP_PARAM_MIN_MS ||
        Candidate.BridgeTimeoutMs   < CFS_CORE_APP_PARAM_MIN_MS ||
        Candidate.PublishPeriodMs   < CFS_CORE_APP_PARAM_MIN_MS)
    {
        CFS_CORE_APP_Data.ErrCounter++;
        CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_REJECTED;
        CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_BAD_VALUE;
        return;
    }

    /* ── 4. PreviousConfig 백업 후 ActiveConfig에 활성화 ── */
    CFS_CORE_APP_Data.PreviousConfig     = CFS_CORE_APP_Data.ActiveConfig;
    CFS_CORE_APP_Data.ActiveConfig       = CFS_CORE_APP_Data.PendingConfig;
    CFS_CORE_APP_Data.ConfigGeneration++;
    CFS_CORE_APP_Data.ConfigPendingState = (uint8)CFS_CORE_APP_CONFIG_PENDING_IDLE;
    CFS_CORE_APP_Data.LastConfigResult   = (uint8)CFS_CORE_APP_CONFIG_RESULT_OK;
    CFS_CORE_APP_Data.CmdCounter++;

    CFE_EVS_SendEvent(CFS_CORE_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP: config activated param=%u value=%lu gen=%lu",
                      (unsigned int)Hdr->ParameterId, (unsigned long)Value,
                      (unsigned long)CFS_CORE_APP_Data.ConfigGeneration);
}

void CFS_CORE_APP_ProcessViewpointCommand(const CFS_CORE_APP_ViewpointCmdTlm_t *Msg)
{
    CFS_CORE_APP_Data.ViewpointCmd.TimestampMs    = Msg->TimestampMs;
    CFS_CORE_APP_Data.ViewpointCmd.SourceSequence = Msg->SourceSequence;
    CFS_CORE_APP_Data.ViewpointCmd.ViewpointType  = Msg->ViewpointType;
    CFS_CORE_APP_Data.ViewpointCmd.PositionFrame  = Msg->PositionFrame;
    CFS_CORE_APP_Data.ViewpointCmd.X              = Msg->X;
    CFS_CORE_APP_Data.ViewpointCmd.Y              = Msg->Y;
    CFS_CORE_APP_Data.ViewpointCmd.Z              = Msg->Z;
    CFS_CORE_APP_Data.ViewpointCmd.Yaw            = Msg->Yaw;
    CFS_CORE_APP_Data.ViewpointCmd.Pitch          = Msg->Pitch;
    CFS_CORE_APP_Data.ViewpointCmd.HoldTimeMs     = Msg->HoldTimeMs;
    CFS_CORE_APP_Data.ViewpointCmd.Valid          = true;

    CFE_EVS_SendEvent(CFS_CORE_APP_VIEWPOINT_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP: viewpoint cmd type=%u seq=%u x=%.1f y=%.1f z=%.1f hold=%ums",
                      (unsigned int)Msg->ViewpointType, (unsigned int)Msg->SourceSequence,
                      (double)Msg->X, (double)Msg->Y, (double)Msg->Z,
                      (unsigned int)Msg->HoldTimeMs);
}

void CFS_CORE_APP_LoadState(void)
{
    CFS_CORE_APP_PersistentState_t State;
    int                             Fd;
    ssize_t                         ReadRc;

    Fd = open(CFS_CORE_APP_STATE_FILE_PATH, O_RDONLY);
    if (Fd < 0)
    {
        return;
    }

    ReadRc = read(Fd, &State, sizeof(State));
    close(Fd);

    if (ReadRc != (ssize_t)sizeof(State))
    {
        return;
    }
    if (State.Magic != CFS_CORE_APP_STATE_MAGIC)
    {
        return;
    }
    if (State.Checksum != (State.Magic + (uint32)State.LastHealthState))
    {
        return;
    }

    CFS_CORE_APP_Data.LastHealthState = State.LastHealthState;

    CFE_EVS_SendEvent(CFS_CORE_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP: restored health state=%u",
                      (unsigned int)State.LastHealthState);
}

void CFS_CORE_APP_SaveState(void)
{
    CFS_CORE_APP_PersistentState_t State;
    char                            TmpPath[sizeof(CFS_CORE_APP_STATE_FILE_PATH) + 4];
    int                             Fd;

    State.Magic           = CFS_CORE_APP_STATE_MAGIC;
    State.LastHealthState = CFS_CORE_APP_Data.LastHealthState;
    State.Reserved[0]     = 0;
    State.Reserved[1]     = 0;
    State.Reserved[2]     = 0;
    State.Checksum        = State.Magic + (uint32)State.LastHealthState;

    snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", CFS_CORE_APP_STATE_FILE_PATH);

    Fd = open(TmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (Fd < 0)
    {
        return;
    }

    if (write(Fd, &State, sizeof(State)) != (ssize_t)sizeof(State))
    {
        close(Fd);
        return;
    }

    close(Fd);
    rename(TmpPath, CFS_CORE_APP_STATE_FILE_PATH);
}

void CFS_CORE_APP_ServicePrototype(void)
{
    CFS_CORE_APP_UpdateHealth(CFS_CORE_APP_GetTimeMs(), false);
}

void CFS_CORE_APP_ProcessRecoveryCommand(const CFS_CORE_APP_RecoveryCmdTlm_t *Msg)
{
    CFS_CORE_APP_Data.RecoveryStartMs    = 0;
    CFS_CORE_APP_Data.BridgeRestartCount = 0;
    CFS_CORE_APP_Data.RecoveryRequestedCount++;
    CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested = 1;
    CFS_CORE_APP_Data.CmdCounter++;

    CFE_EVS_SendEvent(CFS_CORE_APP_RECOVERY_CMD_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP: recovery cmd seq=%u count=%lu",
                      (unsigned int)Msg->SourceSequence,
                      (unsigned long)CFS_CORE_APP_Data.RecoveryRequestedCount);
}

void CFS_CORE_APP_ProcessModeCommand(const CFS_CORE_APP_ModeCmdTlm_t *Msg)
{
    if (Msg->PayloadLength >= 1)
    {
        CFS_CORE_APP_Data.LastModeValue = Msg->Payload[0];
    }
    CFS_CORE_APP_Data.CmdCounter++;

    CFE_EVS_SendEvent(CFS_CORE_APP_MODE_CMD_EID, CFE_EVS_EventType_INFORMATION,
                      "CFS_CORE_APP: mode cmd seq=%u mode=%u",
                      (unsigned int)Msg->SourceSequence,
                      (unsigned int)CFS_CORE_APP_Data.LastModeValue);
}


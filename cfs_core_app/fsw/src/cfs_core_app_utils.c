#include "cfs_core_app_utils.h"
#include "cfs_core_app_eventids.h"

#include <stdio.h>
#include <string.h>

static uint32 CFS_CORE_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

static void CFS_CORE_APP_UpdateStateCache(CFS_CORE_APP_StateCache_t *Cache, const CFS_CORE_APP_GenericStateTlm_t *Msg)
{
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

void CFS_CORE_APP_ReportHousekeeping(void)
{
    CFS_CORE_APP_Data.HkTlm.CommandCounter       = CFS_CORE_APP_Data.CmdCounter;
    CFS_CORE_APP_Data.HkTlm.CommandErrorCounter  = CFS_CORE_APP_Data.ErrCounter;
    CFS_CORE_APP_Data.HkTlm.PublishCount         = CFS_CORE_APP_Data.PublishCount;
    CFS_CORE_APP_Data.HkTlm.LastPublishTimestampMs = CFS_CORE_APP_Data.LastPublishTimeMs;

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
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.AttitudeState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.LocalState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.GpsState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr);
    }
    else if (CFE_SB_MsgIdToValue(MsgId) == CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE)
    {
        CFS_CORE_APP_UpdateStateCache(&CFS_CORE_APP_Data.EkfState, (const CFS_CORE_APP_GenericStateTlm_t *)MsgPtr);
    }

    NowMs = CFS_CORE_APP_GetTimeMs();
    CFS_CORE_APP_UpdateHealth(NowMs, true);
}

void CFS_CORE_APP_UpdateHealth(uint32 NowMs, bool ForcePublish)
{
    CFS_CORE_APP_SystemHealthTlm_t *Tlm;
    uint32                          LastValidInputTimestampMs;
    bool                            BridgeTimedOut;
    bool                            GpsUnavailable;
    bool                            EkfUnavailable;

    if (!ForcePublish && (NowMs - CFS_CORE_APP_Data.LastPublishTimeMs) < CFS_CORE_APP_PROTOTYPE_PERIOD_MS)
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

    BridgeTimedOut = !CFS_CORE_APP_Data.BridgeState.Received ||
                     (NowMs - CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs) > CFS_CORE_APP_BRIDGE_TIMEOUT_MS;
    GpsUnavailable = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.GpsState, NowMs, CFS_CORE_APP_GPS_TIMEOUT_MS) ||
                     !CFS_CORE_APP_Data.GpsState.Valid || (CFS_CORE_APP_Data.GpsState.Stale != 0);
    EkfUnavailable = CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.EkfState, NowMs, CFS_CORE_APP_EKF_TIMEOUT_MS) ||
                     CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.LocalState, NowMs, CFS_CORE_APP_LOCAL_TIMEOUT_MS) ||
                     CFS_CORE_APP_StateExpired(&CFS_CORE_APP_Data.AttitudeState, NowMs, CFS_CORE_APP_ATTITUDE_TIMEOUT_MS) ||
                     !CFS_CORE_APP_Data.EkfState.Valid || (CFS_CORE_APP_Data.EkfState.Stale != 0) ||
                     !CFS_CORE_APP_Data.LocalState.Valid || (CFS_CORE_APP_Data.LocalState.Stale != 0) ||
                     !CFS_CORE_APP_Data.AttitudeState.Valid || (CFS_CORE_APP_Data.AttitudeState.Stale != 0);

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
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_RECOVERY;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT;
        Tlm->RecoveryRequested = 1;
    }
    else if (EkfUnavailable)
    {
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_EKF_INVALID;
        Tlm->RecoveryRequested = 0;
    }
    else if (GpsUnavailable)
    {
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_DEGRADED;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_GPS_STALE;
        Tlm->RecoveryRequested = 0;
    }
    else
    {
        Tlm->HealthState       = CFS_CORE_APP_HEALTH_NOMINAL;
        Tlm->FaultCode         = CFS_CORE_APP_FAULT_NONE;
        Tlm->RecoveryRequested = 0;
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
}

void CFS_CORE_APP_ServicePrototype(void)
{
    CFS_CORE_APP_UpdateHealth(CFS_CORE_APP_GetTimeMs(), false);
}


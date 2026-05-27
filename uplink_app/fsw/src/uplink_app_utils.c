#include "uplink_app_utils.h"
#include "uplink_app_eventids.h"

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define UPLINK_APP_STATE_MAGIC 0x55504C4BU  /* "UPLK" */

typedef struct
{
    uint32 Magic;
    uint32 LastAcceptedSequence;
    uint32 Checksum;
} UPLINK_APP_PersistentState_t;

static uint32 UPLINK_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

void UPLINK_APP_UpdateStatusTelemetry(uint32 NowMs)
{
    UPLINK_APP_StatusTlm_t *Tlm;

    if (NowMs == 0)
    {
        NowMs = UPLINK_APP_GetTimeMs();
    }

    Tlm = &UPLINK_APP_Data.StatusTlm;

    UPLINK_APP_Data.LastPublishTimeMs = NowMs;
    UPLINK_APP_Data.PublishCount++;
    UPLINK_APP_Data.SequenceCounter++;

    memset(Tlm, 0, sizeof(*Tlm));
    CFE_MSG_Init(CFE_MSG_PTR(Tlm->TelemetryHeader), CFE_SB_ValueToMsgId(UPLINK_STATUS_MID), sizeof(*Tlm));
    Tlm->Seq                 = UPLINK_APP_Data.SequenceCounter;
    Tlm->TimestampMs         = NowMs;
    Tlm->LastRxTimestampMs   = UPLINK_APP_Data.LastRxTimeMs;
    Tlm->AcceptedCount       = UPLINK_APP_Data.AcceptedCount;
    Tlm->RejectedCount       = UPLINK_APP_Data.RejectedCount;
    Tlm->RoutingFailureCount = UPLINK_APP_Data.RoutingFailureCount;
    Tlm->LastCommandCode     = UPLINK_APP_Data.LastCommandCode;
    Tlm->LastCommandSequence = UPLINK_APP_Data.LastRxSequence;
    Tlm->LastCommandResult   = UPLINK_APP_Data.LastCommandResult;
    Tlm->LinkState           = UPLINK_APP_Data.LinkState;
    Tlm->Valid               = UPLINK_APP_Data.Valid;
    Tlm->ActiveTransportId   = UPLINK_APP_Data.ActiveTransportId;
    Tlm->LastRouteTarget     = UPLINK_APP_Data.LastRouteTarget;
    Tlm->ConfigPendingState  = UPLINK_APP_Data.ConfigPendingState;
    Tlm->LastConfigResult    = UPLINK_APP_Data.LastConfigResult;
    Tlm->LastRollbackReason  = UPLINK_APP_Data.LastRollbackReason;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
}

void UPLINK_APP_ReportHousekeeping(void)
{
    UPLINK_APP_Data.HkTlm.CommandCounter       = UPLINK_APP_Data.CmdCounter;
    UPLINK_APP_Data.HkTlm.CommandErrorCounter  = UPLINK_APP_Data.ErrCounter;
    UPLINK_APP_Data.HkTlm.PublishCount         = UPLINK_APP_Data.PublishCount;
    UPLINK_APP_Data.HkTlm.LastPublishTimestampMs = UPLINK_APP_Data.LastPublishTimeMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(UPLINK_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(UPLINK_APP_Data.HkTlm.TelemetryHeader), true);
}

bool UPLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        UPLINK_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: Invalid cmd length expected=%lu actual=%lu",
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }

    return true;
}

bool UPLINK_APP_ValidateProxyCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_Result_t *Result)
{
    if (Cmd->Version != UPLINK_APP_PROTOCOL_VERSION)
    {
        *Result = UPLINK_APP_RESULT_REJECT_VERSION;
        return false;
    }

    if (Cmd->PayloadLength > UPLINK_APP_MAX_PAYLOAD_LENGTH)
    {
        *Result = UPLINK_APP_RESULT_REJECT_LENGTH;
        return false;
    }

    switch (Cmd->CommandClass)
    {
        case UPLINK_APP_CLASS_CONFIG:
        case UPLINK_APP_CLASS_ROUTE_UPDATE:
        case UPLINK_APP_CLASS_VIEWPOINT:
        case UPLINK_APP_CLASS_RECOVERY:
        case UPLINK_APP_CLASS_MODE:
        case UPLINK_APP_CLASS_DIAGNOSTIC:
            break;

        default:
            *Result = UPLINK_APP_RESULT_REJECT_CLASS;
            return false;
    }

    if ((Cmd->CommandClass == UPLINK_APP_CLASS_ROUTE_UPDATE ||
         Cmd->CommandClass == UPLINK_APP_CLASS_VIEWPOINT) &&
        Cmd->PayloadLength == 0)
    {
        *Result = UPLINK_APP_RESULT_REJECT_LENGTH;
        return false;
    }

    *Result = UPLINK_APP_RESULT_ACCEPT;
    return true;
}

static bool UPLINK_APP_IsWaypointFinite(const UPLINK_APP_Waypoint_t *Waypoint)
{
    return isfinite(Waypoint->X) && isfinite(Waypoint->Y) && isfinite(Waypoint->Z);
}

static bool UPLINK_APP_IsWaypointInFlyableArea(const UPLINK_APP_Waypoint_t *Waypoint)
{
    if (Waypoint->X < UPLINK_APP_ROUTE_FLYABLE_X_MIN_M || Waypoint->X > UPLINK_APP_ROUTE_FLYABLE_X_MAX_M)
    {
        return false;
    }

    if (Waypoint->Y < UPLINK_APP_ROUTE_FLYABLE_Y_MIN_M || Waypoint->Y > UPLINK_APP_ROUTE_FLYABLE_Y_MAX_M)
    {
        return false;
    }

    if (Waypoint->Z < UPLINK_APP_ROUTE_ALTITUDE_MIN_M || Waypoint->Z > UPLINK_APP_ROUTE_ALTITUDE_MAX_M)
    {
        return false;
    }

    return true;
}

static bool UPLINK_APP_IsWaypointInNoFlyArea(const UPLINK_APP_Waypoint_t *Waypoint)
{
#if UPLINK_APP_ROUTE_NOFLY_ENABLE
    return (Waypoint->X >= UPLINK_APP_ROUTE_NOFLY_X_MIN_M && Waypoint->X <= UPLINK_APP_ROUTE_NOFLY_X_MAX_M &&
            Waypoint->Y >= UPLINK_APP_ROUTE_NOFLY_Y_MIN_M && Waypoint->Y <= UPLINK_APP_ROUTE_NOFLY_Y_MAX_M);
#else
    (void)Waypoint;
    return false;
#endif
}

static float UPLINK_APP_GetWaypointDistance(const UPLINK_APP_Waypoint_t *First, const UPLINK_APP_Waypoint_t *Second)
{
    const float Dx = Second->X - First->X;
    const float Dy = Second->Y - First->Y;
    const float Dz = Second->Z - First->Z;

    return sqrtf((Dx * Dx) + (Dy * Dy) + (Dz * Dz));
}

static bool UPLINK_APP_IsWaypointSegmentDistanceValid(const UPLINK_APP_Waypoint_t *First,
                                                      const UPLINK_APP_Waypoint_t *Second)
{
    const float SegmentDistance = UPLINK_APP_GetWaypointDistance(First, Second);

    return fabsf(SegmentDistance - UPLINK_APP_ROUTE_SEGMENT_DIST_M) <= UPLINK_APP_ROUTE_SEGMENT_DIST_TOL_M;
}

bool UPLINK_APP_ParseRouteUpdatePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_RouteUpdatePayload_t *Payload)
{
    size_t ExpectedLength;
    uint32 Index;

    memset(Payload, 0, sizeof(*Payload));

    if (Cmd->PayloadLength < 4U)
    {
        return false;
    }

    if ((size_t)Cmd->PayloadLength > sizeof(*Payload))
    {
        return false;
    }

    memcpy(Payload, Cmd->Payload, Cmd->PayloadLength);

    if ((Payload->RouteType != UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_SEGMENT_LANDING))
    {
        return false;
    }

    if ((Payload->WaypointCount == 0U) || (Payload->WaypointCount > UPLINK_APP_ROUTE_MAX_WAYPOINTS))
    {
        return false;
    }

    ExpectedLength = 4U + ((size_t)Payload->WaypointCount * sizeof(UPLINK_APP_Waypoint_t));
    if ((size_t)Cmd->PayloadLength != ExpectedLength)
    {
        return false;
    }

    for (Index = 0; Index < Payload->WaypointCount; ++Index)
    {
        const UPLINK_APP_Waypoint_t *Waypoint = &Payload->Waypoints[Index];

        if (!UPLINK_APP_IsWaypointFinite(Waypoint))
        {
            return false;
        }

        if (!UPLINK_APP_IsWaypointInFlyableArea(Waypoint))
        {
            return false;
        }

        if (UPLINK_APP_IsWaypointInNoFlyArea(Waypoint))
        {
            return false;
        }

        if (Index > 0U)
        {
            if (!UPLINK_APP_IsWaypointSegmentDistanceValid(&Payload->Waypoints[Index - 1U], Waypoint))
            {
                return false;
            }
        }
    }

    return true;
}

bool UPLINK_APP_PublishRouteUpdate(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, const UPLINK_APP_RouteUpdatePayload_t *Payload)
{
    UPLINK_APP_RouteUpdateTlm_t RouteUpdate;

    memset(&RouteUpdate, 0, sizeof(RouteUpdate));
    CFE_MSG_Init(CFE_MSG_PTR(RouteUpdate.TelemetryHeader), CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID), sizeof(RouteUpdate));

    RouteUpdate.Seq           = UPLINK_APP_Data.SequenceCounter + 1U;
    RouteUpdate.TimestampMs   = UPLINK_APP_Data.LastRxTimeMs;
    RouteUpdate.SourceSequence = Cmd->Sequence;
    RouteUpdate.RouteType     = Payload->RouteType;
    RouteUpdate.RouteVersion  = Payload->RouteVersion;
    RouteUpdate.WaypointCount = Payload->WaypointCount;
    memcpy(RouteUpdate.Waypoints, Payload->Waypoints, sizeof(RouteUpdate.Waypoints));

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(RouteUpdate.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(RouteUpdate.TelemetryHeader), true) == CFE_SUCCESS);
}

bool UPLINK_APP_ForwardRecoveryCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_RecoveryCmdTlm_t RecoveryTlm;

    memset(&RecoveryTlm, 0, sizeof(RecoveryTlm));
    CFE_MSG_Init(CFE_MSG_PTR(RecoveryTlm.TelemetryHeader), CFE_SB_ValueToMsgId(RECOVERY_CMD_MID),
                 sizeof(RecoveryTlm));

    RecoveryTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    RecoveryTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    RecoveryTlm.SourceSequence = Cmd->Sequence;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(RecoveryTlm.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(RecoveryTlm.TelemetryHeader), true) == CFE_SUCCESS);
}

bool UPLINK_APP_ForwardConfigCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_ConfigCmdTlm_t ConfigTlm;
    bool                      Ok;

    UPLINK_APP_Data.ConfigPendingState = UPLINK_APP_CONFIG_PENDING;

    memset(&ConfigTlm, 0, sizeof(ConfigTlm));
    CFE_MSG_Init(CFE_MSG_PTR(ConfigTlm.TelemetryHeader), CFE_SB_ValueToMsgId(CONFIG_CMD_MID),
                 sizeof(ConfigTlm));

    ConfigTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    ConfigTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    ConfigTlm.SourceSequence = Cmd->Sequence;
    ConfigTlm.PayloadLength  = Cmd->PayloadLength;
    memcpy(ConfigTlm.Payload, Cmd->Payload, Cmd->PayloadLength);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ConfigTlm.TelemetryHeader));
    Ok = (CFE_SB_TransmitMsg(CFE_MSG_PTR(ConfigTlm.TelemetryHeader), true) == CFE_SUCCESS);

    UPLINK_APP_Data.ConfigPendingState = Ok ? UPLINK_APP_CONFIG_IDLE : UPLINK_APP_CONFIG_REJECTED;
    UPLINK_APP_Data.LastConfigResult   = (uint8)(!Ok);
    return Ok;
}

void UPLINK_APP_LoadState(void)
{
    UPLINK_APP_PersistentState_t State;
    int                          Fd;
    ssize_t                      ReadRc;

    Fd = open(UPLINK_APP_STATE_FILE_PATH, O_RDONLY);
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
    if (State.Magic != UPLINK_APP_STATE_MAGIC)
    {
        return;
    }
    if (State.Checksum != (State.Magic + State.LastAcceptedSequence))
    {
        return;
    }

    UPLINK_APP_Data.LastAcceptedSequence = State.LastAcceptedSequence;
    UPLINK_APP_Data.AcceptedCount        = 1;

    CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: restored persistent state seq=%lu",
                      (unsigned long)State.LastAcceptedSequence);
}

void UPLINK_APP_SaveState(void)
{
    UPLINK_APP_PersistentState_t State;
    int                          Fd;
    char                         TmpPath[sizeof(UPLINK_APP_STATE_FILE_PATH) + 4];

    State.Magic                = UPLINK_APP_STATE_MAGIC;
    State.LastAcceptedSequence = UPLINK_APP_Data.LastAcceptedSequence;
    State.Checksum             = State.Magic + State.LastAcceptedSequence;

    snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", UPLINK_APP_STATE_FILE_PATH);

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
    rename(TmpPath, UPLINK_APP_STATE_FILE_PATH);
}

bool UPLINK_APP_ForwardViewpointCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_ViewpointCmdTlm_t ViewpointTlm;

    memset(&ViewpointTlm, 0, sizeof(ViewpointTlm));
    CFE_MSG_Init(CFE_MSG_PTR(ViewpointTlm.TelemetryHeader), CFE_SB_ValueToMsgId(VIEWPOINT_CMD_MID),
                 sizeof(ViewpointTlm));

    ViewpointTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    ViewpointTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    ViewpointTlm.SourceSequence = Cmd->Sequence;
    ViewpointTlm.PayloadLength  = Cmd->PayloadLength;
    memcpy(ViewpointTlm.Payload, Cmd->Payload, Cmd->PayloadLength);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ViewpointTlm.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(ViewpointTlm.TelemetryHeader), true) == CFE_SUCCESS);
}

UPLINK_APP_RouteTarget_t UPLINK_APP_ResolveRouteTarget(uint8 CommandClass)
{
    switch (CommandClass)
    {
        case UPLINK_APP_CLASS_CONFIG:
        case UPLINK_APP_CLASS_ROUTE_UPDATE:
        case UPLINK_APP_CLASS_VIEWPOINT:
        case UPLINK_APP_CLASS_RECOVERY:
        case UPLINK_APP_CLASS_MODE:
            return UPLINK_APP_ROUTE_CORE;

        case UPLINK_APP_CLASS_DIAGNOSTIC:
            return UPLINK_APP_ROUTE_DOWNLINK;

        default:
            return UPLINK_APP_ROUTE_NONE;
    }
}

void UPLINK_APP_ServicePrototype(void)
{
    uint32                NowMs;

    NowMs = UPLINK_APP_GetTimeMs();
    if ((NowMs - UPLINK_APP_Data.LastPublishTimeMs) < UPLINK_APP_PROTOTYPE_PERIOD_MS)
    {
        return;
    }
    UPLINK_APP_UpdateStatusTelemetry(NowMs);
}


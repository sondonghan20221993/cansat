#include "uplink_app_utils.h"
#include "uplink_app_eventids.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static uint16 UPLINK_APP_CRC16(const uint8 *Data, uint16 Len);

#define UPLINK_APP_STATE_MAGIC 0x55504C4BU  /* "UPLK" */

typedef struct
{
    uint32 Magic;
    uint32 LastAcceptedSequence;
    uint32 BootCount; /* BL-12: 8비트 값만 쓰지만 구조체는 uint32로 통일(다른 필드와 정렬) */
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
    Tlm->FcMissionResult             = UPLINK_APP_Data.FcMissionResult;
    Tlm->FcMissionUploadState        = UPLINK_APP_Data.FcMissionUploadState;
    Tlm->FcMissionUploadSuccessCount = UPLINK_APP_Data.FcMissionUploadSuccessCount;
    Tlm->BootCount                   = UPLINK_APP_Data.BootCount;
    Tlm->LastAcceptedSequence        = (uint16)UPLINK_APP_Data.LastAcceptedSequence;

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

uint16 UPLINK_APP_ComputeProxyCrc(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    uint8  Buf[6 + UPLINK_APP_MAX_PAYLOAD_LENGTH];
    uint16 Len;

    Buf[0] = Cmd->Version;
    Buf[1] = Cmd->CommandClass;
    Buf[2] = Cmd->PayloadLength;
    Buf[3] = Cmd->Flags;
    Buf[4] = (uint8)(Cmd->Sequence & 0xFFU);
    Buf[5] = (uint8)(Cmd->Sequence >> 8);
    Len    = (uint16)(6U + Cmd->PayloadLength);
    if (Len > (uint16)sizeof(Buf))
    {
        Len = (uint16)sizeof(Buf);
    }
    memcpy(&Buf[6], Cmd->Payload, (size_t)(Len - 6U));

    return UPLINK_APP_CRC16(Buf, Len);
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
        case UPLINK_APP_CLASS_COUNTER_MGMT:
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

    if (UPLINK_APP_ComputeProxyCrc(Cmd) != Cmd->Checksum)
    {
        *Result = UPLINK_APP_RESULT_REJECT_CHECKSUM;
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

    if ((Payload->RouteType != UPLINK_APP_ROUTE_OP_REPLACE) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_OP_APPEND) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_OP_DELETE))
    {
        return false;
    }

    if (Payload->WaypointCount == 0U)
    {
        return false;
    }

    /* DELETE: WaypointCount = number to remove from end; no waypoint payload. */
    if (Payload->RouteType == UPLINK_APP_ROUTE_OP_DELETE)
    {
        if (Payload->WaypointCount > UPLINK_APP_ROUTE_MAX_WAYPOINTS)
        {
            return false;
        }
        if ((size_t)Cmd->PayloadLength != 4U)
        {
            return false;
        }
        return true;
    }

    if (Payload->WaypointCount > UPLINK_APP_ROUTE_MAX_WAYPOINTS)
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

    if (Cmd->PayloadLength >= 8U)
    {
        RecoveryTlm.RecoveryAction   = Cmd->Payload[0];
        RecoveryTlm.TargetComponent  = Cmd->Payload[1];
        RecoveryTlm.ReasonCode       = (uint16)Cmd->Payload[2] | ((uint16)Cmd->Payload[3] << 8);
        RecoveryTlm.RequestToken     = (uint32)Cmd->Payload[4] | ((uint32)Cmd->Payload[5] << 8) |
                                       ((uint32)Cmd->Payload[6] << 16) | ((uint32)Cmd->Payload[7] << 24);
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(RecoveryTlm.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(RecoveryTlm.TelemetryHeader), true) == CFE_SUCCESS);
}

/* counter management(§18.4.6.7, 2026-07-22): payload = counter_scope(1) +
 * counter_action(1) + request_token(4). scope=UPLINK(자신)는 로컬 카운터를
 * 직접 초기화하고, 그 외는 대상 앱의 기존 CMD_MID에 RESET_COUNTERS CC를
 * CFE_MSG_SetFcnCode로 얹어 직접 전송한다(cfs_core_app 경유 없음,
 * P1-a의 CFS_CORE_APP_SendBridgeCtrlCmd와 동일 패턴). */
bool UPLINK_APP_ForwardCounterMgmtCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    uint8              Scope;
    uint8              Action;
    CFE_SB_MsgId_t     TargetMid;
    CFE_MSG_CommandHeader_t CmdHdr;

    if (Cmd->PayloadLength < 6U)
    {
        return false;
    }

    Scope  = Cmd->Payload[0];
    Action = Cmd->Payload[1];

    if (Action != 0U)
    {
        return false;
    }

    switch (Scope)
    {
        case UPLINK_APP_COUNTER_SCOPE_UPLINK:
            UPLINK_APP_Data.CmdCounter = 0;
            UPLINK_APP_Data.ErrCounter = 0;
            return true;

        case UPLINK_APP_COUNTER_SCOPE_MAVLINK_BRIDGE:
            TargetMid = CFE_SB_ValueToMsgId(UPLINK_APP_COUNTER_TARGET_MAVLINK_BRIDGE_CMD_MID);
            break;

        case UPLINK_APP_COUNTER_SCOPE_CFS_CORE:
            TargetMid = CFE_SB_ValueToMsgId(UPLINK_APP_COUNTER_TARGET_CFS_CORE_CMD_MID);
            break;

        case UPLINK_APP_COUNTER_SCOPE_LORA_TDM:
            TargetMid = CFE_SB_ValueToMsgId(UPLINK_APP_COUNTER_TARGET_LORA_TDM_CMD_MID);
            break;

        default:
            return false;
    }

    memset(&CmdHdr, 0, sizeof(CmdHdr));
    CFE_MSG_Init(CFE_MSG_PTR(CmdHdr), TargetMid, sizeof(CmdHdr));
    if (CFE_MSG_SetFcnCode(CFE_MSG_PTR(CmdHdr), UPLINK_APP_COUNTER_TARGET_RESET_COUNTERS_CC) != CFE_SUCCESS)
    {
        return false;
    }

    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(CmdHdr), true) == CFE_SUCCESS);
}

static uint16 UPLINK_APP_ConfigChecksum(const UPLINK_APP_ConfigPayloadHdr_t *Hdr,
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

bool UPLINK_APP_ForwardConfigCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_ConfigCmdTlm_t       ConfigTlm;
    const UPLINK_APP_ConfigPayloadHdr_t *Hdr;
    const uint8                    *ValueBytes;
    uint16                          Expected;
    bool                           Ok;

    if (Cmd->PayloadLength < (uint8)sizeof(UPLINK_APP_ConfigPayloadHdr_t))
    {
        UPLINK_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: config payload too short len=%u",
                          (unsigned int)Cmd->PayloadLength);
        return false;
    }

    Hdr = (const UPLINK_APP_ConfigPayloadHdr_t *)Cmd->Payload;

    if (Hdr->ValueLength != sizeof(uint32) ||
        (uint8)(sizeof(*Hdr) + Hdr->ValueLength) > Cmd->PayloadLength)
    {
        UPLINK_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: config checksum validation error: invalid length payload_len=%u hdr_len=%u val_len=%u",
                          (unsigned int)Cmd->PayloadLength, (unsigned int)sizeof(*Hdr), (unsigned int)Hdr->ValueLength);
        return false;
    }

    ValueBytes = Cmd->Payload + sizeof(*Hdr);
    Expected   = UPLINK_APP_ConfigChecksum(Hdr, ValueBytes, Hdr->ValueLength);
    if (Hdr->Checksum != Expected)
    {
        UPLINK_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: config checksum mismatch got=0x%04X expected=0x%04X",
                          (unsigned int)Hdr->Checksum, (unsigned int)Expected);
        return false;
    }

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

bool UPLINK_APP_ForwardModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_ModeCmdTlm_t ModeTlm;

    memset(&ModeTlm, 0, sizeof(ModeTlm));
    CFE_MSG_Init(CFE_MSG_PTR(ModeTlm.TelemetryHeader), CFE_SB_ValueToMsgId(MODE_CMD_MID),
                 sizeof(ModeTlm));

    ModeTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    ModeTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    ModeTlm.SourceSequence = Cmd->Sequence;

    if (Cmd->PayloadLength >= 6U)
    {
        ModeTlm.ModeAction     = Cmd->Payload[0];
        ModeTlm.RequestedState = Cmd->Payload[1];
        ModeTlm.RequestToken   = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                                 ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ModeTlm.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(ModeTlm.TelemetryHeader), true) == CFE_SUCCESS);
}

bool UPLINK_APP_ForwardDiagnosticCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UPLINK_APP_DiagnosticCmdTlm_t DiagTlm;

    memset(&DiagTlm, 0, sizeof(DiagTlm));
    CFE_MSG_Init(CFE_MSG_PTR(DiagTlm.TelemetryHeader), CFE_SB_ValueToMsgId(DIAGNOSTIC_CMD_MID),
                 sizeof(DiagTlm));

    DiagTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    DiagTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    DiagTlm.SourceSequence = Cmd->Sequence;

    if (Cmd->PayloadLength >= 6U)
    {
        DiagTlm.DiagAction   = Cmd->Payload[0];
        DiagTlm.DiagTarget   = Cmd->Payload[1];
        DiagTlm.RequestToken = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                               ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(DiagTlm.TelemetryHeader));
    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(DiagTlm.TelemetryHeader), true) == CFE_SUCCESS);
}

/* BL-17(2026-07-22) 커버리지 갭 해소: 테스트 환경(PTY mock 등)에서 상태
 * 파일 경로를 주입할 수 있도록 env var를 우선 확인한다. 미설정 시 기존
 * 컴파일타임 경로 사용(실환경 동작 불변) — lora_tdm_app/mavlink_bridge_app의
 * 시리얼 경로 주입과 동일 패턴. */
static const char *UPLINK_APP_GetStateFilePath(void)
{
    const char *EnvPath = getenv("UPLINK_APP_STATE_FILE_PATH");

    if (EnvPath != NULL && EnvPath[0] != '\0')
    {
        return EnvPath;
    }
    return UPLINK_APP_STATE_FILE_PATH;
}

void UPLINK_APP_LoadState(void)
{
    UPLINK_APP_PersistentState_t State;
    int                          Fd;
    ssize_t                      ReadRc;
    const char                  *StatePath = UPLINK_APP_GetStateFilePath();

    Fd = open(StatePath, O_RDONLY);
    if (Fd < 0)
    {
        /* BL-17(2026-07-22): ENOENT(파일 없음)=첫 부팅, 정상 상태라 조용히
         * 반환한다. 그 외(권한 오류 등)는 이례적이라 상태 손상과 동일하게
         * 취급해 아래 corrupt 이벤트로 보고한다. */
        if (errno != ENOENT)
        {
            CFE_EVS_SendEvent(UPLINK_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: state file open failed errno=%d, using defaults", errno);
        }
        return;
    }

    ReadRc = read(Fd, &State, sizeof(State));
    close(Fd);

    if (ReadRc != (ssize_t)sizeof(State))
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state file truncated (rc=%ld), using defaults", (long)ReadRc);
        return;
    }
    if (State.Magic != UPLINK_APP_STATE_MAGIC)
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state file bad magic (0x%08lX), using defaults",
                          (unsigned long)State.Magic);
        return;
    }
    if (State.Checksum != (State.Magic + State.LastAcceptedSequence + State.BootCount))
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state file checksum mismatch, using defaults");
        return;
    }

    UPLINK_APP_Data.LastAcceptedSequence = State.LastAcceptedSequence;
    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.BootCount            = (uint8)State.BootCount;

    CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: restored persistent state seq=%lu boot_count=%u",
                      (unsigned long)State.LastAcceptedSequence, (unsigned int)UPLINK_APP_Data.BootCount);
}

void UPLINK_APP_IncrementBootCount(void)
{
    /* BL-12(2026-07-21): LoadState() 이후 호출 — 복원된 값(또는 첫 부팅
     * 0)에서 +1(uint8 wrap), 즉시 SaveState()로 영속화해 이번 부팅
     * 세션의 boot_count를 첫 다운링크부터 반영할 수 있게 한다. */
    UPLINK_APP_Data.BootCount++;
    UPLINK_APP_SaveState();
}

void UPLINK_APP_SaveState(void)
{
    UPLINK_APP_PersistentState_t State;
    int                          Fd;
    int                          DirFd;
    char                         TmpPath[256];
    char                         DirPath[256];
    char                        *Slash;
    const char                  *StatePath = UPLINK_APP_GetStateFilePath();

    State.Magic                = UPLINK_APP_STATE_MAGIC;
    State.LastAcceptedSequence = UPLINK_APP_Data.LastAcceptedSequence;
    State.BootCount            = UPLINK_APP_Data.BootCount;
    State.Checksum             = State.Magic + State.LastAcceptedSequence + State.BootCount;

    snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", StatePath);

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

    /* rename()이 디스크에 반영됐다는 보장은 파일 fd만 fsync해선 안 됨 —
     * POSIX상 디렉터리 항목(rename) 자체도 부모 디렉터리 fd를 fsync해야
     * 정전 시 유실되지 않는다 (BL-18). */
    fsync(Fd);
    close(Fd);

    if (rename(TmpPath, StatePath) != 0)
    {
        return;
    }

    snprintf(DirPath, sizeof(DirPath), "%s", StatePath);
    Slash = strrchr(DirPath, '/');
    if (Slash != NULL)
    {
        *Slash = '\0';
        DirFd  = open(DirPath, O_RDONLY);
        if (DirFd >= 0)
        {
            fsync(DirFd);
            close(DirFd);
        }
    }
}

bool UPLINK_APP_ParseViewpointPayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                      UPLINK_APP_ViewpointPayload_t *Payload)
{
    if (Cmd->PayloadLength != (uint8)sizeof(UPLINK_APP_ViewpointPayload_t))
    {
        return false;
    }

    memcpy(Payload, Cmd->Payload, sizeof(*Payload));

    if (Payload->ViewpointVersion != UPLINK_APP_VIEWPOINT_VERSION)
    {
        return false;
    }
    if (Payload->ViewpointType > UPLINK_APP_VIEWPOINT_MAX_TYPE)
    {
        return false;
    }
    if (Payload->PositionFrame > UPLINK_APP_VIEWPOINT_MAX_FRAME)
    {
        return false;
    }
    if (!isfinite(Payload->X) || Payload->X < UPLINK_APP_VIEWPOINT_MIN_X_M || Payload->X > UPLINK_APP_VIEWPOINT_MAX_X_M)
    {
        return false;
    }
    if (!isfinite(Payload->Y) || Payload->Y < UPLINK_APP_VIEWPOINT_MIN_Y_M || Payload->Y > UPLINK_APP_VIEWPOINT_MAX_Y_M)
    {
        return false;
    }
    if (!isfinite(Payload->Z) || Payload->Z < UPLINK_APP_VIEWPOINT_MIN_ALT_M || Payload->Z > UPLINK_APP_VIEWPOINT_MAX_ALT_M)
    {
        return false;
    }
    if (!isfinite(Payload->Yaw) || Payload->Yaw < -UPLINK_APP_VIEWPOINT_MAX_YAW || Payload->Yaw > UPLINK_APP_VIEWPOINT_MAX_YAW)
    {
        return false;
    }
    if (!isfinite(Payload->Pitch) || Payload->Pitch < -UPLINK_APP_VIEWPOINT_MAX_PITCH || Payload->Pitch > UPLINK_APP_VIEWPOINT_MAX_PITCH)
    {
        return false;
    }
    if (Payload->HoldTimeMs > UPLINK_APP_VIEWPOINT_MAX_HOLD_MS)
    {
        return false;
    }

    return true;
}

bool UPLINK_APP_ForwardViewpointCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                        const UPLINK_APP_ViewpointPayload_t *Payload)
{
    UPLINK_APP_ViewpointCmdTlm_t ViewpointTlm;

    memset(&ViewpointTlm, 0, sizeof(ViewpointTlm));
    CFE_MSG_Init(CFE_MSG_PTR(ViewpointTlm.TelemetryHeader), CFE_SB_ValueToMsgId(VIEWPOINT_CMD_MID),
                 sizeof(ViewpointTlm));

    ViewpointTlm.Seq            = UPLINK_APP_Data.SequenceCounter + 1U;
    ViewpointTlm.TimestampMs    = UPLINK_APP_Data.LastRxTimeMs;
    ViewpointTlm.SourceSequence = Cmd->Sequence;
    ViewpointTlm.ViewpointType  = Payload->ViewpointType;
    ViewpointTlm.PositionFrame  = Payload->PositionFrame;
    ViewpointTlm.X              = Payload->X;
    ViewpointTlm.Y              = Payload->Y;
    ViewpointTlm.Z              = Payload->Z;
    ViewpointTlm.Yaw            = Payload->Yaw;
    ViewpointTlm.Pitch          = Payload->Pitch;
    ViewpointTlm.HoldTimeMs     = Payload->HoldTimeMs;

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

        case UPLINK_APP_CLASS_COUNTER_MGMT:
            return UPLINK_APP_ROUTE_COUNTER_MGMT;

        default:
            return UPLINK_APP_ROUTE_NONE;
    }
}

static uint16 UPLINK_APP_CRC16(const uint8 *Data, uint16 Len)
{
    uint16 Crc = 0xFFFF;
    uint16 i;
    uint8  j;

    for (i = 0; i < Len; i++)
    {
        Crc ^= (uint16)Data[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (Crc & 0x8000)
                Crc = (Crc << 1) ^ 0x1021;
            else
                Crc <<= 1;
        }
    }
    return Crc;
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


#include "uplink_app_utils.h"
#include "uplink_app_eventids.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
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
    /* BL-43(2026-07-23): 부팅/오류 영속화 — spec §12.3 ① */
    uint8  LastResetReason;
    uint8  SurvivedMark;
    uint8  ShortBootStreak;
    uint8  BootReserved;
    uint32 Checksum;
} UPLINK_APP_PersistentState_t;

/* BL-43: 생존 마커 파라미터 */
#define UPLINK_APP_BOOT_SURVIVE_MS      120000U
#define UPLINK_APP_BOOT_LOOP_THRESHOLD  5U

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
    /* BL-43: 부팅/오류 보고 필드 */
    Tlm->BootLoopSuspect             = UPLINK_APP_Data.BootLoopSuspect;
    Tlm->ShortBootStreak             = UPLINK_APP_Data.ShortBootStreak;
    Tlm->LastResetReason             = UPLINK_APP_Data.LastResetReason;

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
        case UPLINK_APP_CLASS_FLIGHT_MODE:
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
    return isfinite(Waypoint->Param1) && isfinite(Waypoint->Param2) && isfinite(Waypoint->Param3) &&
           isfinite(Waypoint->Param4) && isfinite(Waypoint->Z);
}

static bool UPLINK_APP_IsWaypointAltitudeValid(const UPLINK_APP_Waypoint_t *Waypoint)
{
    return (Waypoint->Z >= UPLINK_APP_ROUTE_ALTITUDE_MIN_M) && (Waypoint->Z <= UPLINK_APP_ROUTE_ALTITUDE_MAX_M);
}

/* BL-56(2026-07-25) wire 레벨 헬퍼: ROUTE_WAYPOINT_t는 C 정렬 패딩으로 sizeof가
 * ROUTE_WAYPOINT_WIRE_SIZE(29)보다 커질 수 있어 memcpy/구조체 캐스팅을 쓸 수 없다 —
 * 필드별 개별 역직렬화(offset 0=CmdType,1~16=Param1~4,17~24=LatE7/LonE7,25~28=Z). */
static uint32 UPLINK_APP_ReadU32LE(const uint8 *Buf)
{
    return ((uint32)Buf[0]) | ((uint32)Buf[1] << 8) | ((uint32)Buf[2] << 16) | ((uint32)Buf[3] << 24);
}

static float UPLINK_APP_ReadFloatLE(const uint8 *Buf)
{
    uint32 Bits = UPLINK_APP_ReadU32LE(Buf);
    float  Value;

    memcpy(&Value, &Bits, sizeof(Value));
    return Value;
}

static void UPLINK_APP_ParseWaypointWire(const uint8 *Raw, UPLINK_APP_Waypoint_t *Waypoint)
{
    memset(Waypoint, 0, sizeof(*Waypoint));

    Waypoint->CmdType = Raw[0];
    Waypoint->Param1  = UPLINK_APP_ReadFloatLE(&Raw[1]);
    Waypoint->Param2  = UPLINK_APP_ReadFloatLE(&Raw[5]);
    Waypoint->Param3  = UPLINK_APP_ReadFloatLE(&Raw[9]);
    Waypoint->Param4  = UPLINK_APP_ReadFloatLE(&Raw[13]);
    Waypoint->LatE7   = (int32)UPLINK_APP_ReadU32LE(&Raw[17]);
    Waypoint->LonE7   = (int32)UPLINK_APP_ReadU32LE(&Raw[21]);
    Waypoint->Z       = UPLINK_APP_ReadFloatLE(&Raw[25]);
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

    Payload->RouteType     = Cmd->Payload[0];
    Payload->RouteVersion  = Cmd->Payload[1];
    Payload->WaypointCount = Cmd->Payload[2]; /* index_or_count: REPLACE/ADD=count, DELETE/MODIFY=index */
    Payload->Reserved      = Cmd->Payload[3];

    /* route_version v1(구 REPLACE/APPEND/DELETE, 12바이트 waypoint)은 v2 파서로 오해석되면
     * 안 되므로 명시적으로 거부한다. */
    if (Payload->RouteVersion != UPLINK_APP_ROUTE_VERSION)
    {
        return false;
    }

    if ((Payload->RouteType != UPLINK_APP_ROUTE_OP_REPLACE) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_OP_ADD) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_OP_DELETE) &&
        (Payload->RouteType != UPLINK_APP_ROUTE_OP_MODIFY))
    {
        return false;
    }

    /* DELETE: index_or_count = 대상 인덱스, waypoint 데이터 없음(헤더 4바이트 고정). */
    if (Payload->RouteType == UPLINK_APP_ROUTE_OP_DELETE)
    {
        if (Payload->WaypointCount >= UPLINK_APP_ROUTE_MAX_WAYPOINTS)
        {
            return false;
        }
        if ((size_t)Cmd->PayloadLength != 4U)
        {
            return false;
        }
        return true;
    }

    /* MODIFY: index_or_count = 대상 인덱스, waypoint 1개(29바이트) 고정. */
    if (Payload->RouteType == UPLINK_APP_ROUTE_OP_MODIFY)
    {
        if (Payload->WaypointCount >= UPLINK_APP_ROUTE_MAX_WAYPOINTS)
        {
            return false;
        }
        if ((size_t)Cmd->PayloadLength != (4U + ROUTE_WAYPOINT_WIRE_SIZE))
        {
            return false;
        }

        UPLINK_APP_ParseWaypointWire(&Cmd->Payload[4], &Payload->Waypoints[0]);

        if (!UPLINK_APP_IsWaypointFinite(&Payload->Waypoints[0]))
        {
            return false;
        }
        if (!UPLINK_APP_IsWaypointAltitudeValid(&Payload->Waypoints[0]))
        {
            return false;
        }

        return true;
    }

    /* REPLACE/ADD: index_or_count = N개(1 이상 MAX 이하), 와이어 크기는 항상
     * ROUTE_WAYPOINT_WIRE_SIZE(29, 명시 상수)로 계산 — sizeof(ROUTE_WAYPOINT_t) 금지. */
    if ((Payload->WaypointCount == 0U) || (Payload->WaypointCount > UPLINK_APP_ROUTE_MAX_WAYPOINTS))
    {
        return false;
    }

    ExpectedLength = 4U + ((size_t)Payload->WaypointCount * ROUTE_WAYPOINT_WIRE_SIZE);
    if ((size_t)Cmd->PayloadLength != ExpectedLength)
    {
        return false;
    }

    for (Index = 0; Index < Payload->WaypointCount; ++Index)
    {
        UPLINK_APP_Waypoint_t *Waypoint = &Payload->Waypoints[Index];

        UPLINK_APP_ParseWaypointWire(&Cmd->Payload[4U + (Index * ROUTE_WAYPOINT_WIRE_SIZE)], Waypoint);

        if (!UPLINK_APP_IsWaypointFinite(Waypoint))
        {
            return false;
        }

        if (!UPLINK_APP_IsWaypointAltitudeValid(Waypoint))
        {
            return false;
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

    /* BL-83(2026-07-28 감사): 이전엔 else가 없어 payload가 짧아도 필드가
     * 0으로 채워진 채 그대로 발행+true 반환됐다 — 대상 앱은 이를 유효한
     * "액션=0" 명령으로 오인해 실행한다. 길이 미달은 명시적으로 거부한다. */
    if (Cmd->PayloadLength < 8U)
    {
        return false;
    }

    RecoveryTlm.RecoveryAction   = Cmd->Payload[0];
    RecoveryTlm.TargetComponent  = Cmd->Payload[1];
    RecoveryTlm.ReasonCode       = (uint16)Cmd->Payload[2] | ((uint16)Cmd->Payload[3] << 8);
    RecoveryTlm.RequestToken     = (uint32)Cmd->Payload[4] | ((uint32)Cmd->Payload[5] << 8) |
                                   ((uint32)Cmd->Payload[6] << 16) | ((uint32)Cmd->Payload[7] << 24);

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

/* BL-44(2026-07-24): flight mode base 명령 파싱 (§18.4.6.8).
 * payload = flight_mode(1) + waypoint_start_index(1) + request_token(4, LE) = 6B. */
bool UPLINK_APP_ParseFlightModePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                       UPLINK_APP_FlightModePayload_t *Payload)
{
    if (Cmd->PayloadLength < 6U)
    {
        return false;
    }

    if (Cmd->Payload[0] > UPLINK_APP_FLIGHT_MODE_LAND)
    {
        return false;
    }

    /* waypoint_start_index는 WAYPOINT 전용 — 그 외 모드에서 0이 아니면 거부 */
    if (Cmd->Payload[0] != UPLINK_APP_FLIGHT_MODE_WAYPOINT && Cmd->Payload[1] != 0U)
    {
        return false;
    }

    Payload->FlightMode         = Cmd->Payload[0];
    Payload->WaypointStartIndex = Cmd->Payload[1];
    Payload->RequestToken       = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                                  ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);

    return true;
}

/* BL-44(2026-07-24): mavlink_bridge_app CMD_MID로 SET_FLIGHT_MODE_CC 직접 전송
 * (counter management와 동일 패턴, cfs_core_app 미경유). RequestToken은 uplink_app의
 * Level 3 게이트에서만 소비되고 mavlink_bridge로는 전달하지 않는다(EXEC_RESULT_MID의
 * SourceSequence 상관으로 결과 회신이 이미 가능하기 때문). */
bool UPLINK_APP_ForwardFlightModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                         const UPLINK_APP_FlightModePayload_t *Payload)
{
    UPLINK_APP_FlightModeCtrlCmd_t *FmCmd = &UPLINK_APP_Data.FlightModeCtrlCmd;

    if (CFE_MSG_SetFcnCode(CFE_MSG_PTR(FmCmd->CommandHeader), UPLINK_APP_FLIGHT_MODE_SET_FLIGHT_MODE_CC) !=
        CFE_SUCCESS)
    {
        return false;
    }

    FmCmd->SourceSequence     = Cmd->Sequence;
    FmCmd->FlightMode         = Payload->FlightMode;
    FmCmd->WaypointStartIndex = Payload->WaypointStartIndex;

    return (CFE_SB_TransmitMsg(CFE_MSG_PTR(FmCmd->CommandHeader), true) == CFE_SUCCESS);
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

    /* BL-91(2026-07-28 감사): CFE_SB_TransmitMsg가 동기 호출이라 PENDING은 이
     * 함수 안에서 세팅되자마자 IDLE/REJECTED로 덮여 HK에 절대 노출되지 않는다
     * (비동기 왕복이 없는 설계상 한계 — 별도 재설계 없이는 관측 불가, 이번엔
     * 미수정). LastConfigResult=0/1은 순수 로컬 성공/실패 불리언이며
     * `CFS_CORE_APP_ConfigResult_t`(0=OK,1=BAD_SCOPE...6=BAD_CHECKSUM, 다른
     * 앱의 richer 코드 공간)와는 무관 — 값 1을 "BAD_SCOPE"로 오독하지 말 것. */
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

    /* BL-83(2026-07-28 감사): else 없이 길이 미달을 통과시키던 것을 명시 거부로 교체 */
    if (Cmd->PayloadLength < 6U)
    {
        return false;
    }

    ModeTlm.ModeAction     = Cmd->Payload[0];
    ModeTlm.RequestedState = Cmd->Payload[1];
    ModeTlm.RequestToken   = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                             ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);

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

    /* BL-83(2026-07-28 감사) — 실제 도달 가능한 경로였음: ValidateProxyCommand()는
     * ROUTE_UPDATE/VIEWPOINT에만 PayloadLength==0 거부를 걸어서 DIAGNOSTIC은
     * 0바이트 payload도 그대로 통과했고, else가 없던 이전 코드는 그 payload를
     * DiagAction=0(=LINK_STATUS)으로 채워 그대로 발행 — 요청하지 않은 진단
     * 동작이 실행됐다. 길이 미달은 명시적으로 거부한다. */
    if (Cmd->PayloadLength < 6U)
    {
        return false;
    }

    DiagTlm.DiagAction   = Cmd->Payload[0];
    DiagTlm.DiagTarget   = Cmd->Payload[1];
    DiagTlm.RequestToken = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                           ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);

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
    /* BL-91(2026-07-29 감사): 필드 가산 체크섬은 같은 크기의 두 필드값이
     * 뒤바뀌는 손상(합계 불변)을 감지 못해 CRC16으로 교체 — 필드별 뒤바뀜/
     * 비트반전 손상 모두 탐지 가능. Checksum 필드 자체는 범위 밖(마지막). */
    if (State.Checksum != (uint32)UPLINK_APP_CRC16((const uint8 *)&State,
                                                    (uint16)offsetof(UPLINK_APP_PersistentState_t, Checksum)))
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state file checksum mismatch, using defaults");
        return;
    }

    UPLINK_APP_Data.LastAcceptedSequence = State.LastAcceptedSequence;
    UPLINK_APP_Data.AcceptedCount        = 1;
    UPLINK_APP_Data.BootCount            = (uint8)State.BootCount;
    /* BL-43: 직전 세션 마커/streak 복원 (이번 세션 판정 재료) */
    UPLINK_APP_Data.PrevSurvivedMark     = State.SurvivedMark;
    UPLINK_APP_Data.ShortBootStreak      = State.ShortBootStreak;

    CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: restored persistent state seq=%lu boot_count=%u",
                      (unsigned long)State.LastAcceptedSequence, (unsigned int)UPLINK_APP_Data.BootCount);
}

/* BL-43(2026-07-23): Init에서 LoadState() 직후 호출 — 직전 세션 생존 마커로
 * 연속 단명 부팅(streak)을 판정하고, 이번 세션 마커를 0으로 내려 저장.
 * streak >= 임계면 BootLoopSuspect=1 (보고 전용 — 동작 변경 없음, 지상국 판단).
 * 첫 부팅(파일 없음)은 Init 기본값 PrevSurvivedMark=1이라 streak 증가 안 함. */
void UPLINK_APP_ProcessBootMarker(void)
{
    uint32 ResetSubtype = 0;

    UPLINK_APP_Data.BootStartMs     = UPLINK_APP_GetTimeMs(); /* 상대 uptime 기준점 */
    UPLINK_APP_Data.LastResetReason = (uint8)CFE_ES_GetResetType(&ResetSubtype);

    if (UPLINK_APP_Data.PrevSurvivedMark == 0U)
    {
        if (UPLINK_APP_Data.ShortBootStreak < 255U)
        {
            UPLINK_APP_Data.ShortBootStreak++;
        }
    }
    else
    {
        UPLINK_APP_Data.ShortBootStreak = 0U;
    }

    UPLINK_APP_Data.BootLoopSuspect =
        (UPLINK_APP_Data.ShortBootStreak >= UPLINK_APP_BOOT_LOOP_THRESHOLD) ? 1U : 0U;

    if (UPLINK_APP_Data.BootLoopSuspect != 0U)
    {
        CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: boot loop suspect - %u consecutive short boots",
                          (unsigned int)UPLINK_APP_Data.ShortBootStreak);
    }

    UPLINK_APP_Data.SurvivedMark = 0U;
    UPLINK_APP_SaveState();
}

/* BL-43: 주기 훅 — 가동 120s 도달 시 생존 마커 1을 1회 저장 (멱등).
 * Pi 실기 발견(2026-07-23): CFE_TIME은 부팅 시 0이 아님(대형 mission time) —
 * 절대 시각이 아니라 Init에서 기록한 BootStartMs 기준 상대 uptime으로 판정. */
void UPLINK_APP_CheckBootSurvival(void)
{
    if (UPLINK_APP_Data.SurvivedMark != 0U)
    {
        return;
    }

    if ((UPLINK_APP_GetTimeMs() - UPLINK_APP_Data.BootStartMs) >= UPLINK_APP_BOOT_SURVIVE_MS)
    {
        UPLINK_APP_Data.SurvivedMark = 1U;
        UPLINK_APP_SaveState();
        CFE_EVS_SendEvent(UPLINK_APP_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                          "UPLINK_APP: boot survival marked (uptime >= %ums)",
                          (unsigned int)UPLINK_APP_BOOT_SURVIVE_MS);
    }
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
    State.LastResetReason      = UPLINK_APP_Data.LastResetReason;
    State.SurvivedMark         = UPLINK_APP_Data.SurvivedMark;
    State.ShortBootStreak      = UPLINK_APP_Data.ShortBootStreak;
    State.BootReserved         = 0;
    State.Checksum             = (uint32)UPLINK_APP_CRC16((const uint8 *)&State,
                                                           (uint16)offsetof(UPLINK_APP_PersistentState_t, Checksum));

    snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", StatePath);

    Fd = open(TmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (Fd < 0)
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state save open failed path=%s errno=%d", TmpPath, errno);
        return;
    }

    if (write(Fd, &State, sizeof(State)) != (ssize_t)sizeof(State))
    {
        CFE_EVS_SendEvent(UPLINK_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state save write failed path=%s errno=%d", TmpPath, errno);
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
        CFE_EVS_SendEvent(UPLINK_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: state save rename failed path=%s errno=%d", StatePath, errno);
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

        case UPLINK_APP_CLASS_FLIGHT_MODE:
            return UPLINK_APP_ROUTE_FLIGHT_MODE;

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

    UPLINK_APP_CheckBootSurvival(); /* BL-43: 120s 생존 마킹 (멱등, 1회 저장) */

    NowMs = UPLINK_APP_GetTimeMs();
    if ((NowMs - UPLINK_APP_Data.LastPublishTimeMs) < UPLINK_APP_PROTOTYPE_PERIOD_MS)
    {
        return;
    }
    UPLINK_APP_UpdateStatusTelemetry(NowMs);
}


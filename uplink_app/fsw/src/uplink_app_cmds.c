#include "uplink_app_cmds.h"
#include "uplink_app_eventids.h"
#include "uplink_app_utils.h"

typedef enum
{
    UPLINK_APP_SEQ_NEW,       /* Sequence > LastAcceptedSequence (or bootstrap) */
    UPLINK_APP_SEQ_DUPLICATE, /* Sequence == LastAcceptedSequence: 4x 재전송 슬롯 중복 */
    UPLINK_APP_SEQ_REPLAY     /* Sequence < LastAcceptedSequence: 진짜 replay/desync */
} UPLINK_APP_SeqCheck_t;

static UPLINK_APP_SeqCheck_t UPLINK_APP_CheckSequence(uint16 Sequence)
{
    uint16 Last;
    uint16 Diff;

    if (UPLINK_APP_Data.AcceptedCount == 0U)
    {
        return UPLINK_APP_SEQ_NEW;
    }

    Last = (uint16)UPLINK_APP_Data.LastAcceptedSequence;

    if (Sequence == Last)
    {
        return UPLINK_APP_SEQ_DUPLICATE;
    }

    /* BL-13(2026-07-21): 지상 seq가 uint16 랩어라운드(65535->1)될 때 평범한
     * `>` 비교는 정상 명령을 REPLAY로 오판함(1 > 65535 == false). 모듈러
     * 비교(윈도우 절반=0x8000)로 대체 — Sequence-Last를 uint16 산술로 계산해
     * 0x8000 미만이면 "앞으로 진행"(NEW), 이상이면 REPLAY. */
    Diff = (uint16)(Sequence - Last);
    return (Diff < 0x8000U) ? UPLINK_APP_SEQ_NEW : UPLINK_APP_SEQ_REPLAY;
}

static uint8 UPLINK_APP_GetClassRequiredLevel(uint8 CommandClass)
{
    /* Return authorization level required for each command class (§18.11.1).
     * §18.10.4: case labels previously used raw numbers that didn't match
     * UPLINK_APP_CLASS_* (MODE=5/DIAGNOSTIC=6 were swapped), which made
     * DIAGNOSTIC permanently unauthorizable (level 3 with no request_token
     * parse path) even though §18.10.1 requires it to remain the always-
     * available intervention class in RECOVERY/FAILED. Keyed by name now. */
    switch (CommandClass)
    {
        case UPLINK_APP_CLASS_NONE:         return 1; /* NOOP */
        case UPLINK_APP_CLASS_CONFIG:       return 2; /* runtime configuration */
        case UPLINK_APP_CLASS_ROUTE_UPDATE: return 2; /* route update */
        case UPLINK_APP_CLASS_VIEWPOINT:    return 2; /* viewpoint update */
        case UPLINK_APP_CLASS_RECOVERY:     return 3; /* recovery command */
        case UPLINK_APP_CLASS_MODE:         return 3; /* mode command */
        case UPLINK_APP_CLASS_DIAGNOSTIC:   return 1; /* diagnostic command */
        case UPLINK_APP_CLASS_COUNTER_MGMT: return 3; /* counter management (§18.4.6.7) */
        case UPLINK_APP_CLASS_FLIGHT_MODE:  return 3; /* flight mode base 명령 (BL-44, §18.4.6.8) */
        default: return 0xFF; /* unknown */
    }
}

static bool UPLINK_APP_IsAuthorized(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, uint32 RequestToken)
{
    uint8 auth_level = (Cmd->Flags >> 6) & 0x3; /* Bits[7:6] */
    uint8 required_level = UPLINK_APP_GetClassRequiredLevel(Cmd->CommandClass);

    if (auth_level < required_level)
    {
        return false;
    }

    /* Level 3 commands require non-zero request_token */
    if (required_level == 3 && RequestToken == 0U)
    {
        return false;
    }

    return true;
}

void UPLINK_APP_Noop(const UPLINK_APP_NoopCmd_t *Cmd)
{
    (void)Cmd;
    UPLINK_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(UPLINK_APP_NOOP_EID, CFE_EVS_EventType_INFORMATION, "UPLINK_APP: NOOP");
}

void UPLINK_APP_ResetCounters(const UPLINK_APP_ResetCountersCmd_t *Cmd)
{
    (void)Cmd;
    UPLINK_APP_Data.CmdCounter          = 0;
    UPLINK_APP_Data.ErrCounter          = 0;
    UPLINK_APP_Data.AcceptedCount       = 0;
    UPLINK_APP_Data.RejectedCount       = 0;
    UPLINK_APP_Data.DuplicateCount      = 0;
    UPLINK_APP_Data.RoutingFailureCount = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;
    UPLINK_APP_Data.LastCommandCode     = 0;
    UPLINK_APP_Data.LastCommandResult   = UPLINK_APP_RESULT_NONE;
    UPLINK_APP_Data.LastRouteTarget     = UPLINK_APP_ROUTE_NONE;
    CFE_EVS_SendEvent(UPLINK_APP_RESET_EID, CFE_EVS_EventType_INFORMATION, "UPLINK_APP: RESET");
}

void UPLINK_APP_ProcessUplink(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    CFE_TIME_SysTime_t       TimeNow;
    uint64                   TimeMs;
    UPLINK_APP_Result_t           Result;
    UPLINK_APP_RouteTarget_t      RouteTarget;
    UPLINK_APP_RouteUpdatePayload_t RoutePayload;
    UPLINK_APP_ViewpointPayload_t ViewpointPayload;
    UPLINK_APP_FlightModePayload_t FlightModePayload;

    TimeNow = CFE_TIME_GetTime();
    TimeMs  = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);

    UPLINK_APP_Data.LastRxTimeMs     = (uint32)TimeMs;
    UPLINK_APP_Data.LastCommandCode  = Cmd->CommandClass;
    UPLINK_APP_Data.LastRxSequence   = Cmd->Sequence;

    UPLINK_APP_SeqCheck_t SeqCheck = UPLINK_APP_CheckSequence(Cmd->Sequence);

    if (SeqCheck == UPLINK_APP_SEQ_DUPLICATE)
    {
        /* 지상국이 반이중 타이밍 지터를 견디려고 같은 명령을 4회 연속
         * 전송하는 설계(§4x 재전송)의 2~4번째 슬롯. replay가 아니라
         * 이미 수락된 명령의 중복 도착이므로 에러/링크저하로 취급하지
         * 않는다 (BL-01, notes/temp/uplink_seq_feedback_redesign). */
        UPLINK_APP_Data.DuplicateCount++;
        UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_DUPLICATE;
        /* BL-14: Flags bits[2:1]=RETX_IDX(0~3=슬롯-1) — 몇 번째 재전송
         * 사본이 중복 도착했는지 진단용으로만 표기(§18.4.3.1) */
        CFE_EVS_SendEvent(UPLINK_APP_DUPLICATE_EID, CFE_EVS_EventType_DEBUG,
                          "UPLINK_APP: duplicate uplink seq=%u retx=%u (retransmit slot)",
                          (unsigned int)Cmd->Sequence,
                          (unsigned int)((Cmd->Flags >> 1) & 0x3U));
        UPLINK_APP_UpdateStatusTelemetry(0);
        return;
    }

    if (SeqCheck == UPLINK_APP_SEQ_REPLAY)
    {
        UPLINK_APP_Data.ErrCounter++;
        UPLINK_APP_Data.RejectedCount++;
        UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_SEQUENCE;
        UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: uplink proxy rejected replay seq=%u last=%u",
                          (unsigned int)Cmd->Sequence, (unsigned int)UPLINK_APP_Data.LastAcceptedSequence);
        UPLINK_APP_UpdateStatusTelemetry(0);
        return;
    }

    if (!UPLINK_APP_ValidateProxyCommand(Cmd, &Result))
    {
        UPLINK_APP_Data.ErrCounter++;
        UPLINK_APP_Data.RejectedCount++;
        UPLINK_APP_Data.LastCommandResult = Result;
        UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: uplink proxy rejected class=%u result=%u seq=%u",
                          (unsigned int)Cmd->CommandClass, (unsigned int)Result, (unsigned int)Cmd->Sequence);
        UPLINK_APP_UpdateStatusTelemetry(0);
        return;
    }

    RouteTarget = UPLINK_APP_ResolveRouteTarget(Cmd->CommandClass);
    if (RouteTarget == UPLINK_APP_ROUTE_NONE)
    {
        UPLINK_APP_Data.ErrCounter++;
        UPLINK_APP_Data.RoutingFailureCount++;
        UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_ROUTE_MISS;
        UPLINK_APP_Data.LastRouteTarget   = UPLINK_APP_ROUTE_NONE;
        UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: no route target for uplink class=%u seq=%u",
                          (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence);
        UPLINK_APP_UpdateStatusTelemetry(0);
        return;
    }

    if (!UPLINK_APP_Data.CfsHealthReceived)
    {
        /* Policy: block all commands until first SYSTEM_HEALTH_MID is received (fail-safe boot) */
        UPLINK_APP_Data.ErrCounter++;
        UPLINK_APP_Data.RejectedCount++;
        UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_STATE;
        UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
        CFE_EVS_SendEvent(UPLINK_APP_STATE_BLOCK_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: command blocked (no health yet) class=%u seq=%u",
                          (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence);
        UPLINK_APP_UpdateStatusTelemetry(0);
        return;
    }

    /* Health state policy checks */
    {
        uint8 State   = UPLINK_APP_Data.CfsHealthState;
        bool  Blocked = false;

        if (State == 3U) /* FAILED: only RECOVERY + DIAGNOSTIC allowed (ground must retain intervention path) */
        {
            Blocked = (Cmd->CommandClass != UPLINK_APP_CLASS_RECOVERY &&
                       Cmd->CommandClass != UPLINK_APP_CLASS_DIAGNOSTIC);
        }
        else if (State == 2U) /* RECOVERY: only RECOVERY + DIAGNOSTIC allowed */
        {
            Blocked = (Cmd->CommandClass != UPLINK_APP_CLASS_RECOVERY &&
                       Cmd->CommandClass != UPLINK_APP_CLASS_DIAGNOSTIC);
        }
        else if (State == 1U) /* DEGRADED: block VIEWPOINT + CONFIG (§18.10.1) + FLIGHT_MODE(WAYPOINT만, BL-44) */
        {
            Blocked = (Cmd->CommandClass == UPLINK_APP_CLASS_VIEWPOINT ||
                       Cmd->CommandClass == UPLINK_APP_CLASS_CONFIG ||
                       (Cmd->CommandClass == UPLINK_APP_CLASS_FLIGHT_MODE && Cmd->PayloadLength >= 1U &&
                        Cmd->Payload[0] == UPLINK_APP_FLIGHT_MODE_WAYPOINT));
        }

        /* BL-44(2026-07-24, §18.4.6.8): HOVER/LAND는 "위험 축소"(새로 시도 않고 유지/종료) 명령이라
         * 시스템 헬스와 무관하게 항상 허용 — WAYPOINT("위험 증가": 새 경로 신뢰)만 위 게이트를 정상 적용받는다. */
        if (Blocked && Cmd->CommandClass == UPLINK_APP_CLASS_FLIGHT_MODE && Cmd->PayloadLength >= 1U &&
            (Cmd->Payload[0] == UPLINK_APP_FLIGHT_MODE_HOVER || Cmd->Payload[0] == UPLINK_APP_FLIGHT_MODE_LAND))
        {
            CFE_EVS_SendEvent(UPLINK_APP_STATE_BLOCK_EID, CFE_EVS_EventType_INFORMATION,
                              "UPLINK_APP: FLIGHT_MODE HOVER/LAND exempt from health gate (state=%u seq=%u)",
                              (unsigned int)State, (unsigned int)Cmd->Sequence);
            Blocked = false;
        }

        /* 명령 자체에 실린 강제 플래그(무선으로 실제 온 값, 컴파일타임 기본값 아님) —
         * 지상에서 매번 명시적으로 세워야 하고 통과될 때마다 이벤트가 남는다. */
        if (Blocked && (Cmd->Flags & UPLINK_APP_FORCE_FLAG) != 0U)
        {
            CFE_EVS_SendEvent(UPLINK_APP_STATE_BLOCK_EID, CFE_EVS_EventType_INFORMATION,
                              "UPLINK_APP: FORCED THROUGH health gate (state=%u class=%u seq=%u)",
                              (unsigned int)State, (unsigned int)Cmd->CommandClass,
                              (unsigned int)Cmd->Sequence);
            Blocked = false;
        }

        if (Blocked)
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_STATE;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_STATE_BLOCK_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: command blocked by health state=%u class=%u seq=%u",
                              (unsigned int)State, (unsigned int)Cmd->CommandClass,
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }

    /* Authorization check (§18.11.1) */
    {
        uint32 request_token = 0U;

        /* Parse request_token from payload if Level 3 command */
        uint8 required_level = UPLINK_APP_GetClassRequiredLevel(Cmd->CommandClass);
        if (required_level == 3 && Cmd->PayloadLength >= 4U)
        {
            /* Different classes store request_token at different offsets */
            if (Cmd->CommandClass == UPLINK_APP_CLASS_RECOVERY && Cmd->PayloadLength >= 8U)
            {
                request_token = (uint32)Cmd->Payload[4] | ((uint32)Cmd->Payload[5] << 8) |
                               ((uint32)Cmd->Payload[6] << 16) | ((uint32)Cmd->Payload[7] << 24);
            }
            else if (Cmd->CommandClass == UPLINK_APP_CLASS_MODE && Cmd->PayloadLength >= 6U)
            {
                request_token = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                               ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);
            }
            else if (Cmd->CommandClass == UPLINK_APP_CLASS_COUNTER_MGMT && Cmd->PayloadLength >= 6U)
            {
                request_token = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                               ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);
            }
            else if (Cmd->CommandClass == UPLINK_APP_CLASS_FLIGHT_MODE && Cmd->PayloadLength >= 6U)
            {
                request_token = (uint32)Cmd->Payload[2] | ((uint32)Cmd->Payload[3] << 8) |
                               ((uint32)Cmd->Payload[4] << 16) | ((uint32)Cmd->Payload[5] << 24);
            }
        }

        if (!UPLINK_APP_IsAuthorized(Cmd, request_token))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_STATE;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            uint8 auth_level = (Cmd->Flags >> 6) & 0x3;
            uint8 req_level = UPLINK_APP_GetClassRequiredLevel(Cmd->CommandClass);
            CFE_EVS_SendEvent(UPLINK_APP_AUTHZ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: command blocked (insufficient auth) auth=%u required=%u class=%u seq=%u",
                              (unsigned int)auth_level, (unsigned int)req_level,
                              (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }

    if (Cmd->CommandClass == UPLINK_APP_CLASS_ROUTE_UPDATE)
    {
        if (!UPLINK_APP_ParseRouteUpdatePayload(Cmd, &RoutePayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_ROUTE;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: invalid route update payload seq=%u len=%u",
                              (unsigned int)Cmd->Sequence, (unsigned int)Cmd->PayloadLength);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }

        if (!UPLINK_APP_PublishRouteUpdate(Cmd, &RoutePayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to publish route update seq=%u type=%u count=%u",
                              (unsigned int)Cmd->Sequence, (unsigned int)RoutePayload.RouteType,
                              (unsigned int)RoutePayload.WaypointCount);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_RECOVERY)
    {
        if (!UPLINK_APP_ForwardRecoveryCommand(Cmd))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward recovery command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_VIEWPOINT)
    {
        if (!UPLINK_APP_ParseViewpointPayload(Cmd, &ViewpointPayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_VIEWPOINT;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: invalid viewpoint payload seq=%u len=%u",
                              (unsigned int)Cmd->Sequence, (unsigned int)Cmd->PayloadLength);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }

        if (!UPLINK_APP_ForwardViewpointCommand(Cmd, &ViewpointPayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward viewpoint command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_CONFIG)
    {
        if (!UPLINK_APP_ForwardConfigCommand(Cmd))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward config command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_MODE)
    {
        if (!UPLINK_APP_ForwardModeCommand(Cmd))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward mode command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_DIAGNOSTIC)
    {
        if (!UPLINK_APP_ForwardDiagnosticCommand(Cmd))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward diagnostic command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_COUNTER_MGMT)
    {
        if (!UPLINK_APP_ForwardCounterMgmtCommand(Cmd))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_COUNTER;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed counter management command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }
    else if (Cmd->CommandClass == UPLINK_APP_CLASS_FLIGHT_MODE)
    {
        if (!UPLINK_APP_ParseFlightModePayload(Cmd, &FlightModePayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RejectedCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_REJECT_FLIGHT_MODE;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: invalid flight mode payload seq=%u len=%u",
                              (unsigned int)Cmd->Sequence, (unsigned int)Cmd->PayloadLength);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }

        if (!UPLINK_APP_ForwardFlightModeCommand(Cmd, &FlightModePayload))
        {
            UPLINK_APP_Data.ErrCounter++;
            UPLINK_APP_Data.RoutingFailureCount++;
            UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_FAILED;
            UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
            UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_DEGRADED;
            CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "UPLINK_APP: failed to forward flight mode command seq=%u",
                              (unsigned int)Cmd->Sequence);
            UPLINK_APP_UpdateStatusTelemetry(0);
            return;
        }
    }

    UPLINK_APP_Data.CmdCounter++;
    UPLINK_APP_Data.AcceptedCount++;
    UPLINK_APP_Data.LastAcceptedSequence = Cmd->Sequence;
    UPLINK_APP_SaveState();
    UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
    UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_NOMINAL;

    /* BL-14: retx=Flags bits[2:1] — 4x 슬롯 중 몇 번째 사본이 최초 수락됐는지.
     * 0이면 첫 슬롯에 통과(링크 양호), 클수록 RF 마진 부족 신호(§18.4.3.1). */
    CFE_EVS_SendEvent(UPLINK_APP_PUBLISH_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: routed uplink class=%u seq=%u target=%u payload_len=%u retx=%u",
                      (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence,
                      (unsigned int)RouteTarget, (unsigned int)Cmd->PayloadLength,
                      (unsigned int)((Cmd->Flags >> 1) & 0x3U));
    UPLINK_APP_UpdateStatusTelemetry(0);
}

/* BL-08(2026-07-22): 대상앱(cfs_core_app/mavlink_bridge_app/lora_tdm_app)의
 * EXEC_RESULT 회신 처리. SourceSequence가 현재 추적 중인 최신 수락 명령과
 * 일치할 때만 반영 — 타임아웃 없이 "새 명령이 오면 이전 PENDING/응답은
 * 자연히 덮어써진다"는 결정(2026-07-22)을 이 상관 검사로 구현. 일치하지
 * 않으면(오래된 명령에 대한 지연 응답, 또는 순서가 꼬인 응답) 조용히
 * 무시 — 최신 상태를 stale 데이터로 덮어쓰지 않기 위함. */
void UPLINK_APP_ProcessExecResult(const UPLINK_APP_ExecResultTlm_t *Msg)
{
    if (Msg->SourceSequence != (uint16)UPLINK_APP_Data.LastAcceptedSequence)
    {
        return;
    }

    UPLINK_APP_Data.LastCommandResult =
        (Msg->GenericResult == 0U) ? UPLINK_APP_RESULT_EXECUTED_OK : UPLINK_APP_RESULT_EXECUTED_FAILED;

    CFE_EVS_SendEvent(UPLINK_APP_PUBLISH_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: exec result seq=%u source_app=%u class=%u generic=%u detail=%u",
                      (unsigned int)Msg->SourceSequence, (unsigned int)Msg->SourceApp,
                      (unsigned int)Msg->CommandClass, (unsigned int)Msg->GenericResult,
                      (unsigned int)Msg->DetailCode);
    UPLINK_APP_UpdateStatusTelemetry(0);
}


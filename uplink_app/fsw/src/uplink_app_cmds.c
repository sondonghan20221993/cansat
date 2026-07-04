#include "uplink_app_cmds.h"
#include "uplink_app_eventids.h"
#include "uplink_app_utils.h"

static bool UPLINK_APP_IsSequenceAccepted(uint16 Sequence)
{
    if (UPLINK_APP_Data.AcceptedCount == 0U)
    {
        return true;
    }

    return (Sequence > UPLINK_APP_Data.LastAcceptedSequence);
}

static uint8 UPLINK_APP_GetClassRequiredLevel(uint8 CommandClass)
{
    /* Return authorization level required for each command class (§18.11.1) */
    switch (CommandClass)
    {
        case 0: return 1; /* NOOP */
        case 1: return 2; /* runtime configuration */
        case 2: return 2; /* route update */
        case 3: return 2; /* viewpoint update */
        case 4: return 3; /* recovery command */
        case 5: return 1; /* diagnostic command */
        case 6: return 3; /* counter management */
        case 7: return 3; /* mode command */
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

    TimeNow = CFE_TIME_GetTime();
    TimeMs  = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);

    UPLINK_APP_Data.LastRxTimeMs     = (uint32)TimeMs;
    UPLINK_APP_Data.LastCommandCode  = Cmd->CommandClass;
    UPLINK_APP_Data.LastRxSequence   = Cmd->Sequence;

    if (!UPLINK_APP_IsSequenceAccepted(Cmd->Sequence))
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
        else if (State == 1U) /* DEGRADED: block VIEWPOINT + CONFIG (§18.10.1) */
        {
            Blocked = (Cmd->CommandClass == UPLINK_APP_CLASS_VIEWPOINT ||
                       Cmd->CommandClass == UPLINK_APP_CLASS_CONFIG);
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
            /* counter management does not use request_token, token stays 0 */
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

    UPLINK_APP_Data.CmdCounter++;
    UPLINK_APP_Data.AcceptedCount++;
    UPLINK_APP_Data.LastAcceptedSequence = Cmd->Sequence;
    UPLINK_APP_SaveState();
    UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
    UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_NOMINAL;

    CFE_EVS_SendEvent(UPLINK_APP_PUBLISH_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: routed uplink class=%u seq=%u target=%u payload_len=%u",
                      (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence,
                      (unsigned int)RouteTarget, (unsigned int)Cmd->PayloadLength);
    UPLINK_APP_UpdateStatusTelemetry(0);
}


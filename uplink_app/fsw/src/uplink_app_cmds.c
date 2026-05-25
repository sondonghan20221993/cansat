#include "uplink_app_cmds.h"
#include "uplink_app_eventids.h"
#include "uplink_app_utils.h"

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
    UPLINK_APP_Data.LastCommandCode     = 0;
    UPLINK_APP_Data.LastCommandResult   = UPLINK_APP_RESULT_NONE;
    UPLINK_APP_Data.LastRouteTarget     = UPLINK_APP_ROUTE_NONE;
    CFE_EVS_SendEvent(UPLINK_APP_RESET_EID, CFE_EVS_EventType_INFORMATION, "UPLINK_APP: RESET");
}

void UPLINK_APP_ProcessUplink(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    CFE_TIME_SysTime_t       TimeNow;
    uint64                   TimeMs;
    UPLINK_APP_Result_t      Result;
    UPLINK_APP_RouteTarget_t RouteTarget;

    TimeNow = CFE_TIME_GetTime();
    TimeMs  = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);

    UPLINK_APP_Data.LastRxTimeMs    = (uint32)TimeMs;
    UPLINK_APP_Data.LastCommandCode = Cmd->CommandClass;

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

    UPLINK_APP_Data.CmdCounter++;
    UPLINK_APP_Data.AcceptedCount++;
    UPLINK_APP_Data.LastCommandResult = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LastRouteTarget   = (uint8)RouteTarget;
    UPLINK_APP_Data.LinkState         = UPLINK_APP_LINK_NOMINAL;

    CFE_EVS_SendEvent(UPLINK_APP_PUBLISH_EID, CFE_EVS_EventType_INFORMATION,
                      "UPLINK_APP: routed uplink class=%u seq=%u target=%u payload_len=%u",
                      (unsigned int)Cmd->CommandClass, (unsigned int)Cmd->Sequence,
                      (unsigned int)RouteTarget, (unsigned int)Cmd->PayloadLength);
    UPLINK_APP_UpdateStatusTelemetry(0);
}


#include "mavlink_bridge_app_cmds.h"
#include "mavlink_bridge_app_eventids.h"
#include "mavlink_bridge_app_utils.h"

void MAVLINK_BRIDGE_APP_Noop(const MAVLINK_BRIDGE_APP_NoopCmd_t *Cmd)
{
    if (!MAVLINK_BRIDGE_APP_VerifyCmdLength(&Cmd->CommandHeader.Msg, sizeof(*Cmd)))
    {
        return;
    }

    MAVLINK_BRIDGE_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_NOOP_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: NOOP command");
}

void MAVLINK_BRIDGE_APP_ResetCountersCmd(const MAVLINK_BRIDGE_APP_ResetCountersCmd_t *Cmd)
{
    if (!MAVLINK_BRIDGE_APP_VerifyCmdLength(&Cmd->CommandHeader.Msg, sizeof(*Cmd)))
    {
        return;
    }

    MAVLINK_BRIDGE_APP_Data.CmdCounter = 0;
    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 0;
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_RESET_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: Reset counters command");
}

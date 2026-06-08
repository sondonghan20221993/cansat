#include "mavlink_bridge_app.h"
#include "mavlink_bridge_app_cmds.h"
#include "utgenstub.h"

void MAVLINK_BRIDGE_APP_Noop(const MAVLINK_BRIDGE_APP_NoopCmd_t *cmd)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_Noop, const MAVLINK_BRIDGE_APP_NoopCmd_t *, cmd);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_Noop, Basic, NULL);
}

void MAVLINK_BRIDGE_APP_ResetCountersCmd(const MAVLINK_BRIDGE_APP_ResetCountersCmd_t *cmd)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_ResetCountersCmd, const MAVLINK_BRIDGE_APP_ResetCountersCmd_t *, cmd);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_ResetCountersCmd, Basic, NULL);
}

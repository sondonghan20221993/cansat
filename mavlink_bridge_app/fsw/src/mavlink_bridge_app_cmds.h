#ifndef MAVLINK_BRIDGE_APP_CMDS_H
#define MAVLINK_BRIDGE_APP_CMDS_H

#include "mavlink_bridge_app.h"

void MAVLINK_BRIDGE_APP_Noop(const MAVLINK_BRIDGE_APP_NoopCmd_t *Cmd);
void MAVLINK_BRIDGE_APP_ResetCountersCmd(const MAVLINK_BRIDGE_APP_ResetCountersCmd_t *Cmd);

#endif

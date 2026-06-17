#ifndef UPLINK_APP_CMDS_H
#define UPLINK_APP_CMDS_H

#include "uplink_app.h"

void UPLINK_APP_Noop(const UPLINK_APP_NoopCmd_t *Cmd);
void UPLINK_APP_ResetCounters(const UPLINK_APP_ResetCountersCmd_t *Cmd);
void UPLINK_APP_ProcessUplink(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);

#endif


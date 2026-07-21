#ifndef UPLINK_APP_UTILS_H
#define UPLINK_APP_UTILS_H

#include "uplink_app.h"

void   UPLINK_APP_UpdateStatusTelemetry(uint32 NowMs);
uint16 UPLINK_APP_ComputeProxyCrc(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
void   UPLINK_APP_LoadState(void);
void   UPLINK_APP_SaveState(void);
void   UPLINK_APP_IncrementBootCount(void);

#endif


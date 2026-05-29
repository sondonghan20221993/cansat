#ifndef UPLINK_APP_UTILS_H
#define UPLINK_APP_UTILS_H

#include "uplink_app.h"

void   UPLINK_APP_UpdateStatusTelemetry(uint32 NowMs);
void   UPLINK_APP_ServiceLoRa(void);
uint16 UPLINK_APP_CRC16(const uint8 *Data, uint16 Len);
bool   UPLINK_APP_ParseLoRaFrame(const char *Line, UPLINK_APP_ProcessUplinkCmd_t *Out);

#endif


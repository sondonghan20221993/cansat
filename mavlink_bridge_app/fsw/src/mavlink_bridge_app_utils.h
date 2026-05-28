#ifndef MAVLINK_BRIDGE_APP_UTILS_H
#define MAVLINK_BRIDGE_APP_UTILS_H

#include "mavlink_bridge_app.h"

void MAVLINK_BRIDGE_APP_ReportHousekeeping(void);
bool MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_APP_LinkState_t NewState);
void MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_APP_ErrorCode_t ErrorCode);
void MAVLINK_BRIDGE_APP_ServiceSerial(void);
void MAVLINK_BRIDGE_APP_RequestTelemetryStreams(void);
void MAVLINK_BRIDGE_APP_StartMissionUpload(const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *Msg);

#endif

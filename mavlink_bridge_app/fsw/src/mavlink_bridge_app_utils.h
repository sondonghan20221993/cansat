#ifndef MAVLINK_BRIDGE_APP_UTILS_H
#define MAVLINK_BRIDGE_APP_UTILS_H

#include "mavlink_bridge_app.h"
#include "mavlink_bridge_app_msg.h"

void MAVLINK_BRIDGE_APP_ReportHousekeeping(void);
bool MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_APP_LinkState_t NewState);
void MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_APP_ErrorCode_t ErrorCode);
void MAVLINK_BRIDGE_APP_ServiceSerial(void);
void MAVLINK_BRIDGE_APP_RequestTelemetryStreams(void);
void MAVLINK_BRIDGE_APP_StartMissionUpload(const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *Msg);
void MAVLINK_BRIDGE_APP_MissionQuery(const MAVLINK_BRIDGE_APP_MissionQueryCmd_t *Cmd);
void MAVLINK_BRIDGE_APP_ProcessConfigCommand(const MAVLINK_BRIDGE_APP_ConfigCmdTlm_t *Msg);
void MAVLINK_BRIDGE_APP_ProcessParserResetCmd(const MAVLINK_BRIDGE_APP_ParserResetCmd_t *Cmd);
void MAVLINK_BRIDGE_APP_ProcessSerialReconnectCmd(const MAVLINK_BRIDGE_APP_SerialReconnectCmd_t *Cmd);

#endif

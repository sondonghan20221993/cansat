#include "mavlink_bridge_app.h"
#include "utgenstub.h"

void MAVLINK_BRIDGE_APP_ReportHousekeeping(void)
{
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_ReportHousekeeping, Basic, NULL);
}

bool MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    UT_GenStub_SetupReturnBuffer(MAVLINK_BRIDGE_APP_VerifyCmdLength, bool);
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_VerifyCmdLength, const CFE_MSG_Message_t *, MsgPtr);
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_VerifyCmdLength, size_t, ExpectedLength);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_VerifyCmdLength, Basic, NULL);
    return UT_GenStub_GetReturnValue(MAVLINK_BRIDGE_APP_VerifyCmdLength, bool);
}

void MAVLINK_BRIDGE_APP_ServiceSerial(void)
{
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_ServiceSerial, Basic, NULL);
}

void MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(uint8 BaseMode, uint8 SystemStatus)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_UpdateFromHeartbeat, uint8, BaseMode);
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_UpdateFromHeartbeat, uint8, SystemStatus);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_UpdateFromHeartbeat, Basic, NULL);
}

void MAVLINK_BRIDGE_APP_ProcessConfigCommand(const MAVLINK_BRIDGE_APP_ConfigCmdTlm_t *Msg)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_ProcessConfigCommand, const MAVLINK_BRIDGE_APP_ConfigCmdTlm_t *, Msg);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_ProcessConfigCommand, Basic, NULL);
}

void MAVLINK_BRIDGE_APP_StartMissionUpload(const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *Msg)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_StartMissionUpload, const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *, Msg);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_StartMissionUpload, Basic, NULL);
}

void MAVLINK_BRIDGE_APP_MissionQuery(const MAVLINK_BRIDGE_APP_MissionQueryCmd_t *Cmd)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_MissionQuery, const MAVLINK_BRIDGE_APP_MissionQueryCmd_t *, Cmd);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_MissionQuery, Basic, NULL);
}

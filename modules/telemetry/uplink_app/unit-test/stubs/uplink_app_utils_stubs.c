/************************************************************************
 * Stubs aligned to current uplink_app public/helper APIs
 ************************************************************************/

#include "uplink_app.h"
#include "uplink_app_utils.h"
#include "utgenstub.h"

void UPLINK_APP_ReportHousekeeping(void)
{
    UT_GenStub_Execute(UPLINK_APP_ReportHousekeeping, Basic, NULL);
}

bool UPLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_VerifyCmdLength, bool);
    UT_GenStub_AddParam(UPLINK_APP_VerifyCmdLength, const CFE_MSG_Message_t *, MsgPtr);
    UT_GenStub_AddParam(UPLINK_APP_VerifyCmdLength, size_t, ExpectedLength);
    UT_GenStub_Execute(UPLINK_APP_VerifyCmdLength, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_VerifyCmdLength, bool);
}

void UPLINK_APP_ServicePrototype(void)
{
    UT_GenStub_Execute(UPLINK_APP_ServicePrototype, Basic, NULL);
}

bool UPLINK_APP_ValidateProxyCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_Result_t *Result)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ValidateProxyCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ValidateProxyCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ValidateProxyCommand, UPLINK_APP_Result_t *, Result);
    UT_GenStub_Execute(UPLINK_APP_ValidateProxyCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ValidateProxyCommand, bool);
}

UPLINK_APP_RouteTarget_t UPLINK_APP_ResolveRouteTarget(uint8 CommandClass)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ResolveRouteTarget, UPLINK_APP_RouteTarget_t);
    UT_GenStub_AddParam(UPLINK_APP_ResolveRouteTarget, uint8, CommandClass);
    UT_GenStub_Execute(UPLINK_APP_ResolveRouteTarget, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ResolveRouteTarget, UPLINK_APP_RouteTarget_t);
}

bool UPLINK_APP_ParseRouteUpdatePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_RouteUpdatePayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ParseRouteUpdatePayload, bool);
    UT_GenStub_AddParam(UPLINK_APP_ParseRouteUpdatePayload, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ParseRouteUpdatePayload, UPLINK_APP_RouteUpdatePayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_ParseRouteUpdatePayload, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ParseRouteUpdatePayload, bool);
}

bool UPLINK_APP_PublishRouteUpdate(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, const UPLINK_APP_RouteUpdatePayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_PublishRouteUpdate, bool);
    UT_GenStub_AddParam(UPLINK_APP_PublishRouteUpdate, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_PublishRouteUpdate, const UPLINK_APP_RouteUpdatePayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_PublishRouteUpdate, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_PublishRouteUpdate, bool);
}

void UPLINK_APP_UpdateStatusTelemetry(uint32 NowMs)
{
    UT_GenStub_AddParam(UPLINK_APP_UpdateStatusTelemetry, uint32, NowMs);
    UT_GenStub_Execute(UPLINK_APP_UpdateStatusTelemetry, Basic, NULL);
}

uint16 UPLINK_APP_ComputeProxyCrc(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ComputeProxyCrc, uint16);
    UT_GenStub_AddParam(UPLINK_APP_ComputeProxyCrc, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ComputeProxyCrc, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ComputeProxyCrc, uint16);
}

bool UPLINK_APP_ForwardRecoveryCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardRecoveryCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardRecoveryCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ForwardRecoveryCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardRecoveryCommand, bool);
}

bool UPLINK_APP_ParseViewpointPayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                      UPLINK_APP_ViewpointPayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ParseViewpointPayload, bool);
    UT_GenStub_AddParam(UPLINK_APP_ParseViewpointPayload, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ParseViewpointPayload, UPLINK_APP_ViewpointPayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_ParseViewpointPayload, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ParseViewpointPayload, bool);
}

bool UPLINK_APP_ForwardViewpointCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                        const UPLINK_APP_ViewpointPayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardViewpointCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardViewpointCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ForwardViewpointCommand, const UPLINK_APP_ViewpointPayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_ForwardViewpointCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardViewpointCommand, bool);
}

bool UPLINK_APP_ForwardConfigCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardConfigCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardConfigCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ForwardConfigCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardConfigCommand, bool);
}

bool UPLINK_APP_ForwardModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardModeCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardModeCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ForwardModeCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardModeCommand, bool);
}

bool UPLINK_APP_ForwardDiagnosticCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardDiagnosticCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardDiagnosticCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ForwardDiagnosticCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardDiagnosticCommand, bool);
}

bool UPLINK_APP_ForwardCounterMgmtCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardCounterMgmtCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardCounterMgmtCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_Execute(UPLINK_APP_ForwardCounterMgmtCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardCounterMgmtCommand, bool);
}

bool UPLINK_APP_ParseFlightModePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                       UPLINK_APP_FlightModePayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ParseFlightModePayload, bool);
    UT_GenStub_AddParam(UPLINK_APP_ParseFlightModePayload, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ParseFlightModePayload, UPLINK_APP_FlightModePayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_ParseFlightModePayload, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ParseFlightModePayload, bool);
}

bool UPLINK_APP_ForwardFlightModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                         const UPLINK_APP_FlightModePayload_t *Payload)
{
    UT_GenStub_SetupReturnBuffer(UPLINK_APP_ForwardFlightModeCommand, bool);
    UT_GenStub_AddParam(UPLINK_APP_ForwardFlightModeCommand, const UPLINK_APP_ProcessUplinkCmd_t *, Cmd);
    UT_GenStub_AddParam(UPLINK_APP_ForwardFlightModeCommand, const UPLINK_APP_FlightModePayload_t *, Payload);
    UT_GenStub_Execute(UPLINK_APP_ForwardFlightModeCommand, Basic, NULL);
    return UT_GenStub_GetReturnValue(UPLINK_APP_ForwardFlightModeCommand, bool);
}

void UPLINK_APP_LoadState(void)
{
    UT_GenStub_Execute(UPLINK_APP_LoadState, Basic, NULL);
}

void UPLINK_APP_SaveState(void)
{
    UT_GenStub_Execute(UPLINK_APP_SaveState, Basic, NULL);
}

void UPLINK_APP_IncrementBootCount(void)
{
    UT_GenStub_Execute(UPLINK_APP_IncrementBootCount, Basic, NULL);
}

void UPLINK_APP_ProcessBootMarker(void)
{
    UT_GenStub_Execute(UPLINK_APP_ProcessBootMarker, Basic, NULL);
}

void UPLINK_APP_CheckBootSurvival(void)
{
    UT_GenStub_Execute(UPLINK_APP_CheckBootSurvival, Basic, NULL);
}

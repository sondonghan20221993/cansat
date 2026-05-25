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

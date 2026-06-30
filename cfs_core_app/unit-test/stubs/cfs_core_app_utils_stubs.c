/************************************************************************
 * Stubs aligned to current cfs_core_app public/helper APIs
 ************************************************************************/

#include "cfs_core_app.h"
#include "utgenstub.h"

void CFS_CORE_APP_ReportHousekeeping(void)
{
    UT_GenStub_Execute(CFS_CORE_APP_ReportHousekeeping, Basic, NULL);
}

bool CFS_CORE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    UT_GenStub_SetupReturnBuffer(CFS_CORE_APP_VerifyCmdLength, bool);
    UT_GenStub_AddParam(CFS_CORE_APP_VerifyCmdLength, const CFE_MSG_Message_t *, MsgPtr);
    UT_GenStub_AddParam(CFS_CORE_APP_VerifyCmdLength, size_t, ExpectedLength);
    UT_GenStub_Execute(CFS_CORE_APP_VerifyCmdLength, Basic, NULL);
    return UT_GenStub_GetReturnValue(CFS_CORE_APP_VerifyCmdLength, bool);
}

void CFS_CORE_APP_ServicePrototype(void)
{
    UT_GenStub_Execute(CFS_CORE_APP_ServicePrototype, Basic, NULL);
}

void CFS_CORE_APP_ProcessStateMessage(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(CFS_CORE_APP_ProcessStateMessage, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(CFS_CORE_APP_ProcessStateMessage, Basic, NULL);
}

void CFS_CORE_APP_UpdateHealth(uint32 NowMs, bool ForcePublish)
{
    UT_GenStub_AddParam(CFS_CORE_APP_UpdateHealth, uint32, NowMs);
    UT_GenStub_AddParam(CFS_CORE_APP_UpdateHealth, bool, ForcePublish);
    UT_GenStub_Execute(CFS_CORE_APP_UpdateHealth, Basic, NULL);
}

void CFS_CORE_APP_ProcessConfigCommand(const CFS_CORE_APP_ConfigCmdTlm_t *Msg)
{
    UT_GenStub_AddParam(CFS_CORE_APP_ProcessConfigCommand, const CFS_CORE_APP_ConfigCmdTlm_t *, Msg);
    UT_GenStub_Execute(CFS_CORE_APP_ProcessConfigCommand, Basic, NULL);
}

void CFS_CORE_APP_ProcessViewpointCommand(const CFS_CORE_APP_ViewpointCmdTlm_t *Msg)
{
    UT_GenStub_AddParam(CFS_CORE_APP_ProcessViewpointCommand, const CFS_CORE_APP_ViewpointCmdTlm_t *, Msg);
    UT_GenStub_Execute(CFS_CORE_APP_ProcessViewpointCommand, Basic, NULL);
}

void CFS_CORE_APP_ProcessRecoveryCommand(const CFS_CORE_APP_RecoveryCmdTlm_t *Msg)
{
    UT_GenStub_AddParam(CFS_CORE_APP_ProcessRecoveryCommand, const CFS_CORE_APP_RecoveryCmdTlm_t *, Msg);
    UT_GenStub_Execute(CFS_CORE_APP_ProcessRecoveryCommand, Basic, NULL);
}

void CFS_CORE_APP_ProcessModeCommand(const CFS_CORE_APP_ModeCmdTlm_t *Msg)
{
    UT_GenStub_AddParam(CFS_CORE_APP_ProcessModeCommand, const CFS_CORE_APP_ModeCmdTlm_t *, Msg);
    UT_GenStub_Execute(CFS_CORE_APP_ProcessModeCommand, Basic, NULL);
}

void CFS_CORE_APP_LoadState(void)
{
    UT_GenStub_Execute(CFS_CORE_APP_LoadState, Basic, NULL);
}

void CFS_CORE_APP_SaveState(void)
{
    UT_GenStub_Execute(CFS_CORE_APP_SaveState, Basic, NULL);
}

#include "lora_tdm_app_cmds.h"
#include "utgenstub.h"

CFE_Status_t LORA_TDM_APP_Noop(const LORA_TDM_APP_NoopCmd_t *Msg)
{
    UT_GenStub_AddParam(LORA_TDM_APP_Noop, const LORA_TDM_APP_NoopCmd_t *, Msg);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_Noop, CFE_Status_t);
    UT_GenStub_Execute(LORA_TDM_APP_Noop, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_Noop, CFE_Status_t);
}

CFE_Status_t LORA_TDM_APP_ResetCounters(const LORA_TDM_APP_ResetCountersCmd_t *Msg)
{
    UT_GenStub_AddParam(LORA_TDM_APP_ResetCounters, const LORA_TDM_APP_ResetCountersCmd_t *, Msg);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_ResetCounters, CFE_Status_t);
    UT_GenStub_Execute(LORA_TDM_APP_ResetCounters, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_ResetCounters, CFE_Status_t);
}

#include "lora_tdm_app.h"
#include "utgenstub.h"

CFE_Status_t LORA_TDM_APP_Init(void)
{
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_Init, CFE_Status_t);
    UT_GenStub_Execute(LORA_TDM_APP_Init, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_Init, CFE_Status_t);
}

void LORA_TDM_APP_Main(void)
{
    UT_GenStub_Execute(LORA_TDM_APP_Main, Basic, NULL);
}

void LORA_TDM_APP_RunCycle(void)
{
    UT_GenStub_Execute(LORA_TDM_APP_RunCycle, Basic, NULL);
}

void LORA_TDM_APP_ReportHousekeeping(void)
{
    UT_GenStub_Execute(LORA_TDM_APP_ReportHousekeeping, Basic, NULL);
}

void LORA_TDM_APP_ReportLinkStatus(void)
{
    UT_GenStub_Execute(LORA_TDM_APP_ReportLinkStatus, Basic, NULL);
}

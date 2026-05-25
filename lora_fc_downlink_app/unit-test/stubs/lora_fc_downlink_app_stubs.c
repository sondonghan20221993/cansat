#include "lora_fc_downlink_app.h"
#include "utgenstub.h"

CFE_Status_t LORA_FC_DOWNLINK_APP_Init(void)
{
    UT_GenStub_SetupReturnBuffer(LORA_FC_DOWNLINK_APP_Init, CFE_Status_t);
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_Init, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_FC_DOWNLINK_APP_Init, CFE_Status_t);
}

void LORA_FC_DOWNLINK_APP_Main(void)
{
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_Main, Basic, NULL);
}

#include "lora_tdm_app_dispatch.h"
#include "utgenstub.h"

void LORA_TDM_APP_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(LORA_TDM_APP_ProcessCommandPacket, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(LORA_TDM_APP_ProcessCommandPacket, Basic, NULL);
}

void LORA_TDM_APP_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(LORA_TDM_APP_ProcessGroundCommand, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(LORA_TDM_APP_ProcessGroundCommand, Basic, NULL);
}

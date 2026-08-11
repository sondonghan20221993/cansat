#ifndef LORA_TDM_APP_DISPATCH_H
#define LORA_TDM_APP_DISPATCH_H

#include "cfe.h"

void LORA_TDM_APP_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr);
void LORA_TDM_APP_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr);

#endif

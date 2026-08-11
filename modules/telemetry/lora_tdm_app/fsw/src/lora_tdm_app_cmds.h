#ifndef LORA_TDM_APP_CMDS_H
#define LORA_TDM_APP_CMDS_H

#include "cfe.h"
#include "lora_tdm_app_msg.h"

CFE_Status_t LORA_TDM_APP_Noop(const LORA_TDM_APP_NoopCmd_t *Msg);
CFE_Status_t LORA_TDM_APP_ResetCounters(const LORA_TDM_APP_ResetCountersCmd_t *Msg);
CFE_Status_t LORA_TDM_APP_SetDownlinkProtocol(const LORA_TDM_APP_SetDownlinkProtocolCmd_t *Msg);

#endif

#ifndef LORA_FC_DOWNLINK_APP_CMDS_H
#define LORA_FC_DOWNLINK_APP_CMDS_H

#include "lora_fc_downlink_app_msg.h"

void LORA_FC_DOWNLINK_APP_NoopCmd(const LORA_FC_DOWNLINK_APP_NoopCmd_t *cmd);
void LORA_FC_DOWNLINK_APP_ResetCountersCmd(const LORA_FC_DOWNLINK_APP_ResetCountersCmd_t *cmd);

#endif

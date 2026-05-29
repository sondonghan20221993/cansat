#ifndef LORA_FC_DOWNLINK_APP_UTILS_H
#define LORA_FC_DOWNLINK_APP_UTILS_H

#include "lora_fc_downlink_app.h"

void LORA_FC_DOWNLINK_APP_ReportHousekeeping(void);
void LORA_FC_DOWNLINK_APP_ProcessInputMessage(const CFE_SB_Buffer_t *sb_buf_ptr);

#endif

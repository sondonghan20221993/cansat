#ifndef LORA_FC_DOWNLINK_APP_DISPATCH_H
#define LORA_FC_DOWNLINK_APP_DISPATCH_H

#include "cfe_sb.h"

bool LORA_FC_DOWNLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *msg_ptr, size_t expected_length);
void LORA_FC_DOWNLINK_APP_TaskPipe(const CFE_SB_Buffer_t *sb_buf_ptr);

#endif

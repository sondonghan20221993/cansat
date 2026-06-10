#ifndef LORA_TDM_APP_TOPICIDS_H
#define LORA_TDM_APP_TOPICIDS_H

#include "default_lora_tdm_app_topicid_values.h"

/* Convenience aliases */
#define LORA_TDM_APP_CMD_MID        CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE)
#define LORA_TDM_APP_SEND_HK_MID   CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE)
#define LORA_TDM_APP_HK_TLM_MID    CFE_SB_ValueToMsgId(LORA_TDM_APP_HK_TLM_MID_VALUE)
#define LORA_TDM_APP_LINK_STATUS_MID CFE_SB_ValueToMsgId(LORA_TDM_APP_LINK_STATUS_MID_VALUE)

#endif

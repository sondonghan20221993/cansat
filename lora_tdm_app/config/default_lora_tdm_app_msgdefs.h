#ifndef DEFAULT_LORA_TDM_APP_MSGDEFS_H
#define DEFAULT_LORA_TDM_APP_MSGDEFS_H

#include "common_types.h"
#include "lora_tdm_app_fcncodes.h"

typedef enum
{
    LORA_TDM_APP_DIAG_ACTION_LINK_STATUS = 0,
    LORA_TDM_APP_DIAG_ACTION_RX_STATS    = 1,
    LORA_TDM_APP_DIAG_ACTION_TX_STATS    = 2
} LORA_TDM_APP_DiagAction_t;

#endif

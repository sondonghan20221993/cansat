#include "lora_tdm_app_cmds.h"
#include "lora_tdm_app.h"
#include "lora_tdm_app_eventids.h"

CFE_Status_t LORA_TDM_APP_Noop(const LORA_TDM_APP_NoopCmd_t *Msg)
{
    (void)Msg;

    LORA_TDM_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(LORA_TDM_APP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: NOOP command");
    return CFE_SUCCESS;
}

CFE_Status_t LORA_TDM_APP_ResetCounters(const LORA_TDM_APP_ResetCountersCmd_t *Msg)
{
    (void)Msg;

    LORA_TDM_APP_Data.CmdCounter   = 0;
    LORA_TDM_APP_Data.ErrCounter   = 0;
    LORA_TDM_APP_Data.TxCount      = 0;
    LORA_TDM_APP_Data.RxAckCount   = 0;
    LORA_TDM_APP_Data.RxCmdCount   = 0;
    LORA_TDM_APP_Data.RxErrorCount = 0;
    LORA_TDM_APP_Data.NoAckCount   = 0;

    CFE_EVS_SendEvent(LORA_TDM_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: ResetCounters command");
    return CFE_SUCCESS;
}

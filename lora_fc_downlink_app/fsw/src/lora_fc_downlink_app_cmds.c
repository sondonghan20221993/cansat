#include "lora_fc_downlink_app_cmds.h"
#include "lora_fc_downlink_app.h"
#include "lora_fc_downlink_app_eventids.h"
#include "lora_fc_downlink_app_version.h"

void LORA_FC_DOWNLINK_APP_NoopCmd(const LORA_FC_DOWNLINK_APP_NoopCmd_t *cmd)
{
    (void)cmd;
    LORA_FC_DOWNLINK_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LoRa FC Downlink App: NOOP command. Version %s", LORA_FC_DOWNLINK_APP_VERSION);
}

void LORA_FC_DOWNLINK_APP_ResetCountersCmd(const LORA_FC_DOWNLINK_APP_ResetCountersCmd_t *cmd)
{
    (void)cmd;
    LORA_FC_DOWNLINK_APP_Data.CmdCounter = 0;
    LORA_FC_DOWNLINK_APP_Data.ErrCounter = 0;
    LORA_FC_DOWNLINK_APP_Data.DownlinkCount = 0;
    CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LoRa FC Downlink App: RESET counters command");
}

#include "lora_fc_downlink_app_cmds.h"
#include "utgenstub.h"

void LORA_FC_DOWNLINK_APP_NoopCmd(const LORA_FC_DOWNLINK_APP_NoopCmd_t *cmd)
{
    UT_GenStub_AddParam(LORA_FC_DOWNLINK_APP_NoopCmd, const LORA_FC_DOWNLINK_APP_NoopCmd_t *, cmd);
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_NoopCmd, Basic, NULL);
}

void LORA_FC_DOWNLINK_APP_ResetCountersCmd(const LORA_FC_DOWNLINK_APP_ResetCountersCmd_t *cmd)
{
    UT_GenStub_AddParam(LORA_FC_DOWNLINK_APP_ResetCountersCmd, const LORA_FC_DOWNLINK_APP_ResetCountersCmd_t *, cmd);
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_ResetCountersCmd, Basic, NULL);
}

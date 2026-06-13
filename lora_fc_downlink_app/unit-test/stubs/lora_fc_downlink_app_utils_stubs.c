#include "lora_fc_downlink_app.h"
#include "utgenstub.h"

void LORA_FC_DOWNLINK_APP_ReportHousekeeping(void)
{
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_ReportHousekeeping, Basic, NULL);
}

void LORA_FC_DOWNLINK_APP_ProcessInputMessage(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    UT_GenStub_AddParam(LORA_FC_DOWNLINK_APP_ProcessInputMessage, const CFE_SB_Buffer_t *, sb_buf_ptr);
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_ProcessInputMessage, Basic, NULL);
}


#include "lora_fc_downlink_app_dispatch.h"
#include "utgenstub.h"

void LORA_FC_DOWNLINK_APP_TaskPipe(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    UT_GenStub_AddParam(LORA_FC_DOWNLINK_APP_TaskPipe, const CFE_SB_Buffer_t *, sb_buf_ptr);
    UT_GenStub_Execute(LORA_FC_DOWNLINK_APP_TaskPipe, Basic, NULL);
}

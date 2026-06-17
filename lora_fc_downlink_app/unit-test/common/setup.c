#include "common_types.h"

#include "setup.h"
#include "lora_fc_downlink_app.h"

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

void LORA_FC_DOWNLINK_APP_UT_Setup(void)
{
    UT_ResetState(0);
    memset(&LORA_FC_DOWNLINK_APP_Data, 0, sizeof(LORA_FC_DOWNLINK_APP_Data));
    LORA_FC_DOWNLINK_APP_Data.LoRaFd = -1;
}

void LORA_FC_DOWNLINK_APP_UT_TearDown(void)
{
}

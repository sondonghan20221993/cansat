#include "common_types.h"
#include "setup.h"
#include "lora_tdm_app.h"
#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

void LORA_TDM_APP_UT_Setup(void)
{
    UT_ResetState(0);
    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
}

void LORA_TDM_APP_UT_TearDown(void)
{
}

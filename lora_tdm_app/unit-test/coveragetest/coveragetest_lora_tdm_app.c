#include "lora_tdm_app_coveragetest_common.h"

void Test_Init(void)
{
    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RunStatus, (int)CFE_ES_RunStatus_APP_RUN);
}

void Test_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void UtTest_Setup(void)
{
    ADD_TEST(Init);
    ADD_TEST(Init_SubscribeError);
}

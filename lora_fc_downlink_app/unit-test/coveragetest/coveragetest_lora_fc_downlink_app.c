#include "lora_fc_downlink_app_coveragetest_common.h"

void Test_LORA_FC_DOWNLINK_APP_Init(void)
{
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

void Test_LORA_FC_DOWNLINK_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void UtTest_Setup(void)
{
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_SubscribeError);
}

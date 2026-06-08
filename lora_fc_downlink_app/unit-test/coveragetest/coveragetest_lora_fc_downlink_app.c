#include "lora_fc_downlink_app_coveragetest_common.h"

void Test_LORA_FC_DOWNLINK_APP_Init(void)
{
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

/* Subscribe #1(SEND_HK) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void Test_LORA_FC_DOWNLINK_APP_Init_EVSRegisterError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

void Test_LORA_FC_DOWNLINK_APP_Init_CreatePipeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_CreatePipe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #2(CMD) 실패 → SEND_HK 성공 후 CMD 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe2Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #3(FC_ATTITUDE_STATE) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe3Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #4(FC_EKF_LOCAL_STATE) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe4Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 4, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #5(FC_GPS_RAW_STATE) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe5Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 5, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #6(FC_EKF_STATUS) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe6Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 6, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #7(SYSTEM_HEALTH) 실패 */
void Test_LORA_FC_DOWNLINK_APP_Init_Subscribe7Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 7, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(LORA_FC_DOWNLINK_APP_Init(), CFE_SUCCESS);
}

void UtTest_Setup(void)
{
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_SubscribeError);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_EVSRegisterError);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_CreatePipeError);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe2Error);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe3Error);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe4Error);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe5Error);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe6Error);
    ADD_TEST(LORA_FC_DOWNLINK_APP_Init_Subscribe7Error);
}

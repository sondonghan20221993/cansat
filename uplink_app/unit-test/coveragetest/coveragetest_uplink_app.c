/************************************************************************
 * Coverage tests for uplink_app.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"

void Test_UPLINK_APP_Init(void)
{
    UtAssert_INT32_EQ(UPLINK_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(UPLINK_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

/* Subscribe #2(SEND_HK) 실패 */
void Test_UPLINK_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(UPLINK_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

void Test_UPLINK_APP_Init_EVSRegisterError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init HkTlm 실패 */
void Test_UPLINK_APP_Init_MsgInitHkError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init StatusTlm 실패 */
void Test_UPLINK_APP_Init_MsgInitStatusError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

void Test_UPLINK_APP_Init_CreatePipeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_CreatePipe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #1(CMD) 실패 */
void Test_UPLINK_APP_Init_Subscribe1Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #3(SYSTEM_HEALTH) 실패 */
void Test_UPLINK_APP_Init_Subscribe3Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(UPLINK_APP_Init(), CFE_SUCCESS);
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_Init);
    ADD_TEST(UPLINK_APP_Init_SubscribeError);
    ADD_TEST(UPLINK_APP_Init_EVSRegisterError);
    ADD_TEST(UPLINK_APP_Init_MsgInitHkError);
    ADD_TEST(UPLINK_APP_Init_MsgInitStatusError);
    ADD_TEST(UPLINK_APP_Init_CreatePipeError);
    ADD_TEST(UPLINK_APP_Init_Subscribe1Error);
    ADD_TEST(UPLINK_APP_Init_Subscribe3Error);
}

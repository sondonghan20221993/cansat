/************************************************************************
 * Coverage tests for cfs_core_app.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_Init(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

void Test_CFS_CORE_APP_Init_SubscribeError(void)
{
    /* 첫 번째 Subscribe(CMD_MID) 실패 */
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

/* CFE_EVS_Register 실패 → Init 실패 */
void Test_CFS_CORE_APP_Init_EVSRegisterError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init(HkTlm) 실패 → Init 실패 */
void Test_CFS_CORE_APP_Init_MsgInitError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* CFE_SB_CreatePipe 실패 → Init 실패 */
void Test_CFS_CORE_APP_Init_CreatePipeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_CreatePipe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe 2번째(SEND_HK) 실패 */
void Test_CFS_CORE_APP_Init_Subscribe2Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe 3번째(BRIDGE_HK) 실패 */
void Test_CFS_CORE_APP_Init_Subscribe3Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe 4~8번째 실패 */
void Test_CFS_CORE_APP_Init_Subscribe4Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 4, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

void Test_CFS_CORE_APP_Init_Subscribe5Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 5, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

void Test_CFS_CORE_APP_Init_Subscribe6Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 6, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

void Test_CFS_CORE_APP_Init_Subscribe7Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 7, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

void Test_CFS_CORE_APP_Init_Subscribe8Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 8, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #9(CONFIG_CMD_MID) 실패 */
void Test_CFS_CORE_APP_Init_Subscribe9Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 9, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
}

/* Init 후 런타임 타임아웃 기본값 확인 */
void Test_CFS_CORE_APP_Init_DefaultTimeouts(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.AttitudeTimeoutMs,
                      CFS_CORE_APP_ATTITUDE_TIMEOUT_MS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.GpsTimeoutMs,
                      CFS_CORE_APP_GPS_TIMEOUT_MS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ActiveConfig.PublishPeriodMs,
                      CFS_CORE_APP_PROTOTYPE_PERIOD_MS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.ConfigGeneration, 0);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Init);
    ADD_TEST(CFS_CORE_APP_Init_SubscribeError);
    ADD_TEST(CFS_CORE_APP_Init_EVSRegisterError);
    ADD_TEST(CFS_CORE_APP_Init_MsgInitError);
    ADD_TEST(CFS_CORE_APP_Init_CreatePipeError);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe2Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe3Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe4Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe5Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe6Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe7Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe8Error);
    ADD_TEST(CFS_CORE_APP_Init_Subscribe9Error);
    ADD_TEST(CFS_CORE_APP_Init_DefaultTimeouts);
}

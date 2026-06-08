#include "mavlink_bridge_app_coveragetest_common.h"

void Test_MAVLINK_BRIDGE_APP_Init(void)
{
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LinkState, (int32)MAVLINK_BRIDGE_LINK_DISCONNECTED);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.SerialFd, -1);
}

/* Subscribe #1(CMD) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

void Test_MAVLINK_BRIDGE_APP_Init_EVSRegisterError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_EVS_Register), 1, CFE_EVS_INVALID_PARAMETER);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init #1(HkTlm) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_MsgInit1Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init #2(EkfLocalTlm) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_MsgInit2Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init #3(AttitudeTlm) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_MsgInit3Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init #4(GpsRawTlm) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_MsgInit4Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 4, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* CFE_MSG_Init #5(EkfStatusTlm) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_MsgInit5Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_MSG_Init), 5, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

void Test_MAVLINK_BRIDGE_APP_Init_CreatePipeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_CreatePipe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #2(SEND_HK) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_Subscribe2Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 2, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

/* Subscribe #3(ROUTE_UPDATE) 실패 */
void Test_MAVLINK_BRIDGE_APP_Init_Subscribe3Error(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 3, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_NEQ(MAVLINK_BRIDGE_APP_Init(), CFE_SUCCESS);
}

void UtTest_Setup(void)
{
    ADD_TEST(MAVLINK_BRIDGE_APP_Init);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_SubscribeError);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_EVSRegisterError);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_MsgInit1Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_MsgInit2Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_MsgInit3Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_MsgInit4Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_MsgInit5Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_CreatePipeError);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_Subscribe2Error);
    ADD_TEST(MAVLINK_BRIDGE_APP_Init_Subscribe3Error);
}

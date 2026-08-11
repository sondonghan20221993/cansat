/************************************************************************
 * Coverage tests for cfs_core_app.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"
#include <stdlib.h>
#include <unistd.h>

void Test_CFS_CORE_APP_Init(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.RunStatus, CFE_ES_RunStatus_APP_RUN);
}

/* BL-41 route: Init이 FC_MISSION_READBACK_MID(0x1914) 구독을 포함하는지 —
 * 기존 15개 구독에 1개 추가돼 총 16회 호출돼야 한다 (TDD red) */
void Test_CFS_CORE_APP_Init_Subscribes_FcMissionReadback(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);

    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_SB_Subscribe)) == 16,
                  "Init() subscribes 16 MIDs (incl. FC_MISSION_READBACK_MID), got %u",
                  (unsigned int)UT_GetStubCount(UT_KEY(CFE_SB_Subscribe)));
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

/* BL-41(2026-07-23): Init()이 LoadState()를 호출해 저장된 ActiveConfig를
 * 실제로 복원하는지 배선 검증 — SaveState로 파일을 만들어 두고 Init 후
 * 기본값이 아닌 저장값이 로드됐는지 확인 */
/* BL-41(2026-07-23): Init→LoadState 배선 검증 — 이 테스트러너에서 utils는
 * stub이므로 값 복원은 utils 테스트(SaveLoadState_RoundTrip)가 담당하고,
 * 여기서는 Init이 LoadState를 실제로 호출하는지(stub count)만 증명한다. */
void Test_CFS_CORE_APP_Init_RestoresPersistedConfig(void)
{
    UtAssert_INT32_EQ(CFS_CORE_APP_Init(), CFE_SUCCESS);

    UtAssert_True(UT_GetStubCount(UT_KEY(CFS_CORE_APP_LoadState)) == 1,
                  "Init()이 LoadState()를 정확히 1회 호출");
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_Init);
    ADD_TEST(CFS_CORE_APP_Init_Subscribes_FcMissionReadback);
    ADD_TEST(CFS_CORE_APP_Init_RestoresPersistedConfig);
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

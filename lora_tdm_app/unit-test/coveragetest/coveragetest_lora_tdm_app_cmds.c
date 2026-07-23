#include "lora_tdm_app_coveragetest_common.h"
#include <stdlib.h>
#include <unistd.h>

void Test_Noop(void)
{
    LORA_TDM_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    LORA_TDM_APP_Data.CmdCounter = 0;

    LORA_TDM_APP_Noop(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ResetCounters(void)
{
    LORA_TDM_APP_ResetCountersCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    LORA_TDM_APP_Data.CmdCounter    = 9;
    LORA_TDM_APP_Data.ErrCounter    = 8;
    LORA_TDM_APP_Data.TxCount       = 7;
    LORA_TDM_APP_Data.RxAckCount    = 6;
    LORA_TDM_APP_Data.RxCmdCount    = 5;
    LORA_TDM_APP_Data.RxErrorCount  = 4;
    LORA_TDM_APP_Data.NoAckCount    = 3;

    LORA_TDM_APP_ResetCounters(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.TxCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxCmdCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxErrorCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.NoAckCount, 0);
}

void Test_SetDownlinkProtocol_ToV2(void)
{
    LORA_TDM_APP_SetDownlinkProtocolCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.UseV2 = 1;
    LORA_TDM_APP_Data.UseV2Downlink = 0;
    LORA_TDM_APP_Data.CmdCounter    = 0;

    LORA_TDM_APP_SetDownlinkProtocol(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

/* BL-41(2026-07-23): CONFIG_CMD_MID 경로만이 아니라 전용 지상 명령
 * (SET_DL_PROTO_CC) 경로도 UseV2Downlink를 바꾸므로 여기서도 SaveState
 * 배선이 있어야 재부팅 후 값이 유지된다 — 두 번째 변이 지점 커버 */
void Test_SetDownlinkProtocol_PersistsOnSuccess(void)
{
    LORA_TDM_APP_SetDownlinkProtocolCmd_t TestMsg;

    /* 이 테스트러너에서 utils는 stub — 파일 왕복은 utils 테스트(RoundTrip)가
     * 담당하고, 여기서는 성공 경로가 SaveState를 호출하는지(stub count)만 증명 */
    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.UseV2 = 1;
    LORA_TDM_APP_Data.UseV2Downlink = 0;

    LORA_TDM_APP_SetDownlinkProtocol(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 1);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_SaveState)) == 1,
                  "SetDownlinkProtocol 성공 시 SaveState() 1회 호출");
}

void Test_SetDownlinkProtocol_BackToV1(void)
{
    LORA_TDM_APP_SetDownlinkProtocolCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.UseV2 = 0;
    LORA_TDM_APP_Data.UseV2Downlink = 1;

    LORA_TDM_APP_SetDownlinkProtocol(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 0);
}

void Test_SetDownlinkProtocol_OutOfRangeRejected(void)
{
    /* BL-16(2026-07-21): 0/1 외 값은 거부(기체 엄격화), 이전엔 "0이 아니면
     * v2"로 정규화했음 — 지상 PARAM_BOUNDS(0,1)와 대칭 일치시킴. */
    LORA_TDM_APP_SetDownlinkProtocolCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    TestMsg.UseV2 = 0xFF;
    LORA_TDM_APP_Data.UseV2Downlink = 0;
    LORA_TDM_APP_Data.CmdCounter    = 0;
    LORA_TDM_APP_Data.ErrCounter    = 0;

    LORA_TDM_APP_SetDownlinkProtocol(&TestMsg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 0);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.ErrCounter, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(Noop);
    ADD_TEST(ResetCounters);
    ADD_TEST(SetDownlinkProtocol_ToV2);
    ADD_TEST(SetDownlinkProtocol_PersistsOnSuccess);
    ADD_TEST(SetDownlinkProtocol_BackToV1);
    ADD_TEST(SetDownlinkProtocol_OutOfRangeRejected);
}

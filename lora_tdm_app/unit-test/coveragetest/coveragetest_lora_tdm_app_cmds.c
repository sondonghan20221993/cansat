#include "lora_tdm_app_coveragetest_common.h"

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
    ADD_TEST(SetDownlinkProtocol_BackToV1);
    ADD_TEST(SetDownlinkProtocol_OutOfRangeRejected);
}

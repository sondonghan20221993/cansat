#include "lora_fc_downlink_app_coveragetest_common.h"

void Test_LORA_FC_DOWNLINK_APP_NoopCmd(void)
{
    LORA_FC_DOWNLINK_APP_NoopCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    LORA_FC_DOWNLINK_APP_NoopCmd(&TestMsg);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.CmdCounter, 1);
}

void Test_LORA_FC_DOWNLINK_APP_ResetCountersCmd(void)
{
    LORA_FC_DOWNLINK_APP_ResetCountersCmd_t TestMsg;

    memset(&TestMsg, 0, sizeof(TestMsg));
    LORA_FC_DOWNLINK_APP_Data.CmdCounter    = 9;
    LORA_FC_DOWNLINK_APP_Data.ErrCounter    = 8;
    LORA_FC_DOWNLINK_APP_Data.DownlinkCount = 7;

    LORA_FC_DOWNLINK_APP_ResetCountersCmd(&TestMsg);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.DownlinkCount, 0);
}

void UtTest_Setup(void)
{
    ADD_TEST(LORA_FC_DOWNLINK_APP_NoopCmd);
    ADD_TEST(LORA_FC_DOWNLINK_APP_ResetCountersCmd);
}

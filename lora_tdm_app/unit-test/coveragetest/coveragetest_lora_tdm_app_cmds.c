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

void UtTest_Setup(void)
{
    ADD_TEST(Noop);
    ADD_TEST(ResetCounters);
}

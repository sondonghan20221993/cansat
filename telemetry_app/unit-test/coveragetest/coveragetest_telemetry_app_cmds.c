/************************************************************************
 * Coverage tests for telemetry_app_cmds.c
 ************************************************************************/

#include "telemetry_app_coveragetest_common.h"

void Test_TELEMETRY_APP_SendHkCmd(void)
{
    TELEMETRY_APP_Data.ErrCounter                 = 2;
    TELEMETRY_APP_Data.CmdCounter                 = 3;
    TELEMETRY_APP_Data.CurrentState               = TELEMETRY_APP_LINK_DEGRADED;
    TELEMETRY_APP_Data.ActiveTransportId          = 4;
    TELEMETRY_APP_Data.LastUpdateAgeMs            = 1000;
    TELEMETRY_APP_Data.NominalTimeoutMs           = 0;
    TELEMETRY_APP_Data.DegradedTimeoutMs          = 1000;
    TELEMETRY_APP_Data.LostTimeoutMs              = 3000;
    TELEMETRY_APP_Data.DegradedTransitionCount    = 1;
    TELEMETRY_APP_Data.LostTransitionCount        = 2;
    TELEMETRY_APP_Data.RecoveryCount              = 3;

    UtAssert_INT32_EQ(TELEMETRY_APP_SendHkCmd(NULL), CFE_SUCCESS);
}

void Test_TELEMETRY_APP_NoopCmd(void)
{
    TELEMETRY_APP_NoopCmd_t TestMsg;
    memset(&TestMsg, 0, sizeof(TestMsg));

    UtAssert_INT32_EQ(TELEMETRY_APP_NoopCmd(&TestMsg), CFE_SUCCESS);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CmdCounter, 1);
}

void Test_TELEMETRY_APP_ResetCountersCmd(void)
{
    TELEMETRY_APP_ResetCountersCmd_t TestMsg;
    memset(&TestMsg, 0, sizeof(TestMsg));

    TELEMETRY_APP_Data.CmdCounter              = 9;
    TELEMETRY_APP_Data.ErrCounter              = 8;
    TELEMETRY_APP_Data.DegradedTransitionCount = 7;
    TELEMETRY_APP_Data.LostTransitionCount     = 6;
    TELEMETRY_APP_Data.RecoveryCount           = 5;

    UtAssert_INT32_EQ(TELEMETRY_APP_ResetCountersCmd(&TestMsg), CFE_SUCCESS);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.ErrCounter, 0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.DegradedTransitionCount, 0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.LostTransitionCount, 0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.RecoveryCount, 0);
}

void UtTest_Setup(void)
{
    ADD_TEST(TELEMETRY_APP_SendHkCmd);
    ADD_TEST(TELEMETRY_APP_NoopCmd);
    ADD_TEST(TELEMETRY_APP_ResetCountersCmd);
}

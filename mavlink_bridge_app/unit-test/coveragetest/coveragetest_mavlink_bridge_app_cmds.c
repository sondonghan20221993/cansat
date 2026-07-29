/************************************************************************
 * Coverage tests for mavlink_bridge_app_cmds.c
 ************************************************************************/

#include "mavlink_bridge_app_coveragetest_common.h"

void Test_MAVLINK_BRIDGE_APP_Noop(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), true);

    MAVLINK_BRIDGE_APP_Noop(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 1);
}

/* VerifyCmdLength 실패 시 Noop → 즉시 반환 (CmdCounter 갱신 없음) */
void Test_MAVLINK_BRIDGE_APP_Noop_LengthCheckFail(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), false);

    MAVLINK_BRIDGE_APP_Data.CmdCounter = 0;

    MAVLINK_BRIDGE_APP_Noop(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 0);
}

void Test_MAVLINK_BRIDGE_APP_ResetCounters(void)
{
    MAVLINK_BRIDGE_APP_ResetCountersCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), true);

    MAVLINK_BRIDGE_APP_Data.CmdCounter    = 5;
    MAVLINK_BRIDGE_APP_Data.ErrCounter    = 3;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 2;

    MAVLINK_BRIDGE_APP_ResetCountersCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter,     0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter,     0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* VerifyCmdLength 실패 시 ResetCountersCmd → 즉시 반환 (카운터 갱신 없음) */
void Test_MAVLINK_BRIDGE_APP_ResetCounters_LengthCheckFail(void)
{
    MAVLINK_BRIDGE_APP_ResetCountersCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    UT_SetDefaultReturnValue(UT_KEY(MAVLINK_BRIDGE_APP_VerifyCmdLength), false);

    MAVLINK_BRIDGE_APP_Data.CmdCounter      = 5;
    MAVLINK_BRIDGE_APP_Data.ErrCounter      = 3;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 2;

    MAVLINK_BRIDGE_APP_ResetCountersCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter,     5);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter,     3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 2);
}

void UtTest_Setup(void)
{
    ADD_TEST(MAVLINK_BRIDGE_APP_Noop);
    ADD_TEST(MAVLINK_BRIDGE_APP_Noop_LengthCheckFail);
    ADD_TEST(MAVLINK_BRIDGE_APP_ResetCounters);
    ADD_TEST(MAVLINK_BRIDGE_APP_ResetCounters_LengthCheckFail);
}

/************************************************************************
 * Coverage tests for mavlink_bridge_app_cmds.c
 ************************************************************************/

#include "mavlink_bridge_app_coveragetest_common.h"

void Test_MAVLINK_BRIDGE_APP_Noop(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    MAVLINK_BRIDGE_APP_Noop(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 1);
}

void Test_MAVLINK_BRIDGE_APP_ResetCounters(void)
{
    MAVLINK_BRIDGE_APP_ResetCountersCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    MAVLINK_BRIDGE_APP_Data.CmdCounter    = 5;
    MAVLINK_BRIDGE_APP_Data.ErrCounter    = 3;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 2;

    MAVLINK_BRIDGE_APP_ResetCountersCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter,     0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter,     0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

void UtTest_Setup(void)
{
    ADD_TEST(MAVLINK_BRIDGE_APP_Noop);
    ADD_TEST(MAVLINK_BRIDGE_APP_ResetCounters);
}

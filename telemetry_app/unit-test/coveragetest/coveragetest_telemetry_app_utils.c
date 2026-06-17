/************************************************************************
 * Coverage tests for telemetry_app_utils.c
 ************************************************************************/

#include "telemetry_app_coveragetest_common.h"

void Test_TELEMETRY_APP_EvaluateLinkState(void)
{
    TELEMETRY_APP_Data.DegradedTimeoutMs = 1000;
    TELEMETRY_APP_Data.LostTimeoutMs     = 3000;
    TELEMETRY_APP_Data.CurrentState      = TELEMETRY_APP_LINK_ALIVE;

    TELEMETRY_APP_ProcessGroundUpdate(0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CurrentState, TELEMETRY_APP_LINK_ALIVE);

    TELEMETRY_APP_ProcessGroundUpdate(1000);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CurrentState, TELEMETRY_APP_LINK_DEGRADED);

    TELEMETRY_APP_ProcessGroundUpdate(3000);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CurrentState, TELEMETRY_APP_LINK_LOST);

    TELEMETRY_APP_ProcessGroundUpdate(0);
    UtAssert_INT32_EQ(TELEMETRY_APP_Data.CurrentState, TELEMETRY_APP_LINK_ALIVE);
}

void Test_TELEMETRY_APP_ReportHousekeeping(void)
{
    TELEMETRY_APP_Data.ErrCounter              = 1;
    TELEMETRY_APP_Data.CmdCounter              = 2;
    TELEMETRY_APP_Data.ActiveTransportId       = 3;
    TELEMETRY_APP_Data.LastUpdateAgeMs         = 4;
    TELEMETRY_APP_Data.NominalTimeoutMs        = 5;
    TELEMETRY_APP_Data.DegradedTimeoutMs       = 6;
    TELEMETRY_APP_Data.LostTimeoutMs           = 7;
    TELEMETRY_APP_Data.DegradedTransitionCount = 8;
    TELEMETRY_APP_Data.LostTransitionCount     = 9;
    TELEMETRY_APP_Data.RecoveryCount           = 10;

    TELEMETRY_APP_ReportHousekeeping();
    TELEMETRY_APP_PublishStatus();
}

void UtTest_Setup(void)
{
    ADD_TEST(TELEMETRY_APP_EvaluateLinkState);
    ADD_TEST(TELEMETRY_APP_ReportHousekeeping);
}

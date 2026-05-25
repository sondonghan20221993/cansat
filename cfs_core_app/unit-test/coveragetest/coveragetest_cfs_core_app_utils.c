/************************************************************************
 * Coverage tests for cfs_core_app_utils.c
 ************************************************************************/

#include "cfs_core_app_coveragetest_common.h"

void Test_CFS_CORE_APP_ReportHousekeeping(void)
{
    CFS_CORE_APP_Data.CmdCounter       = 2;
    CFS_CORE_APP_Data.ErrCounter       = 1;
    CFS_CORE_APP_Data.PublishCount     = 3;
    CFS_CORE_APP_Data.LastPublishTimeMs = 44;

    CFS_CORE_APP_ReportHousekeeping();
}

void Test_CFS_CORE_APP_UpdateHealth_Nominal(void)
{
    uint32 NowMs = 5000;

    CFS_CORE_APP_Data.AttitudeState.Received  = true;
    CFS_CORE_APP_Data.AttitudeState.TimestampMs = 4900;
    CFS_CORE_APP_Data.AttitudeState.Valid     = 1;
    CFS_CORE_APP_Data.LocalState.Received     = true;
    CFS_CORE_APP_Data.LocalState.TimestampMs  = 4900;
    CFS_CORE_APP_Data.LocalState.Valid        = 1;
    CFS_CORE_APP_Data.GpsState.Received       = true;
    CFS_CORE_APP_Data.GpsState.TimestampMs    = 4800;
    CFS_CORE_APP_Data.GpsState.Valid          = 1;
    CFS_CORE_APP_Data.EkfState.Received       = true;
    CFS_CORE_APP_Data.EkfState.TimestampMs    = 4900;
    CFS_CORE_APP_Data.EkfState.Valid          = 1;
    CFS_CORE_APP_Data.BridgeState.Received    = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 4900;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_NOMINAL);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_NONE);
}

void Test_CFS_CORE_APP_UpdateHealth_Recovery(void)
{
    uint32 NowMs = 10000;

    memset(&CFS_CORE_APP_Data.SystemHealthTlm, 0, sizeof(CFS_CORE_APP_Data.SystemHealthTlm));
    CFS_CORE_APP_Data.BridgeState.Received          = true;
    CFS_CORE_APP_Data.BridgeState.LastRxTimestampMs = 1000;

    CFS_CORE_APP_UpdateHealth(NowMs, true);

    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.HealthState, CFS_CORE_APP_HEALTH_RECOVERY);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.FaultCode, CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT);
    UtAssert_INT32_EQ(CFS_CORE_APP_Data.SystemHealthTlm.RecoveryRequested, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(CFS_CORE_APP_ReportHousekeeping);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Nominal);
    ADD_TEST(CFS_CORE_APP_UpdateHealth_Recovery);
}

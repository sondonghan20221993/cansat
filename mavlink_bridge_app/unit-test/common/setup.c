#include "common_types.h"
#include "setup.h"
#include "mavlink_bridge_app.h"
#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

void MAVLINK_BRIDGE_APP_UT_Setup(void)
{
    UT_ResetState(0);
    memset(&MAVLINK_BRIDGE_APP_Data, 0, sizeof(MAVLINK_BRIDGE_APP_Data));
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs        = MAVLINK_BRIDGE_APP_ATTITUDE_INTERVAL_US;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.LocalPositionIntervalUs   = MAVLINK_BRIDGE_APP_LOCAL_POSITION_INTERVAL_US;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.GlobalPositionIntervalUs  = MAVLINK_BRIDGE_APP_GLOBAL_POSITION_INTERVAL_US;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.GpsRawIntervalUs          = MAVLINK_BRIDGE_APP_GPS_RAW_INTERVAL_US;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.EkfStatusIntervalUs       = MAVLINK_BRIDGE_APP_EKF_STATUS_INTERVAL_US;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs       = MAVLINK_BRIDGE_APP_RECONNECT_INTERVAL_MS;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.HeartbeatIntervalMs       = MAVLINK_BRIDGE_APP_HEARTBEAT_INTERVAL_MS;
    MAVLINK_BRIDGE_APP_Data.PendingConfig  = MAVLINK_BRIDGE_APP_Data.ActiveConfig;
    MAVLINK_BRIDGE_APP_Data.PreviousConfig = MAVLINK_BRIDGE_APP_Data.ActiveConfig;
}

void MAVLINK_BRIDGE_APP_UT_TearDown(void)
{
}

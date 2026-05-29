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
}

void MAVLINK_BRIDGE_APP_UT_TearDown(void)
{
}

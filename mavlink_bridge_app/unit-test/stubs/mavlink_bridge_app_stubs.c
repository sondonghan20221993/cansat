#include "mavlink_bridge_app.h"
#include "utgenstub.h"

void MAV_BRIDGE_APP_Main(void)
{
    UT_GenStub_Execute(MAV_BRIDGE_APP_Main, Basic, NULL);
}

CFE_Status_t MAVLINK_BRIDGE_APP_Init(void)
{
    UT_GenStub_SetupReturnBuffer(MAVLINK_BRIDGE_APP_Init, CFE_Status_t);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_Init, Basic, NULL);
    return UT_GenStub_GetReturnValue(MAVLINK_BRIDGE_APP_Init, CFE_Status_t);
}

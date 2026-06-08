#include "mavlink_bridge_app.h"
#include "mavlink_bridge_app_dispatch.h"
#include "utgenstub.h"

void MAVLINK_BRIDGE_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(MAVLINK_BRIDGE_APP_TaskPipe, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(MAVLINK_BRIDGE_APP_TaskPipe, Basic, NULL);
}

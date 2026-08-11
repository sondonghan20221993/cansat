/************************************************************************
 * Stubs aligned to current uplink_app_dispatch.h
 ************************************************************************/

#include "uplink_app_dispatch.h"
#include "utgenstub.h"

void UPLINK_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(UPLINK_APP_TaskPipe, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(UPLINK_APP_TaskPipe, Basic, NULL);
}

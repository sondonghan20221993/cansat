/************************************************************************
 * Stubs aligned to current cfs_core_app_dispatch.h
 ************************************************************************/

#include "cfs_core_app_dispatch.h"
#include "utgenstub.h"

void CFS_CORE_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    UT_GenStub_AddParam(CFS_CORE_APP_TaskPipe, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_Execute(CFS_CORE_APP_TaskPipe, Basic, NULL);
}

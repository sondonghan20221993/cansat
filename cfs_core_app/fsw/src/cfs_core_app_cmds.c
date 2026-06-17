#include "cfs_core_app_cmds.h"
#include "cfs_core_app_eventids.h"

void CFS_CORE_APP_Noop(const CFS_CORE_APP_NoopCmd_t *Cmd)
{
    (void)Cmd;
    CFS_CORE_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(CFS_CORE_APP_NOOP_EID, CFE_EVS_EventType_INFORMATION, "CFS_CORE_APP: NOOP");
}

void CFS_CORE_APP_ResetCounters(const CFS_CORE_APP_ResetCountersCmd_t *Cmd)
{
    (void)Cmd;
    CFS_CORE_APP_Data.CmdCounter = 0;
    CFS_CORE_APP_Data.ErrCounter = 0;
    CFE_EVS_SendEvent(CFS_CORE_APP_RESET_EID, CFE_EVS_EventType_INFORMATION, "CFS_CORE_APP: RESET");
}


#include "img_app_cmds.h"
#include "img_app_eventids.h"

void IMG_APP_Noop(const IMG_APP_NoopCmd_t *Cmd)
{
    (void)Cmd;
    IMG_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(IMG_APP_NOOP_EID, CFE_EVS_EventType_INFORMATION, "IMG_APP: NOOP");
}

void IMG_APP_ResetCounters(const IMG_APP_ResetCountersCmd_t *Cmd)
{
    (void)Cmd;
    IMG_APP_Data.CmdCounter = 0;
    IMG_APP_Data.ErrCounter = 0;
    CFE_EVS_SendEvent(IMG_APP_RESET_EID, CFE_EVS_EventType_INFORMATION, "IMG_APP: RESET");
}

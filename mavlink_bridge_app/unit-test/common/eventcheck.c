#include "common_types.h"
#include "cfe_evs.h"

#include "eventcheck.h"

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

static int32 UT_CheckEvent_Hook(void *UserObj, int32 StubRetcode, uint32 CallCount, const UT_StubContext_t *Context,
                                va_list va)
{
    UT_CheckEvent_t *State = UserObj;
    uint16           EventId;
    const char *     Spec;

    (void)StubRetcode;
    (void)CallCount;
    (void)va;

    if (Context->ArgCount > 0)
    {
        EventId = UT_Hook_GetArgValueByName(Context, "EventID", uint16);
        if (EventId == State->ExpectedEvent)
        {
            if (State->ExpectedFormat != NULL)
            {
                Spec = UT_Hook_GetArgValueByName(Context, "Spec", const char *);
                if (Spec != NULL && strcmp(Spec, State->ExpectedFormat) == 0)
                {
                    ++State->MatchCount;
                }
            }
            else
            {
                ++State->MatchCount;
            }
        }
    }

    return 0;
}

void UT_CheckEvent_Setup_Impl(UT_CheckEvent_t *Evt, uint16 ExpectedEvent, const char *EventName,
                              const char *ExpectedFormat)
{
    if (ExpectedFormat == NULL)
    {
        UtPrintf("CheckEvent will match: %s(%u)", EventName, ExpectedEvent);
    }
    else
    {
        UtPrintf("CheckEvent will match: %s(%u), \"%s\"", EventName, ExpectedEvent, ExpectedFormat);
    }

    memset(Evt, 0, sizeof(*Evt));
    Evt->ExpectedEvent  = ExpectedEvent;
    Evt->ExpectedFormat = ExpectedFormat;
    UT_SetVaHookFunction(UT_KEY(CFE_EVS_SendEvent), UT_CheckEvent_Hook, Evt);
}

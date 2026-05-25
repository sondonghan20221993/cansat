#include "uplink_app_utils.h"
#include "uplink_app_eventids.h"

#include <stdio.h>
#include <string.h>

static uint32 UPLINK_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

void UPLINK_APP_UpdateStatusTelemetry(uint32 NowMs)
{
    UPLINK_APP_StatusTlm_t *Tlm;

    if (NowMs == 0)
    {
        NowMs = UPLINK_APP_GetTimeMs();
    }

    Tlm = &UPLINK_APP_Data.StatusTlm;

    UPLINK_APP_Data.LastPublishTimeMs = NowMs;
    UPLINK_APP_Data.PublishCount++;
    UPLINK_APP_Data.SequenceCounter++;

    memset(Tlm, 0, sizeof(*Tlm));
    CFE_MSG_Init(CFE_MSG_PTR(Tlm->TelemetryHeader), CFE_SB_ValueToMsgId(UPLINK_STATUS_MID), sizeof(*Tlm));
    Tlm->Seq                 = UPLINK_APP_Data.SequenceCounter;
    Tlm->TimestampMs         = NowMs;
    Tlm->LastRxTimestampMs   = UPLINK_APP_Data.LastRxTimeMs;
    Tlm->AcceptedCount       = UPLINK_APP_Data.AcceptedCount;
    Tlm->RejectedCount       = UPLINK_APP_Data.RejectedCount;
    Tlm->RoutingFailureCount = UPLINK_APP_Data.RoutingFailureCount;
    Tlm->LastCommandCode     = UPLINK_APP_Data.LastCommandCode;
    Tlm->LastCommandResult   = UPLINK_APP_Data.LastCommandResult;
    Tlm->LinkState           = UPLINK_APP_Data.LinkState;
    Tlm->LastRouteTarget     = UPLINK_APP_Data.LastRouteTarget;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
}

void UPLINK_APP_ReportHousekeeping(void)
{
    UPLINK_APP_Data.HkTlm.CommandCounter       = UPLINK_APP_Data.CmdCounter;
    UPLINK_APP_Data.HkTlm.CommandErrorCounter  = UPLINK_APP_Data.ErrCounter;
    UPLINK_APP_Data.HkTlm.PublishCount         = UPLINK_APP_Data.PublishCount;
    UPLINK_APP_Data.HkTlm.LastPublishTimestampMs = UPLINK_APP_Data.LastPublishTimeMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(UPLINK_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(UPLINK_APP_Data.HkTlm.TelemetryHeader), true);
}

bool UPLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        UPLINK_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(UPLINK_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "UPLINK_APP: Invalid cmd length expected=%lu actual=%lu",
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }

    return true;
}

bool UPLINK_APP_ValidateProxyCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_Result_t *Result)
{
    if (Cmd->Version != UPLINK_APP_PROTOCOL_VERSION)
    {
        *Result = UPLINK_APP_RESULT_REJECT_VERSION;
        return false;
    }

    if (Cmd->PayloadLength > UPLINK_APP_MAX_PAYLOAD_LENGTH)
    {
        *Result = UPLINK_APP_RESULT_REJECT_LENGTH;
        return false;
    }

    switch (Cmd->CommandClass)
    {
        case UPLINK_APP_CLASS_CONFIG:
        case UPLINK_APP_CLASS_ROUTE_UPDATE:
        case UPLINK_APP_CLASS_VIEWPOINT:
        case UPLINK_APP_CLASS_RECOVERY:
        case UPLINK_APP_CLASS_MODE:
        case UPLINK_APP_CLASS_DIAGNOSTIC:
            break;

        default:
            *Result = UPLINK_APP_RESULT_REJECT_CLASS;
            return false;
    }

    if ((Cmd->CommandClass == UPLINK_APP_CLASS_ROUTE_UPDATE ||
         Cmd->CommandClass == UPLINK_APP_CLASS_VIEWPOINT) &&
        Cmd->PayloadLength == 0)
    {
        *Result = UPLINK_APP_RESULT_REJECT_LENGTH;
        return false;
    }

    *Result = UPLINK_APP_RESULT_ACCEPT;
    return true;
}

UPLINK_APP_RouteTarget_t UPLINK_APP_ResolveRouteTarget(uint8 CommandClass)
{
    switch (CommandClass)
    {
        case UPLINK_APP_CLASS_CONFIG:
        case UPLINK_APP_CLASS_ROUTE_UPDATE:
        case UPLINK_APP_CLASS_VIEWPOINT:
        case UPLINK_APP_CLASS_RECOVERY:
        case UPLINK_APP_CLASS_MODE:
            return UPLINK_APP_ROUTE_CORE;

        case UPLINK_APP_CLASS_DIAGNOSTIC:
            return UPLINK_APP_ROUTE_DOWNLINK;

        default:
            return UPLINK_APP_ROUTE_NONE;
    }
}

void UPLINK_APP_ServicePrototype(void)
{
    uint32                NowMs;

    NowMs = UPLINK_APP_GetTimeMs();
    if ((NowMs - UPLINK_APP_Data.LastPublishTimeMs) < UPLINK_APP_PROTOTYPE_PERIOD_MS)
    {
        return;
    }
    UPLINK_APP_UpdateStatusTelemetry(NowMs);
}


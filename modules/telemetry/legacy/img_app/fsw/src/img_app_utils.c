#include "img_app_utils.h"
#include "img_app_eventids.h"

#include <stdio.h>
#include <string.h>

static uint32 IMG_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

void IMG_APP_ReportHousekeeping(void)
{
    IMG_APP_Data.HkTlm.CommandCounter       = IMG_APP_Data.CmdCounter;
    IMG_APP_Data.HkTlm.CommandErrorCounter  = IMG_APP_Data.ErrCounter;
    IMG_APP_Data.HkTlm.PublishCount         = IMG_APP_Data.PublishCount;
    IMG_APP_Data.HkTlm.LastSnapshotTimestampMs = IMG_APP_Data.LastPublishTimeMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(IMG_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(IMG_APP_Data.HkTlm.TelemetryHeader), true);
}

bool IMG_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        IMG_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(IMG_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "IMG_APP: Invalid cmd length expected=%lu actual=%lu",
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }

    return true;
}

void IMG_APP_ServicePrototype(void)
{
    uint32                NowMs;
    IMG_APP_ImageMetaTlm_t *Tlm;

    NowMs = IMG_APP_GetTimeMs();
    if ((NowMs - IMG_APP_Data.LastPublishTimeMs) < IMG_APP_PROTOTYPE_PERIOD_MS)
    {
        return;
    }

    Tlm = &IMG_APP_Data.ImageMetaTlm;

    IMG_APP_Data.LastPublishTimeMs = NowMs;
    IMG_APP_Data.PublishCount++;
    IMG_APP_Data.SequenceCounter++;

    memset(Tlm, 0, sizeof(*Tlm));
    CFE_MSG_Init(CFE_MSG_PTR(Tlm->TelemetryHeader), CFE_SB_ValueToMsgId(IMAGE_META_MID_VALUE), sizeof(*Tlm));
    Tlm->Seq                  = IMG_APP_Data.SequenceCounter;
    Tlm->SnapshotTimestampMs  = NowMs;
    Tlm->CaptureTimestampMs   = 0;
    Tlm->RxTimestampMs        = NowMs;
    Tlm->AttitudeRefTimestampMs = NowMs;
    Tlm->EkfRefTimestampMs    = NowMs;
    Tlm->GpsRefTimestampMs    = NowMs;
    Tlm->TimestampQuality     = IMG_APP_TIMESTAMP_QUALITY_CFS_SNAPSHOT_ESTIMATE;
    Tlm->TransferStatus       = IMG_APP_TRANSFER_AVAILABLE;
    Tlm->CorrelationStatus    = IMG_APP_CORRELATION_ESTIMATED;
    snprintf(Tlm->ImageId, sizeof(Tlm->ImageId), "img-%06lu", (unsigned long)Tlm->Seq);
    snprintf(Tlm->CameraId, sizeof(Tlm->CameraId), "cam0");
    snprintf(Tlm->ArtifactRef, sizeof(Tlm->ArtifactRef), "wlan://camera/img-%06lu.jpg", (unsigned long)Tlm->Seq);

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);

    CFE_EVS_SendEvent(IMG_APP_PUBLISH_EID, CFE_EVS_EventType_INFORMATION,
                      "IMG_APP: published prototype IMAGE_META_MID image_id=%s seq=%lu",
                      Tlm->ImageId, (unsigned long)Tlm->Seq);
}

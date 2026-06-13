#ifndef DEFAULT_IMG_APP_MSGSTRUCT_H
#define DEFAULT_IMG_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "img_app_msgdefs.h"
#include "img_app_mission_cfg.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} IMG_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} IMG_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint16                    Reserved;
    uint32                    PublishCount;
    uint32                    LastSnapshotTimestampMs;
} IMG_APP_HkTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    SnapshotTimestampMs;
    uint32                    CaptureTimestampMs;
    uint32                    RxTimestampMs;
    uint32                    AttitudeRefTimestampMs;
    uint32                    EkfRefTimestampMs;
    uint32                    GpsRefTimestampMs;
    int32                     AttitudeDeltaMs;
    int32                     EkfDeltaMs;
    int32                     GpsDeltaMs;
    uint8                     TimestampQuality;
    uint8                     TransferStatus;
    uint8                     CorrelationStatus;
    uint8                     Reserved;
    char                      ImageId[IMG_APP_IMAGE_ID_LEN];
    char                      CameraId[IMG_APP_CAMERA_ID_LEN];
    char                      ArtifactRef[IMG_APP_ARTIFACT_REF_LEN];
} IMG_APP_ImageMetaTlm_t;

#endif

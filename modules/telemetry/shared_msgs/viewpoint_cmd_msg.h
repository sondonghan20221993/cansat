#ifndef VIEWPOINT_CMD_MSG_H
#define VIEWPOINT_CMD_MSG_H

#include "cfe.h"

/*
 * uplink_app 발행 (VIEWPOINT_CMD_MID 0x190D), cfs_core_app 구독 — 단일 진실.
 * 필드 추가는 항상 끝에 append (notes/temp/mirror_struct_layout_audit.md 참조).
 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     ViewpointType;
    uint8                     PositionFrame;
    float                     X;
    float                     Y;
    float                     Z;
    float                     Yaw;
    float                     Pitch;
    uint32                    HoldTimeMs;
} VIEWPOINT_CMD_TLM_t;

#endif

#ifndef BRIDGE_HK_MSG_H
#define BRIDGE_HK_MSG_H

#include "cfe.h"

/*
 * mavlink_bridge_app 발행, cfs_core_app 구독 — 단일 진실.
 * 필드 추가는 항상 끝에 append (notes/temp/mirror_struct_layout_audit.md 참조).
 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint8                     LinkState;
    uint8                     LastErrorCode;
    uint32                    BytesReceived;
    uint32                    ReconnectAttemptCount;
    uint32                    ParseErrorCount;
    uint32                    NonFiniteValueCount;
    uint32                    LastRxTimestampMs;
    uint32                    MissionUploadSuccessCount;
    uint32                    MissionUploadFailCount;
    uint32                    LastUploadTimestampMs;
    uint8                     LastUploadWaypointCount;
    uint8                     LastUploadResult;
    uint16                    HkSpare;
} BRIDGE_HK_TLM_t;

#endif

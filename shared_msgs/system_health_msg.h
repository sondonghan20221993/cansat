#ifndef SYSTEM_HEALTH_MSG_H
#define SYSTEM_HEALTH_MSG_H

#include "cfe.h"

/*
 * cfs_core_app 발행, lora_tdm_app 구독 — 단일 진실.
 * 필드 추가는 항상 끝에 append (notes/temp/mirror_struct_layout_audit.md 참조).
 */
typedef struct
{
    uint8 Valid;
    uint8 Stale;
    uint8 ErrorCode;
    uint8 TimedOut;
} INPUT_STATUS_t;

typedef struct
{
    uint8 LinkState;
    uint8 ErrorCode;
    uint8 TimedOut;
    uint8 Reserved;
} BRIDGE_STATUS_t;

typedef struct
{
    uint8 TimedOut;
    uint8 Reserved[3];
} APP_STATUS_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastValidInputTimestampMs;
    uint8                     HealthState;
    uint8                     FaultCode;
    uint8                     RecoveryRequested;
    uint8                     Reserved;
    INPUT_STATUS_t            AttitudeStatus;
    INPUT_STATUS_t            LocalStatus;
    INPUT_STATUS_t            GpsStatus;
    INPUT_STATUS_t            EkfStatus;
    BRIDGE_STATUS_t           BridgeStatus;
    APP_STATUS_t              UplinkStatus;
    APP_STATUS_t              LoraStatus;
} SYSTEM_HEALTH_TLM_t;

#endif

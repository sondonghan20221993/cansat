#ifndef FC_STATE_MSG_H
#define FC_STATE_MSG_H

#include "cfe.h"

/*
 * mavlink_bridge_app 발행, cfs_core_app + lora_tdm_app 구독(삼중 진실) —
 * 단일 진실. 필드 추가는 항상 끝에 append
 * (notes/temp/mirror_struct_layout_audit.md 참조).
 *
 * FC_STATE_PREFIX_t: 4종 전부가 공유하는 공통 prefix — cfs_core_app이
 * 4종을 하나의 핸들러(UpdateStateCache)로 처리할 때 이 타입으로 캐스팅.
 * 아래 4종 전체 구조체의 선두 7필드와 바이트 단위로 동일해야 함.
 */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} FC_STATE_PREFIX_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     RollRad;
    float                     PitchRad;
    float                     YawRad;
    float                     RollspeedRps;
    float                     PitchspeedRps;
    float                     YawspeedRps;
} FC_ATTITUDE_TLM_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     X_m;
    float                     Y_m;
    float                     Z_m;
    float                     Vx_mps;
    float                     Vy_mps;
    float                     Vz_mps;
} FC_EKF_LOCAL_TLM_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     FixType;
    uint8                     SatellitesVisible;
    uint8                     Reserved;
    int32                     LatE7;
    int32                     LonE7;
    int32                     AltMm;
} FC_GPS_RAW_TLM_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    uint16                    Flags;
} FC_EKF_STATUS_TLM_t;

#endif

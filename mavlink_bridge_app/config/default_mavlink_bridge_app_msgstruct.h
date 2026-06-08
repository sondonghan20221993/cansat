#ifndef DEFAULT_MAVLINK_BRIDGE_APP_MSGSTRUCT_H
#define DEFAULT_MAVLINK_BRIDGE_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "mavlink_bridge_app_msgdefs.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_MissionQueryCmd_t;

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
    uint32                    LastRxTimestampMs;
    uint32                    MissionUploadSuccessCount;
    uint32                    MissionUploadFailCount;
    uint32                    LastUploadTimestampMs;
    uint8                     LastUploadWaypointCount;
    uint8                     LastUploadResult;
    uint16                    HkSpare;
} MAVLINK_BRIDGE_APP_HkTlm_t;

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
} MAVLINK_BRIDGE_APP_EkfLocalTlm_t;

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
} MAVLINK_BRIDGE_APP_AttitudeTlm_t;

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
} MAVLINK_BRIDGE_APP_GpsRawTlm_t;

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
} MAVLINK_BRIDGE_APP_EkfStatusTlm_t;

#define MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS 16U

typedef struct
{
    float X;
    float Y;
    float Z;
} MAVLINK_BRIDGE_APP_WaypointMirror_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t           TelemetryHeader;
    uint32                              Seq;
    uint32                              TimestampMs;
    uint32                              SourceSequence;
    uint8                               RouteType;
    uint8                               RouteVersion;
    uint8                               WaypointCount;
    uint8                               Reserved;
    MAVLINK_BRIDGE_APP_WaypointMirror_t Waypoints[MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS];
} MAVLINK_BRIDGE_APP_RouteUpdateMirror_t;

/* CONFIG_CMD_MID 수신용 (uplink_app의 UPLINK_APP_ConfigCmdTlm_t와 동일 레이아웃) */
#define MAVLINK_BRIDGE_APP_CONFIG_MAX_PAYLOAD 196U

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     PayloadLength;
    uint8                     Reserved;
    uint8                     Payload[MAVLINK_BRIDGE_APP_CONFIG_MAX_PAYLOAD];
} MAVLINK_BRIDGE_APP_ConfigCmdTlm_t;

typedef struct
{
    uint8  ConfigScope;
    uint8  ConfigVersion;
    uint16 ParameterId;
    uint8  ValueType;
    uint8  ValueLength;
    uint16 Checksum;   /* uint16 additive sum: scope+version+param_id(2B)+value_type+value_length+value_bytes */
} MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t;

#endif

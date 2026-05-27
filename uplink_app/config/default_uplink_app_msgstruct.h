#ifndef DEFAULT_UPLINK_APP_MSGSTRUCT_H
#define DEFAULT_UPLINK_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "uplink_app_msgdefs.h"
#include "uplink_app_mission_cfg.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} UPLINK_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} UPLINK_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    uint8                   Version;
    uint8                   CommandClass;
    uint8                   PayloadLength;
    uint8                   Flags;
    uint16                  Sequence;
    uint16                  Reserved;
    uint8                   Payload[UPLINK_APP_MAX_PAYLOAD_LENGTH];
} UPLINK_APP_ProcessUplinkCmd_t;

typedef struct
{
    float X;
    float Y;
    float Z;
} UPLINK_APP_Waypoint_t;

typedef struct
{
    uint8  RouteType;
    uint8  RouteVersion;
    uint8  WaypointCount;
    uint8  Reserved;
    UPLINK_APP_Waypoint_t Waypoints[UPLINK_APP_ROUTE_MAX_WAYPOINTS];
} UPLINK_APP_RouteUpdatePayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    SourceSequence;
    uint8                     RouteType;
    uint8                     RouteVersion;
    uint8                     WaypointCount;
    uint8                     Reserved;
    UPLINK_APP_Waypoint_t     Waypoints[UPLINK_APP_ROUTE_MAX_WAYPOINTS];
} UPLINK_APP_RouteUpdateTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     Reserved[2];
} UPLINK_APP_RecoveryCmdTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     PayloadLength;
    uint8                     Reserved;
    uint8                     Payload[UPLINK_APP_MAX_PAYLOAD_LENGTH];
} UPLINK_APP_ViewpointCmdTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint8                     CommandCounter;
    uint8                     CommandErrorCounter;
    uint16                    Reserved;
    uint32                    PublishCount;
    uint32                    LastPublishTimestampMs;
} UPLINK_APP_HkTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastRxTimestampMs;
    uint32                    AcceptedCount;
    uint32                    RejectedCount;
    uint32                    RoutingFailureCount;
    uint16                    LastCommandCode;
    uint16                    LastCommandSequence;
    uint8                     LastCommandResult;
    uint8                     LinkState;
    uint8                     Valid;
    uint8                     ActiveTransportId;
    uint8                     LastRouteTarget;
    uint8                     ConfigPendingState;
    uint8                     LastConfigResult;
    uint8                     LastRollbackReason;
} UPLINK_APP_StatusTlm_t;

#endif


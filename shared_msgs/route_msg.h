#ifndef ROUTE_MSG_H
#define ROUTE_MSG_H

#include "cfe_msg_hdr.h"

#define ROUTE_MAX_WAYPOINTS 16

typedef struct {
    float X;
    float Y;
    float Z;
} ROUTE_WAYPOINT_t;

typedef struct {
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32 Seq;
    uint32 TimestampMs;
    uint32 SourceSequence;
    uint8 RouteType;
    uint8 RouteVersion;
    uint8 WaypointCount;
    uint8 Reserved;
    ROUTE_WAYPOINT_t Waypoints[ROUTE_MAX_WAYPOINTS];
} ROUTE_UPDATE_TLM_t;

#endif

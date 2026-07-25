#ifndef ROUTE_MSG_H
#define ROUTE_MSG_H

#include "cfe_msg_hdr.h"

#define ROUTE_MAX_WAYPOINTS 16

/* BL-56(2026-07-25): wire-level 직렬화 크기 — sizeof(ROUTE_WAYPOINT_t)는 정렬 패딩으로
 * 이 값보다 커질 수 있으므로 길이 검증에는 반드시 이 상수를 쓴다(구조체 캐스팅/sizeof 금지). */
#define ROUTE_WAYPOINT_WIRE_SIZE 29

typedef struct {
    uint8 CmdType;  /* MAVLink MAV_CMD: NAV_WAYPOINT=16(기본), NAV_LOITER_UNLIM=17, NAV_LOITER_TIME=19 등 */
    float Param1;
    float Param2;
    float Param3;
    float Param4;
    int32 LatE7;    /* 절대 위도 degE7 — 항상 유효 */
    int32 LonE7;    /* 절대 경도 degE7 — 항상 유효 */
    float Z;        /* 고도(m, relative alt) — 항상 유효 */
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

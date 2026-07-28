#ifndef DEFAULT_UPLINK_APP_MISSION_CFG_H
#define DEFAULT_UPLINK_APP_MISSION_CFG_H

#define UPLINK_APP_ROUTE_MAX_WAYPOINTS        37
#define UPLINK_APP_ROUTE_ALTITUDE_MIN_M       2.0f
#define UPLINK_APP_ROUTE_ALTITUDE_MAX_M       8.0f

/* BL-56(2026-07-25): 세그먼트 거리(2.0m) 강제 및 flyable area(±50m)/no-fly zone 기체측
 * 검증 전면 폐지 — waypoint가 항상 절대좌표(LatE7/LonE7)라 로컬 X/Y 기준점이 없어졌고,
 * flyable area 판단은 GUI 재확인 다이얼로그로 이관. 고도(2m~8m)와 finite 검증만 유지. */

#endif

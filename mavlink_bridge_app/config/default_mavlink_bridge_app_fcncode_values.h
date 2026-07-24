#ifndef DEFAULT_MAVLINK_BRIDGE_APP_FCNCODE_VALUES_H
#define DEFAULT_MAVLINK_BRIDGE_APP_FCNCODE_VALUES_H

#define MAVLINK_BRIDGE_APP_NOOP_CC             0
#define MAVLINK_BRIDGE_APP_RESET_COUNTERS_CC   1
#define MAVLINK_BRIDGE_APP_MISSION_QUERY_CC    2
/* ground_controllable_capability_plan(2026-07-21) P1-a: cfs_core_app의
 * RECOVERY(PARSER_RESET/SERIAL_RECONNECT)가 SB로 트리거하는 대상 */
#define MAVLINK_BRIDGE_APP_PARSER_RESET_CC     3
#define MAVLINK_BRIDGE_APP_SERIAL_RECONNECT_CC 4
/* BL-44(2026-07-24): uplink_app이 직접 트리거(cfs_core 미경유, counter mgmt와 동일 패턴) */
#define MAVLINK_BRIDGE_APP_SET_FLIGHT_MODE_CC  5

#endif

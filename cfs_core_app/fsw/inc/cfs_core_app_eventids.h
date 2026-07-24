#ifndef CFS_CORE_APP_EVENTIDS_H
#define CFS_CORE_APP_EVENTIDS_H

#define CFS_CORE_APP_STARTUP_EID     1
#define CFS_CORE_APP_COMMAND_ERR_EID 2
#define CFS_CORE_APP_NOOP_EID        3
#define CFS_CORE_APP_RESET_EID       4
#define CFS_CORE_APP_PUBLISH_EID     5
#define CFS_CORE_APP_HK_EID                6
#define CFS_CORE_APP_HEALTH_TRANSITION_EID 7
#define CFS_CORE_APP_SEQ_ERR_EID           8
#define CFS_CORE_APP_TIMESTAMP_ERR_EID     9
#define CFS_CORE_APP_BRIDGE_RESTART_EID   10
#define CFS_CORE_APP_VIEWPOINT_EID        11
#define CFS_CORE_APP_SEQ_GAP_EID          12
#define CFS_CORE_APP_RECOVERY_CMD_EID     13
#define CFS_CORE_APP_MODE_CMD_EID         14
#define CFS_CORE_APP_UPLINK_RESTART_EID   15
#define CFS_CORE_APP_LORA_RESTART_EID     16
#define CFS_CORE_APP_STATE_SAVE_FAIL_EID  17 /* BL-39(2026-07-23): SaveState 실패 */
#define CFS_CORE_APP_ROUTE_READBACK_EID   18 /* waypoint readback(2026-07-23) */
#define CFS_CORE_APP_STATE_CORRUPT_EID    19 /* BL-41(2026-07-23): 상태 파일 손상 → 기본값 폴백 */
#define CFS_CORE_APP_TIMEBASE_SHIFT_EID   20 /* BL-42(2026-07-24): FC 재부팅/time base 역행 감지 */

#endif


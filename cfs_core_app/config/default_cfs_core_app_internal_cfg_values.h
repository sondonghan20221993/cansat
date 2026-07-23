#ifndef DEFAULT_CFS_CORE_APP_INTERNAL_CFG_VALUES_H
#define DEFAULT_CFS_CORE_APP_INTERNAL_CFG_VALUES_H

#define CFS_CORE_APP_PLATFORM_PIPE_DEPTH  16
#define CFS_CORE_APP_PLATFORM_PIPE_NAME   "CFS_CORE_CMD"
#define CFS_CORE_APP_SB_POLL_TIMEOUT_MS   200
#define CFS_CORE_APP_PROTOTYPE_PERIOD_MS  1000
#define CFS_CORE_APP_ATTITUDE_TIMEOUT_MS  2000
#define CFS_CORE_APP_LOCAL_TIMEOUT_MS     2000
#define CFS_CORE_APP_GPS_TIMEOUT_MS       3000
#define CFS_CORE_APP_EKF_TIMEOUT_MS       2000
#define CFS_CORE_APP_BRIDGE_TIMEOUT_MS    3000
#define CFS_CORE_APP_NOMINAL_STABILITY_MS         10000
#define CFS_CORE_APP_FAILED_ESCALATION_MS         30000
#define CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS       5000
/* BL-38(2026-07-23): MAX_RESTARTS 제거 — 고정 쿨다운만으로 빈도 제한,
 * 무한 재시도(spec §11.1 참조). */
#define CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS    5000
#define CFS_CORE_APP_BRIDGE_APP_NAME   "MAVLINK_BRIDGE_APP" /* cFE 등록명(startup.scr 3번째 필드, 대소문자 구분) — BL-40 */
#define CFS_CORE_APP_UPLINK_TIMEOUT_MS             5000
#define CFS_CORE_APP_LORA_TIMEOUT_MS               5000
#define CFS_CORE_APP_UPLINK_RESTART_INTERVAL_MS    5000
#define CFS_CORE_APP_UPLINK_APP_NAME   "UPLINK_APP" /* BL-40 */
#define CFS_CORE_APP_LORA_RESTART_INTERVAL_MS      5000
#define CFS_CORE_APP_LORA_APP_NAME     "LORA_TDM_APP" /* BL-40 */
/* BL-39(2026-07-23): uplink_app과 동일 결함 — 절대경로 "/cf/..."가
 * Pi 실파일시스템에 없어 ENOENT(실측 확인). 상대경로로 WorkingDirectory
 * 기준 cf/(EEPROM.DAT 등 실사용 경로)와 일치시킨다. */
#define CFS_CORE_APP_STATE_FILE_PATH   "cf/cfs_core_app_state.bin"
#define CFS_CORE_APP_STATE_MAGIC       0xCF5C0A00U
#define CFS_CORE_APP_CONFIG_VERSION    1U
#define CFS_CORE_APP_CONFIG_SCOPE      1U   /* cfs_core_app 대상 범위 */
#define CFS_CORE_APP_PARAM_MIN_MS      100U /* 파라미터 최솟값 100ms */
#define CFS_CORE_APP_PARAM_MAX_MS      60000U /* 파라미터 최댓값 60s */

#endif


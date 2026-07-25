#ifndef DEFAULT_LORA_TDM_APP_MISSION_CFG_H
#define DEFAULT_LORA_TDM_APP_MISSION_CFG_H

#include "lora_tdm_app_interface_cfg.h"

/* TDM timing */
/* BL-15 Stage 4b 실측(2026-07-22): 100ms/50ms/50 — Stage 4a(150ms) PASS 후
 * 10Hz 시도. 5분 soak 결과로 최종 상한 확정. 실측 완료 전까지 임시값. */
#define LORA_TDM_APP_CYCLE_PERIOD_MS          100
#define LORA_TDM_APP_RX_WINDOW_MS             50
#define LORA_TDM_APP_LINK_LOSS_THRESHOLD      50
#define LORA_TDM_APP_LINK_TIMEOUT_MS          5000

/* Packet types */
#define LORA_TDM_APP_FC_STATE_PACKET_TYPE      1
#define LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE 2

/* UplinkFeedback codes */
#define LORA_TDM_APP_UPLINK_FB_OK              0
#define LORA_TDM_APP_UPLINK_FB_CRC_FAIL        1
#define LORA_TDM_APP_UPLINK_FB_SEQ_FAIL        2
#define LORA_TDM_APP_UPLINK_FB_STATE_BLOCKED   3
/* BL-11(2026-07-22): UFB 전용 독립 번호 체계로 순차 배정 (uplink_app
 * 내부 UPLINK_APP_RESULT_* 번호와는 별개 — lora_tdm_app_behavior_spec.md
 * §9.2 참조) */
#define LORA_TDM_APP_UPLINK_FB_FAILED            4
#define LORA_TDM_APP_UPLINK_FB_REJECT_VERSION    5
#define LORA_TDM_APP_UPLINK_FB_REJECT_CLASS      6
#define LORA_TDM_APP_UPLINK_FB_REJECT_LENGTH     7
#define LORA_TDM_APP_UPLINK_FB_ROUTE_MISS        8
#define LORA_TDM_APP_UPLINK_FB_REJECT_ROUTE      9
#define LORA_TDM_APP_UPLINK_FB_REJECT_CHECKSUM   10
#define LORA_TDM_APP_UPLINK_FB_REJECT_VIEWPOINT  11
#define LORA_TDM_APP_UPLINK_FB_REJECT_COUNTER    12 /* counter management scope/action 오류(§18.4.6.7, BL-CTR) */
#define LORA_TDM_APP_UPLINK_FB_REJECT_FLIGHT_MODE 13 /* flight mode payload 오류(BL-44, §18.4.6.8) */
#define LORA_TDM_APP_UPLINK_FB_APPLIED             0x0E /* ROUTED(성공) 명시 세팅 — UFB_OK와의 모호성 해소(BL-55).
                                                          * spec §9.2: 최초 0x0C로 배정했으나 REJECT_COUNTER(12=0x0C)와
                                                          * 충돌 발견돼 0x0E로 정정(behavior_spec.md 2026-07-25). */

/* Link state values */
#define LORA_TDM_APP_LINK_DISCONNECTED         0
#define LORA_TDM_APP_LINK_CONNECTED            1
#define LORA_TDM_APP_LINK_DEGRADED             2

/* Serial */
#define LORA_TDM_APP_LORA_SERIAL_PATH \
    "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0"
#define LORA_TDM_APP_LORA_BAUDRATE 57600

/* uplink_app function code for forwarded UP frames (mirrors UPLINK_APP_PROCESS_UPLINK_CC) */
#define LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC  2

/* CONFIG_CMD_MID scope routing (cfs_core_app=1, mavlink_bridge_app=2와 공존) —
 * openMCT fc_serial_ws_server.py의 UPLINK_CLASS_CONFIG 경로로 도달 가능한 유일한
 * 실제 지상->기체 커맨드 채널 (uplink_app의 6개 CommandClass 중 CONFIG만 여기로 옴). */
#define LORA_TDM_APP_CONFIG_SCOPE               3U
#define LORA_TDM_APP_CONFIG_VERSION             1U
#define LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL    0U /* value: 0=v1, 1=v2(DL2) */

/* BL-41(2026-07-23): CONFIG 영속화 — BL-39 관례에 따라 상대경로 cf/ 사용 */
#define LORA_TDM_APP_STATE_FILE_PATH "cf/lora_tdm_app_state.bin"
#define LORA_TDM_APP_STATE_MAGIC     0x10A7D3B0U

#endif

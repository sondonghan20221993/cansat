#ifndef DEFAULT_LORA_TDM_APP_MISSION_CFG_H
#define DEFAULT_LORA_TDM_APP_MISSION_CFG_H

#include "lora_tdm_app_interface_cfg.h"

/* TDM timing */
#define LORA_TDM_APP_CYCLE_PERIOD_MS          500
#define LORA_TDM_APP_RX_WINDOW_MS             150
#define LORA_TDM_APP_LINK_LOSS_THRESHOLD      6
#define LORA_TDM_APP_LINK_TIMEOUT_MS          5000
#define LORA_TDM_APP_SERIAL_REOPEN_DELAY_MS   1000

/* Packet types */
#define LORA_TDM_APP_FC_STATE_PACKET_TYPE      1
#define LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE 2

/* UplinkFeedback codes */
#define LORA_TDM_APP_UPLINK_FB_OK              0
#define LORA_TDM_APP_UPLINK_FB_CRC_FAIL        1
#define LORA_TDM_APP_UPLINK_FB_SEQ_FAIL        2

/* Link state values */
#define LORA_TDM_APP_LINK_DISCONNECTED         0
#define LORA_TDM_APP_LINK_CONNECTED            1
#define LORA_TDM_APP_LINK_DEGRADED             2

/* Serial */
#define LORA_TDM_APP_LORA_SERIAL_PATH \
    "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0"
#define LORA_TDM_APP_LORA_BAUDRATE 57600

/* Protocol */
#define LORA_TDM_APP_PROTOCOL_VERSION          1
#define LORA_TDM_APP_MAX_PAYLOAD_LENGTH        196

/* uplink_app function code for forwarded UP frames (mirrors UPLINK_APP_PROCESS_UPLINK_CC) */
#define LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC  2

/* CONFIG_CMD_MID scope routing (cfs_core_app=1, mavlink_bridge_app=2와 공존) —
 * openMCT fc_serial_ws_server.py의 UPLINK_CLASS_CONFIG 경로로 도달 가능한 유일한
 * 실제 지상->기체 커맨드 채널 (uplink_app의 6개 CommandClass 중 CONFIG만 여기로 옴). */
#define LORA_TDM_APP_CONFIG_SCOPE               3U
#define LORA_TDM_APP_CONFIG_VERSION             1U
#define LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL    0U /* value: 0=v1, 1=v2(DL2) */

#endif

#ifndef DEFAULT_LORA_TDM_APP_MISSION_CFG_H
#define DEFAULT_LORA_TDM_APP_MISSION_CFG_H

#include "lora_tdm_app_interface_cfg.h"

/* TDM timing */
#define LORA_TDM_APP_CYCLE_PERIOD_MS          1000
#define LORA_TDM_APP_RX_WINDOW_MS             300
#define LORA_TDM_APP_LINK_LOSS_THRESHOLD      3
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

#endif

#ifndef LORA_FC_DOWNLINK_APP_INTERNAL_CFG_H
#define LORA_FC_DOWNLINK_APP_INTERNAL_CFG_H

#include "lora_fc_downlink_app_internal_cfg_values.h"

#define LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_DEPTH         LORA_FC_DOWNLINK_APP_PLATFORM_CFGVAL(PIPE_DEPTH)
/* depth 32: ServiceLoRa의 300ms 블로킹 RX 윈도우 동안 SB 수신이 멈추는데,
 * FC 스트림(~45 msg/s) 기준 300ms에 ~14개 누적됨. depth 10이면 오버플로.
 * cFS 최대(50) 미만에서 여유 마진 확보. 근본 해결(블로킹 I/O 분리)은 별도 과제. */
#define DEFAULT_LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_DEPTH 32

#define LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_NAME         LORA_FC_DOWNLINK_APP_PLATFORM_CFGVAL(PIPE_NAME)
#define DEFAULT_LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_NAME "LORA_FC_DN_CMD_PIPE"

#endif

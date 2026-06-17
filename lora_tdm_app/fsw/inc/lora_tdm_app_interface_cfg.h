#ifndef LORA_TDM_APP_INTERFACE_CFG_H
#define LORA_TDM_APP_INTERFACE_CFG_H

/* Max line length for serial framing */
#define LORA_TDM_APP_LINE_BUF_LEN       256

/* ACK frame prefix */
#define LORA_TDM_APP_ACK_PREFIX         "ACK,"

/* Uplink frame prefix */
#define LORA_TDM_APP_UP_PREFIX          "UP,"

/* Downlink frame prefixes */
#define LORA_TDM_APP_FC_PREFIX          "FC,"
#define LORA_TDM_APP_SH_PREFIX          "SH,"

#endif

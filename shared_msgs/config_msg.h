#ifndef CONFIG_MSG_H
#define CONFIG_MSG_H

#include "cfe_msg_hdr.h"

#define CONFIG_MAX_PAYLOAD 196

typedef struct {
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32 Seq;
    uint32 TimestampMs;
    uint16 SourceSequence;
    uint8 PayloadLength;
    uint8 Reserved;
    uint8 Payload[CONFIG_MAX_PAYLOAD];
} CONFIG_CMD_TLM_t;

#endif

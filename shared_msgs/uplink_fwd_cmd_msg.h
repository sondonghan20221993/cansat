#ifndef UPLINK_FWD_CMD_MSG_H
#define UPLINK_FWD_CMD_MSG_H

#include "cfe.h"

/*
 * lora_tdm_app 발행 (UPLINK_APP_CMD_MID 0x18D0, UP 프레임 forward), uplink_app 구독 — 단일 진실.
 * 필드 추가는 항상 끝에 append (notes/temp/mirror_struct_layout_audit.md 참조).
 */
#define UPLINK_FWD_CMD_MAX_PAYLOAD_LENGTH 196

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    uint8                    Version;
    uint8                    CommandClass;
    uint8                    PayloadLength;
    uint8                    Flags;
    uint16                   Sequence;
    uint16                   Checksum; /* CRC-16/CCITT-FALSE over Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload */
    uint8                    Payload[UPLINK_FWD_CMD_MAX_PAYLOAD_LENGTH];
} UPLINK_FWD_CMD_TLM_t;

#endif

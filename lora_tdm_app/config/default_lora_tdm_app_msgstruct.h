#ifndef DEFAULT_LORA_TDM_APP_MSGSTRUCT_H
#define DEFAULT_LORA_TDM_APP_MSGSTRUCT_H

#include "common_types.h"
#include "cfe_msg_hdr.h"
#include "lora_tdm_app_msgdefs.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} LORA_TDM_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} LORA_TDM_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} LORA_TDM_APP_SendHkCmd_t;

typedef struct
{
    uint8  CommandCounter;
    uint8  CommandErrorCounter;
    uint8  LinkState;
    uint8  PacketType;
    uint8  AttitudeValid;
    uint8  LocalValid;
    uint8  GpsValid;
    uint8  EkfValid;
    uint8  SystemHealthState;
    uint8  PendingUplinkFeedback;
    uint8  Reserved[2];
    uint32 TxCount;
    uint32 RxAckCount;
    uint32 RxCmdCount;
    uint16 RxErrorCount;
    uint16 NoAckCount;
    uint32 LastAckTimestampMs;
} LORA_TDM_APP_HkPayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    LORA_TDM_APP_HkPayload_t  Payload;
} LORA_TDM_APP_HkTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint8                     LinkState;
    uint8                     Reserved[3];
    uint32                    LastAckTimestampMs;
    uint16                    NoAckCount;
    uint16                    RxErrorCount;
    uint32                    TxCount;
    uint32                    RxAckCount;
    uint32                    RxCmdCount;
} LORA_TDM_APP_LinkStatusTlm_t;

/* Forwarded to UPLINK_APP_CMD_MID (0x18D0); layout matches UPLINK_APP_ProcessUplinkCmd_t */
typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    uint8                   Version;
    uint8                   CommandClass;
    uint8                   PayloadLength;
    uint8                   Flags;
    uint16                  Sequence;
    uint16                  Reserved;
    uint8                   Payload[196]; /* LORA_TDM_APP_MAX_PAYLOAD_LENGTH */
} LORA_TDM_APP_UplinkFwdCmd_t;

#endif

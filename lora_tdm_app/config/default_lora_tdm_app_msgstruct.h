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

/* §8 v1/v2 다운링크 프로토콜 런타임 전환 — UseV2=0(v1)/1(v2) */
typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    uint8                   UseV2;
    uint8                   Reserved[3];
} LORA_TDM_APP_SetDownlinkProtocolCmd_t;

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
    uint16                  Checksum; /* CRC-16/CCITT-FALSE over Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload */
    uint8                   Payload[196]; /* LORA_TDM_APP_MAX_PAYLOAD_LENGTH */
} LORA_TDM_APP_UplinkFwdCmd_t;

/* DIAGNOSTIC_CMD_MID (0x1910) received from uplink_app */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     DiagAction;
    uint8                     DiagTarget;
    uint32                    RequestToken;
} LORA_TDM_APP_DiagnosticCmdTlm_t;

/* CONFIG_CMD_MID(0x190E) 수신용 — uplink_app UPLINK_APP_ConfigCmdTlm_t와 동일 레이아웃
 * (cfs_core_app_msgstruct.h CFS_CORE_APP_ConfigCmdTlm_t와도 동일 — 여러 앱이 같은
 * MID를 구독하고 Payload 내부 scope로 자기 것만 골라 처리). */
#define LORA_TDM_APP_CONFIG_MAX_PAYLOAD 196
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint16                    SourceSequence;
    uint8                     PayloadLength;
    uint8                     Reserved;
    uint8                     Payload[LORA_TDM_APP_CONFIG_MAX_PAYLOAD];
} LORA_TDM_APP_ConfigCmdTlm_t;

/* config payload 내부 헤더 (openMCT fc_serial_ws_server.py _build_config_payload와 동일 레이아웃) */
typedef struct
{
    uint8  ConfigScope;
    uint8  ConfigVersion;
    uint16 ParameterId;
    uint8  ValueType;
    uint8  ValueLength;
    uint16 Checksum; /* additive sum: scope+version+param_id(2B)+value_type+value_length+value_bytes */
} LORA_TDM_APP_ConfigPayloadHdr_t;

#endif

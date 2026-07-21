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

/* v2 바이너리 프레임 magic — lora_protocol_v2_spec.md §3.
 * v1 텍스트 첫 바이트('A','U','F','S')와 겹치지 않음 → 수신기가 첫 바이트로 즉시 분기. */
#define LORA_TDM_APP_DL2_MAGIC          0xD2u
#define LORA_TDM_APP_UP2_MAGIC          0xB2u
#define LORA_TDM_APP_ACK2_MAGIC         0xA2u

/* DL2 고정 필드(FC/헬스) 길이 — magic부터 linkstate까지, SysTime 삽입 지점(offset) */
#define LORA_TDM_APP_DL2_BASE_LEN       45u
/* BL-03(2026-07-22): uplink_last_seq(u16)+uplink_boot_count(u8) — 지상 자가복구/
 * 재부팅감지용, SysTime 블록(있으면) 뒤·CRC 앞에 항상 붙는 꼬리 필드 */
#define LORA_TDM_APP_DL2_TAIL_LEN       3u
/* DL2 기본(SysTime 확장 없음) 길이 — magic부터 CRC 직전까지(len 필드 값) = BASE+TAIL */
#define LORA_TDM_APP_DL2_LEN_FIELD      (LORA_TDM_APP_DL2_BASE_LEN + LORA_TDM_APP_DL2_TAIL_LEN)
/* DL2 기본 프레임 총 길이(CRC 포함) */
#define LORA_TDM_APP_DL2_FRAME_LEN      (LORA_TDM_APP_DL2_LEN_FIELD + 2u)
/* SysTime 확장 블록 길이(uint64 TimeUnixUsec) — spec §4.2, DL2_FLAG_SYSTIME(bit0) */
#define LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN  8u
#define LORA_TDM_APP_DL2_FLAG_SYSTIME       0x01u
/* SysTime 확장 포함 시 최대 프레임 총 길이(CRC 포함) */
#define LORA_TDM_APP_DL2_MAX_FRAME_LEN  (LORA_TDM_APP_DL2_FRAME_LEN + LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN)

#endif

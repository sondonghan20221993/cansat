#ifndef LORA_TDM_APP_UTILS_H
#define LORA_TDM_APP_UTILS_H

#include "cfe.h"
#include "lora_tdm_app.h"

/* ACK parse result */
typedef enum
{
    LORA_TDM_ACK_OK      = 0,
    LORA_TDM_ACK_INVALID = 1
} LORA_TDM_AckResult_t;

/* RX line type */
typedef enum
{
    LORA_TDM_RX_ACK     = 0,
    LORA_TDM_RX_UPLINK  = 1,
    LORA_TDM_RX_UNKNOWN = 2
} LORA_TDM_RxLineType_t;

/* Parse "ACK,<seq>\n" → fills *SeqEcho; returns LORA_TDM_ACK_OK on success */
LORA_TDM_AckResult_t LORA_TDM_APP_ParseAckFrame(const char *Line, uint32 *SeqEcho);

/* Parse ACK2(v2 바이너리, 5B) — lora_protocol_v2_spec.md §6. CRC 검증 포함. */
LORA_TDM_AckResult_t LORA_TDM_APP_ParseAck2Frame(const uint8 *Buf, size_t Len, uint32 *SeqEcho);

/* UP2(v2 바이너리 업링크) 디코드 결과 — lora_protocol_v2_spec.md §5 */
typedef struct
{
    uint8  Version;
    uint8  CommandClass;
    uint16 Seq;
    uint8  Flags;
    uint8  PayloadLen;
    uint8  Payload[196]; /* ProcessUpFrame 최대 페이로드와 동일 크기 */
} LORA_TDM_APP_Up2Decoded_t;

/* Parse UP2 프레임 — CRC 검증 포함, 성공 시 *Out 채움 */
LORA_TDM_AckResult_t LORA_TDM_APP_ParseUp2Frame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Up2Decoded_t *Out);

/* 완성된 v2 바이너리 프레임 1개 처리 — Buf[0](magic)로 ACK2/UP2 분기, AppData 상태 갱신.
 * ProcessRxLine(v1)과 대응되는 v2 진입점. RunRxWindow의 길이기반 상태머신이 프레임을
 * 다 모으면 호출한다. */
void LORA_TDM_APP_ProcessRxBinaryFrame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Data_t *AppData);

/* CONFIG_CMD_MID 수신 처리 — scope(LORA_TDM_APP_CONFIG_SCOPE) 아니면 조용히 무시,
 * 맞으면 checksum 검증 후 파라미터 적용(§8, openMCT UPLINK_CLASS_CONFIG 경로). */
void LORA_TDM_APP_ProcessConfigCommand(const LORA_TDM_APP_ConfigCmdTlm_t *Msg);
void LORA_TDM_APP_LoadState(void);
void LORA_TDM_APP_SaveState(void);

/* Build FC downlink line into Buf (size BufLen); returns bytes written or <0 on error */
int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen,
                                      const LORA_TDM_APP_Data_t *AppData);

/* Build SH downlink line into Buf (size BufLen); returns bytes written or <0 on error */
int LORA_TDM_APP_BuildShDownlinkLine(char *Buf, size_t BufLen,
                                      const LORA_TDM_APP_Data_t *AppData);

/* Process a received line (ACK or UP); updates AppData state */
void LORA_TDM_APP_ProcessRxLine(const char *Line, LORA_TDM_APP_Data_t *AppData);

/* Update LinkState based on NoAckCount and LastAckTimestampMs */
void LORA_TDM_APP_UpdateLinkState(LORA_TDM_APP_Data_t *AppData, uint32 NowMs);

/* Update FcState cache from incoming SB message */
void LORA_TDM_APP_UpdateCacheFromMsg(CFE_SB_Buffer_t *SBBufPtr, LORA_TDM_APP_Data_t *AppData);

/* CRC-16/CCITT-FALSE */
uint16 LORA_TDM_APP_Crc16(const uint8 *Data, size_t Len);

/* Build DL2(v2 바이너리 다운링크 통합) 프레임 — lora_protocol_v2_spec.md §4.
 * SysTime 확장 블록(§4.2) 포함 — FC_SYS_TIME_MID 구독(lora_tdm_app.c) +
 * 캐시(lora_tdm_app_utils.c) + 인코딩(버퍼 부족 시 47B 폴백) 모두 구현됨.
 * Buf는 최소 LORA_TDM_APP_DL2_FRAME_LEN 바이트, 성공 시 바이트 수 또는 <0 on error. */
int LORA_TDM_APP_BuildDl2Frame(uint8 *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData);

/* Process DIAGNOSTIC_CMD_MID from uplink_app */
void LORA_TDM_APP_ProcessDiagnosticCommand(CFE_SB_Buffer_t *SBBufPtr);
void LORA_TDM_APP_ProcessRouteSnapshot(const LORA_TDM_APP_RouteSnapshotTlm_t *Msg);

#endif

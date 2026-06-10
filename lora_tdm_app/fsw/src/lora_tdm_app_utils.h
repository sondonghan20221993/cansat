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

/* Build FC downlink line into Buf (size BufLen); returns bytes written or <0 on error */
int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen,
                                      const LORA_TDM_APP_Data_t *AppData);

/* Build SH downlink line into Buf (size BufLen); returns bytes written or <0 on error */
int LORA_TDM_APP_BuildShDownlinkLine(char *Buf, size_t BufLen,
                                      const LORA_TDM_APP_Data_t *AppData);

/* Process a received line (ACK or UP); updates AppData state */
void LORA_TDM_APP_ProcessRxLine(char *Line, LORA_TDM_APP_Data_t *AppData);

/* Update LinkState based on NoAckCount and LastAckTimestampMs */
void LORA_TDM_APP_UpdateLinkState(LORA_TDM_APP_Data_t *AppData, uint32 NowMs);

/* Update FcState cache from incoming SB message */
void LORA_TDM_APP_UpdateCacheFromMsg(CFE_SB_Buffer_t *SBBufPtr, LORA_TDM_APP_Data_t *AppData);

/* CRC-16/CCITT-FALSE */
uint16 LORA_TDM_APP_Crc16(const uint8 *Data, size_t Len);

#endif

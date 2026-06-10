#include "lora_tdm_app_utils.h"
#include "utgenstub.h"

LORA_TDM_AckResult_t LORA_TDM_APP_ParseAckFrame(const char *Line, uint32 *SeqEcho)
{
    UT_GenStub_AddParam(LORA_TDM_APP_ParseAckFrame, const char *, Line);
    UT_GenStub_AddParam(LORA_TDM_APP_ParseAckFrame, uint32 *, SeqEcho);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_ParseAckFrame, LORA_TDM_AckResult_t);
    UT_GenStub_Execute(LORA_TDM_APP_ParseAckFrame, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_ParseAckFrame, LORA_TDM_AckResult_t);
}

int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    UT_GenStub_AddParam(LORA_TDM_APP_BuildFcDownlinkLine, char *, Buf);
    UT_GenStub_AddParam(LORA_TDM_APP_BuildFcDownlinkLine, size_t, BufLen);
    UT_GenStub_AddParam(LORA_TDM_APP_BuildFcDownlinkLine, const LORA_TDM_APP_Data_t *, AppData);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_BuildFcDownlinkLine, int);
    UT_GenStub_Execute(LORA_TDM_APP_BuildFcDownlinkLine, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_BuildFcDownlinkLine, int);
}

int LORA_TDM_APP_BuildShDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    UT_GenStub_AddParam(LORA_TDM_APP_BuildShDownlinkLine, char *, Buf);
    UT_GenStub_AddParam(LORA_TDM_APP_BuildShDownlinkLine, size_t, BufLen);
    UT_GenStub_AddParam(LORA_TDM_APP_BuildShDownlinkLine, const LORA_TDM_APP_Data_t *, AppData);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_BuildShDownlinkLine, int);
    UT_GenStub_Execute(LORA_TDM_APP_BuildShDownlinkLine, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_BuildShDownlinkLine, int);
}

void LORA_TDM_APP_ProcessRxLine(char *Line, LORA_TDM_APP_Data_t *AppData)
{
    UT_GenStub_AddParam(LORA_TDM_APP_ProcessRxLine, char *, Line);
    UT_GenStub_AddParam(LORA_TDM_APP_ProcessRxLine, LORA_TDM_APP_Data_t *, AppData);
    UT_GenStub_Execute(LORA_TDM_APP_ProcessRxLine, Basic, NULL);
}

void LORA_TDM_APP_UpdateLinkState(LORA_TDM_APP_Data_t *AppData, uint32 NowMs)
{
    UT_GenStub_AddParam(LORA_TDM_APP_UpdateLinkState, LORA_TDM_APP_Data_t *, AppData);
    UT_GenStub_AddParam(LORA_TDM_APP_UpdateLinkState, uint32, NowMs);
    UT_GenStub_Execute(LORA_TDM_APP_UpdateLinkState, Basic, NULL);
}

void LORA_TDM_APP_UpdateCacheFromMsg(CFE_SB_Buffer_t *SBBufPtr, LORA_TDM_APP_Data_t *AppData)
{
    UT_GenStub_AddParam(LORA_TDM_APP_UpdateCacheFromMsg, CFE_SB_Buffer_t *, SBBufPtr);
    UT_GenStub_AddParam(LORA_TDM_APP_UpdateCacheFromMsg, LORA_TDM_APP_Data_t *, AppData);
    UT_GenStub_Execute(LORA_TDM_APP_UpdateCacheFromMsg, Basic, NULL);
}

uint16 LORA_TDM_APP_Crc16(const uint8 *Data, size_t Len)
{
    UT_GenStub_AddParam(LORA_TDM_APP_Crc16, const uint8 *, Data);
    UT_GenStub_AddParam(LORA_TDM_APP_Crc16, size_t, Len);
    UT_GenStub_SetupReturnBuffer(LORA_TDM_APP_Crc16, uint16);
    UT_GenStub_Execute(LORA_TDM_APP_Crc16, Basic, NULL);
    return UT_GenStub_GetReturnValue(LORA_TDM_APP_Crc16, uint16);
}

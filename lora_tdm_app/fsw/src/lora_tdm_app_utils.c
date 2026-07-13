#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"

#include "cfe_time.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32 UtilsGetTimeMs(void)
{
    CFE_TIME_SysTime_t T = CFE_TIME_GetTime();
    uint64             Ms;

    Ms = ((uint64)T.Seconds * 1000ULL) + ((uint64)T.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)Ms;
}

/* ---- CRC-16/CCITT-FALSE ---- */

uint16 LORA_TDM_APP_Crc16(const uint8 *Data, size_t Len)
{
    uint16 Crc = 0xFFFF;
    size_t i;
    uint8  j;

    for (i = 0; i < Len; i++)
    {
        Crc ^= (uint16)Data[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (Crc & 0x8000)
                Crc = (Crc << 1) ^ 0x1021;
            else
                Crc <<= 1;
        }
    }
    return Crc;
}

/* ---- Parse ACK frame: "ACK,<seq>\n" ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseAckFrame(const char *Line, uint32 *SeqEcho)
{
    unsigned long Val;
    char         *End;

    if (Line == NULL || SeqEcho == NULL)
    {
        return LORA_TDM_ACK_INVALID;
    }

    if (strncmp(Line, "ACK,", 4) != 0)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Val = strtoul(Line + 4, &End, 10);
    if (End == Line + 4 || (*End != '\n' && *End != '\r' && *End != '\0'))
    {
        return LORA_TDM_ACK_INVALID;
    }

    *SeqEcho = (uint32)Val;
    return LORA_TDM_ACK_OK;
}

/* ---- Build FC downlink line ---- */
/*
 * Format: FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat>,<lon>,<alt>,<fix>,<ufb>,<sats>\n
 *
 * <sats> appended as field 18 (2026-07-13) — 기존 필드 순서/개수를 바꾸지 않고 끝에 추가해
 * 구 파서(17필드 고정 파싱)와의 호환을 유지한다. 지상 파서는 len(parts)>=18일 때만 읽는다.
 */
int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    int Len;

    Len = snprintf(Buf, BufLen,
                   "FC,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%ld,%ld,%ld,%u,%u,%u\n",
                   (unsigned long)AppData->DownlinkSeq,
                   (unsigned long)AppData->FcState.TimestampMs,
                   (double)AppData->FcState.RollRad,
                   (double)AppData->FcState.PitchRad,
                   (double)AppData->FcState.YawRad,
                   (double)AppData->FcState.PosX,
                   (double)AppData->FcState.PosY,
                   (double)AppData->FcState.PosZ,
                   (double)AppData->FcState.VelX,
                   (double)AppData->FcState.VelY,
                   (double)AppData->FcState.VelZ,
                   (long)AppData->FcState.LatE7,
                   (long)AppData->FcState.LonE7,
                   (long)AppData->FcState.AltMm,
                   (unsigned)AppData->FcState.GpsFix,
                   (unsigned)AppData->PendingUplinkFeedback,
                   (unsigned)AppData->FcState.SatellitesVisible);

    if (Len <= 0 || (size_t)Len >= BufLen)
    {
        return -1;
    }
    return Len;
}

/* ---- DL2 binary frame helpers (little-endian packing, portable) ---- */

static uint16 GetU16LE(const uint8 *P)
{
    return (uint16)(P[0] | ((uint16)P[1] << 8));
}

static void PutU16LE(uint8 *P, uint16 V)
{
    P[0] = (uint8)(V & 0xFFu);
    P[1] = (uint8)((V >> 8) & 0xFFu);
}

static void PutI16LE(uint8 *P, int16 V)
{
    PutU16LE(P, (uint16)V);
}

static void PutU32LE(uint8 *P, uint32 V)
{
    P[0] = (uint8)(V & 0xFFu);
    P[1] = (uint8)((V >> 8) & 0xFFu);
    P[2] = (uint8)((V >> 16) & 0xFFu);
    P[3] = (uint8)((V >> 24) & 0xFFu);
}

static void PutI32LE(uint8 *P, int32 V)
{
    PutU32LE(P, (uint32)V);
}

/* rad -> i16(rad*1e4), 범위 방어적 clamp (스펙상 각도 saturation 플래그는 정의 안 함) */
static int16 ScaleRadToI16(float Rad)
{
    double V = (double)Rad * 10000.0;
    if (V > 32767.0)
    {
        V = 32767.0;
    }
    else if (V < -32768.0)
    {
        V = -32768.0;
    }
    return (int16)V;
}

/* m 또는 m/s -> i16(값*100 = cm 또는 cm/s), §4.1 saturation: 초과 시 clamp+플래그 요청 */
static int16 ScaleMeterToI16Cm(float MetersVal, bool *Saturated)
{
    double Cm = (double)MetersVal * 100.0;

    if (Cm > 32767.0)
    {
        Cm = 32767.0;
        *Saturated = true;
    }
    else if (Cm < -32768.0)
    {
        Cm = -32768.0;
        *Saturated = true;
    }
    return (int16)Cm;
}

/* ---- Build DL2 frame (v2 바이너리 다운링크) — lora_protocol_v2_spec.md §4 ----
 * SysTime 확장 블록(§4.2) 미포함 — flags bit0 항상 0, 기본 47B 고정 길이. */
int LORA_TDM_APP_BuildDl2Frame(uint8 *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    uint8  Flags = 0;
    bool   Saturated = false;
    uint16 Crc;

    if (Buf == NULL || AppData == NULL || BufLen < LORA_TDM_APP_DL2_FRAME_LEN)
    {
        return -1;
    }

    Buf[0] = (uint8)LORA_TDM_APP_DL2_MAGIC;
    Buf[1] = (uint8)LORA_TDM_APP_DL2_LEN_FIELD;
    PutU16LE(&Buf[2], (uint16)AppData->DownlinkSeq);
    /* Buf[4] flags — 아래서 saturation 확인 후 채움 */
    Buf[5] = (uint8)AppData->PendingUplinkFeedback;
    PutU32LE(&Buf[6], AppData->FcState.TimestampMs);

    PutI16LE(&Buf[10], ScaleRadToI16(AppData->FcState.RollRad));
    PutI16LE(&Buf[12], ScaleRadToI16(AppData->FcState.PitchRad));
    PutI16LE(&Buf[14], ScaleRadToI16(AppData->FcState.YawRad));

    PutI16LE(&Buf[16], ScaleMeterToI16Cm(AppData->FcState.PosX, &Saturated));
    PutI16LE(&Buf[18], ScaleMeterToI16Cm(AppData->FcState.PosY, &Saturated));
    PutI16LE(&Buf[20], ScaleMeterToI16Cm(AppData->FcState.PosZ, &Saturated));

    PutI16LE(&Buf[22], ScaleMeterToI16Cm(AppData->FcState.VelX, &Saturated));
    PutI16LE(&Buf[24], ScaleMeterToI16Cm(AppData->FcState.VelY, &Saturated));
    PutI16LE(&Buf[26], ScaleMeterToI16Cm(AppData->FcState.VelZ, &Saturated));

    PutI32LE(&Buf[28], AppData->FcState.LatE7);
    PutI32LE(&Buf[32], AppData->FcState.LonE7);
    PutI32LE(&Buf[36], AppData->FcState.AltMm);

    Buf[40] = AppData->FcState.GpsFix;
    Buf[41] = AppData->FcState.SatellitesVisible;
    Buf[42] = AppData->SystemHealth.SystemHealthState;
    Buf[43] = AppData->SystemHealth.FaultCode;
    Buf[44] = AppData->LinkState;

    Flags |= Saturated ? 0x02u : 0x00u;
    Buf[4] = Flags;

    Crc = LORA_TDM_APP_Crc16(Buf, LORA_TDM_APP_DL2_LEN_FIELD);
    PutU16LE(&Buf[LORA_TDM_APP_DL2_LEN_FIELD], Crc);

    return (int)LORA_TDM_APP_DL2_FRAME_LEN;
}

/* ---- Parse ACK2 frame (v2 바이너리, 5B) — lora_protocol_v2_spec.md §6 ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseAck2Frame(const uint8 *Buf, size_t Len, uint32 *SeqEcho)
{
    uint16 ExpectedCrc;
    uint16 ActualCrc;

    if (Buf == NULL || SeqEcho == NULL || Len < 5)
    {
        return LORA_TDM_ACK_INVALID;
    }
    if (Buf[0] != (uint8)LORA_TDM_APP_ACK2_MAGIC)
    {
        return LORA_TDM_ACK_INVALID;
    }

    ActualCrc   = LORA_TDM_APP_Crc16(Buf, 3);
    ExpectedCrc = GetU16LE(&Buf[3]);
    if (ActualCrc != ExpectedCrc)
    {
        return LORA_TDM_ACK_INVALID;
    }

    *SeqEcho = GetU16LE(&Buf[1]);
    return LORA_TDM_ACK_OK;
}

/* ---- Parse UP2 frame (v2 바이너리 업링크) — lora_protocol_v2_spec.md §5 ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseUp2Frame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Up2Decoded_t *Out)
{
    uint8  Plen;
    size_t FrameLen;
    uint16 ExpectedCrc;
    uint16 ActualCrc;

    /* 최소 프레임(plen=0): magic+plen+version+class+seq(2)+flags+crc(2) = 8B */
    if (Buf == NULL || Out == NULL || Len < 8)
    {
        return LORA_TDM_ACK_INVALID;
    }
    if (Buf[0] != (uint8)LORA_TDM_APP_UP2_MAGIC)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Plen     = Buf[1];
    FrameLen = 7u + (size_t)Plen + 2u;
    if (Len < FrameLen || Plen > sizeof(Out->Payload))
    {
        return LORA_TDM_ACK_INVALID;
    }

    ActualCrc   = LORA_TDM_APP_Crc16(Buf, 7u + Plen);
    ExpectedCrc = GetU16LE(&Buf[7u + Plen]);
    if (ActualCrc != ExpectedCrc)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Out->Version      = Buf[2];
    Out->CommandClass = Buf[3];
    Out->Seq          = GetU16LE(&Buf[4]);
    Out->Flags        = Buf[6];
    Out->PayloadLen   = Plen;
    if (Plen > 0)
    {
        memcpy(Out->Payload, &Buf[7], Plen);
    }

    return LORA_TDM_ACK_OK;
}

/* ---- Build SH downlink line ---- */
/*
 * Format: SH,<seq>,<ts>,<state>,<fault>,<linkstate>,<ufb>\n
 */
int LORA_TDM_APP_BuildShDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    int Len;

    Len = snprintf(Buf, BufLen,
                   "SH,%lu,%lu,%u,%u,%u,%u\n",
                   (unsigned long)AppData->DownlinkSeq,
                   (unsigned long)AppData->SystemHealth.TimestampMs,
                   (unsigned)AppData->SystemHealth.SystemHealthState,
                   (unsigned)AppData->SystemHealth.FaultCode,
                   (unsigned)AppData->LinkState,
                   (unsigned)AppData->PendingUplinkFeedback);

    if (Len <= 0 || (size_t)Len >= BufLen)
    {
        return -1;
    }
    return Len;
}

/* ---- Update link state ---- */

void LORA_TDM_APP_UpdateLinkState(LORA_TDM_APP_Data_t *AppData, uint32 NowMs)
{
    uint32 Elapsed;

    if (AppData->LastAckTimestampMs == 0)
    {
        Elapsed = 0;
    }
    else if (NowMs >= AppData->LastAckTimestampMs)
    {
        Elapsed = NowMs - AppData->LastAckTimestampMs;
    }
    else
    {
        Elapsed = 0;
    }

    if (Elapsed > LORA_TDM_APP_LINK_TIMEOUT_MS)
    {
        AppData->LinkState = LORA_TDM_APP_LINK_DISCONNECTED;
    }
    else if (AppData->NoAckCount >= LORA_TDM_APP_LINK_LOSS_THRESHOLD)
    {
        AppData->LinkState = LORA_TDM_APP_LINK_DEGRADED;
    }
    else
    {
        AppData->LinkState = LORA_TDM_APP_LINK_CONNECTED;
    }
}

/* ---- Hex nibble helper ---- */

static int HexNibble(char C)
{
    if (C >= '0' && C <= '9') return C - '0';
    if (C >= 'a' && C <= 'f') return C - 'a' + 10;
    if (C >= 'A' && C <= 'F') return C - 'A' + 10;
    return -1;
}

/* ---- Parse UP frame and forward to uplink_app via SB ---- */

static void ProcessUpFrame(const char *Line, LORA_TDM_APP_Data_t *AppData)
{
    char     VersionStr[8], ClassStr[8], SeqStr[8], FlagsStr[8];
    char     PayloadHex[196 * 2 + 2];  /* max payload hex */
    char     CrcStr[8];
    char     Canonical[640];
    int      CanonLen;
    uint16   ExpectedCrc;
    uint16   ActualCrc;
    uint8    Version;
    uint8    CommandClass;
    uint16   Seq;
    uint8    Flags;
    uint8    Payload[196];
    uint16   PayloadLen;
    uint16   HexLen;
    uint16   i;
    LORA_TDM_APP_UplinkFwdCmd_t FwdCmd;

    if (sscanf(Line + 3, "%7[^,],%7[^,],%7[^,],%7[^,],%[^,],%7s",
               VersionStr, ClassStr, SeqStr, FlagsStr, PayloadHex, CrcStr) != 6)
    {
        /* "%[^,]" requires >=1 char, so a zero-length payload_hex field (",,")
         * falls through here. Retry assuming an empty payload before failing. */
        PayloadHex[0] = '\0';
        if (sscanf(Line + 3, "%7[^,],%7[^,],%7[^,],%7[^,],,%7s",
                   VersionStr, ClassStr, SeqStr, FlagsStr, CrcStr) != 5)
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: UP frame parse error");
            return;
        }
    }

    Version      = (uint8)strtoul(VersionStr, NULL, 0);
    CommandClass = (uint8)strtoul(ClassStr, NULL, 0);
    Seq          = (uint16)strtoul(SeqStr, NULL, 0);
    Flags        = (uint8)strtoul(FlagsStr, NULL, 0);
    ExpectedCrc  = (uint16)strtoul(CrcStr, NULL, 16);

    CanonLen = snprintf(Canonical, sizeof(Canonical), "UP,%s,%s,%s,%s,%s",
                        VersionStr, ClassStr, SeqStr, FlagsStr, PayloadHex);
    if (CanonLen <= 0 || (size_t)CanonLen >= sizeof(Canonical))
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        return;
    }

    ActualCrc = LORA_TDM_APP_Crc16((const uint8 *)Canonical, (size_t)CanonLen);
    if (ActualCrc != ExpectedCrc)
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: UP frame CRC fail expected=%04X actual=%04X",
                          (unsigned)ExpectedCrc, (unsigned)ActualCrc);
        return;
    }

    HexLen = (uint16)strlen(PayloadHex);
    if (HexLen % 2 != 0 || HexLen / 2 > sizeof(Payload))
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        return;
    }

    PayloadLen = HexLen / 2;
    for (i = 0; i < PayloadLen; i++)
    {
        int Hi = HexNibble(PayloadHex[i * 2]);
        int Lo = HexNibble(PayloadHex[i * 2 + 1]);
        if (Hi < 0 || Lo < 0)
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            return;
        }
        Payload[i] = (uint8)((Hi << 4) | Lo);
    }

    memset(&FwdCmd, 0, sizeof(FwdCmd));
    CFE_MSG_Init(CFE_MSG_PTR(FwdCmd.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_APP_CMD_MID_VALUE),
                 sizeof(FwdCmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(FwdCmd.CommandHeader), LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC);
    FwdCmd.Version       = Version;
    FwdCmd.CommandClass  = CommandClass;
    FwdCmd.PayloadLength = (uint8)PayloadLen;
    FwdCmd.Flags         = Flags;
    FwdCmd.Sequence      = Seq;
    if (PayloadLen > 0)
    {
        memcpy(FwdCmd.Payload, Payload, PayloadLen);
    }

    /* CRC-16/CCITT-FALSE over Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload
     * mirrors build_process_uplink_payload() in Python tools */
    {
        uint8 CrcBuf[6 + 196];
        CrcBuf[0] = Version;
        CrcBuf[1] = CommandClass;
        CrcBuf[2] = (uint8)PayloadLen;
        CrcBuf[3] = Flags;
        CrcBuf[4] = (uint8)(Seq & 0xFF);
        CrcBuf[5] = (uint8)(Seq >> 8);
        if (PayloadLen > 0) { memcpy(&CrcBuf[6], Payload, PayloadLen); }
        FwdCmd.Checksum = LORA_TDM_APP_Crc16(CrcBuf, 6 + PayloadLen);
    }

    CFE_SB_TransmitMsg(CFE_MSG_PTR(FwdCmd.CommandHeader), true);

    AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    AppData->RxCmdCount++;
}

/* ---- Process a received line ---- */

void LORA_TDM_APP_ProcessRxLine(const char *Line, LORA_TDM_APP_Data_t *AppData)
{
    uint32               SeqEcho;
    LORA_TDM_AckResult_t AckResult;

    if (Line == NULL || AppData == NULL)
    {
        return;
    }

    if (strncmp(Line, "ACK,", 4) == 0)
    {
        AckResult = LORA_TDM_APP_ParseAckFrame(Line, &SeqEcho);
        if (AckResult == LORA_TDM_ACK_OK)
        {
            AppData->RxAckCount++;
            AppData->NoAckCount         = 0;
            AppData->LastAckTimestampMs = UtilsGetTimeMs();
            (void)SeqEcho;
        }
        else
        {
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_ACK_PARSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: bad ACK frame");
        }
        return;
    }

    if (strncmp(Line, "UP,", 3) == 0)
    {
        ProcessUpFrame(Line, AppData);
        return;
    }

    /* Unknown frame type — ignore silently */
}

/* ---- Forward UP2 결과를 uplink_app으로 SB 전달 (ProcessUpFrame의 v2 대응) ---- */

static void ForwardUp2ToUplinkApp(const LORA_TDM_APP_Up2Decoded_t *Decoded, LORA_TDM_APP_Data_t *AppData)
{
    LORA_TDM_APP_UplinkFwdCmd_t FwdCmd;

    memset(&FwdCmd, 0, sizeof(FwdCmd));
    CFE_MSG_Init(CFE_MSG_PTR(FwdCmd.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_APP_CMD_MID_VALUE),
                 sizeof(FwdCmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(FwdCmd.CommandHeader), LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC);
    FwdCmd.Version       = Decoded->Version;
    FwdCmd.CommandClass  = Decoded->CommandClass;
    FwdCmd.PayloadLength = Decoded->PayloadLen;
    FwdCmd.Flags         = 0;
    FwdCmd.Sequence      = Decoded->Seq;
    if (Decoded->PayloadLen > 0)
    {
        memcpy(FwdCmd.Payload, Decoded->Payload, Decoded->PayloadLen);
    }

    /* ProcessUpFrame과 동일한 CRC 대상 구성 (Version+CommandClass+PayloadLength+Flags+Seq(LE)+Payload) */
    {
        uint8 CrcBuf[6 + 196];
        CrcBuf[0] = Decoded->Version;
        CrcBuf[1] = Decoded->CommandClass;
        CrcBuf[2] = Decoded->PayloadLen;
        CrcBuf[3] = 0;
        CrcBuf[4] = (uint8)(Decoded->Seq & 0xFF);
        CrcBuf[5] = (uint8)(Decoded->Seq >> 8);
        if (Decoded->PayloadLen > 0)
        {
            memcpy(&CrcBuf[6], Decoded->Payload, Decoded->PayloadLen);
        }
        FwdCmd.Checksum = LORA_TDM_APP_Crc16(CrcBuf, 6u + Decoded->PayloadLen);
    }

    CFE_SB_TransmitMsg(CFE_MSG_PTR(FwdCmd.CommandHeader), true);

    AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    AppData->RxCmdCount++;
}

/* ---- 완성된 v2 바이너리 프레임 1개 처리 (ProcessRxLine의 v2 대응) ---- */

void LORA_TDM_APP_ProcessRxBinaryFrame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Data_t *AppData)
{
    uint32                    SeqEcho;
    LORA_TDM_APP_Up2Decoded_t Decoded;

    if (Buf == NULL || Len == 0 || AppData == NULL)
    {
        return;
    }

    if (Buf[0] == (uint8)LORA_TDM_APP_ACK2_MAGIC)
    {
        if (LORA_TDM_APP_ParseAck2Frame(Buf, Len, &SeqEcho) == LORA_TDM_ACK_OK)
        {
            AppData->RxAckCount++;
            AppData->NoAckCount         = 0;
            AppData->LastAckTimestampMs = UtilsGetTimeMs();
            (void)SeqEcho;
        }
        else
        {
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_ACK_PARSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: bad ACK2 frame");
        }
        return;
    }

    if (Buf[0] == (uint8)LORA_TDM_APP_UP2_MAGIC)
    {
        if (LORA_TDM_APP_ParseUp2Frame(Buf, Len, &Decoded) == LORA_TDM_ACK_OK)
        {
            ForwardUp2ToUplinkApp(&Decoded, AppData);
        }
        else
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: UP2 frame parse error");
        }
        return;
    }

    /* Unknown magic — ignore silently */
}

/* ---- CONFIG_CMD_MID 처리 (§8, openMCT UPLINK_CLASS_CONFIG 경로) ---- */

static uint16 ConfigChecksum(const LORA_TDM_APP_ConfigPayloadHdr_t *Hdr, const uint8 *ValueBytes, uint8 ValueLength)
{
    uint16 Sum = 0;
    uint8  i;

    Sum += (uint16)Hdr->ConfigScope;
    Sum += (uint16)Hdr->ConfigVersion;
    Sum += (uint16)(Hdr->ParameterId & 0xFFU);
    Sum += (uint16)((Hdr->ParameterId >> 8U) & 0xFFU);
    Sum += (uint16)Hdr->ValueType;
    Sum += (uint16)Hdr->ValueLength;
    for (i = 0; i < ValueLength; i++)
    {
        Sum += (uint16)ValueBytes[i];
    }
    return Sum;
}

void LORA_TDM_APP_ProcessConfigCommand(const LORA_TDM_APP_ConfigCmdTlm_t *Msg)
{
    const LORA_TDM_APP_ConfigPayloadHdr_t *Hdr;
    const uint8                           *ValueBytes;
    uint32                                  Value;
    uint16                                  Expected;

    if (Msg == NULL || Msg->PayloadLength < (uint8)sizeof(LORA_TDM_APP_ConfigPayloadHdr_t))
    {
        return;
    }

    Hdr = (const LORA_TDM_APP_ConfigPayloadHdr_t *)Msg->Payload;

    /* scope가 다르면 (cfs_core_app/mavlink_bridge_app 대상) 조용히 무시 — 여러 앱이
     * 같은 MID를 구독하는 정상 상황이라 에러가 아니다. */
    if (Hdr->ConfigScope != (uint8)LORA_TDM_APP_CONFIG_SCOPE)
    {
        return;
    }

    if (Hdr->ConfigVersion != (uint8)LORA_TDM_APP_CONFIG_VERSION)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        return;
    }

    if (Hdr->ValueLength != sizeof(uint32) ||
        (uint8)(sizeof(*Hdr) + Hdr->ValueLength) > Msg->PayloadLength)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        return;
    }

    ValueBytes = Msg->Payload + sizeof(*Hdr);
    Expected   = ConfigChecksum(Hdr, ValueBytes, Hdr->ValueLength);
    if (Hdr->Checksum != Expected)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: config checksum mismatch got=0x%04X expected=0x%04X",
                          (unsigned int)Hdr->Checksum, (unsigned int)Expected);
        return;
    }

    memcpy(&Value, ValueBytes, sizeof(Value));

    switch (Hdr->ParameterId)
    {
        case LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL:
            LORA_TDM_APP_Data.UseV2Downlink = (Value != 0U) ? 1U : 0U;
            LORA_TDM_APP_Data.CmdCounter++;
            CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: downlink protocol set to %s (via CONFIG_CMD_MID)",
                              LORA_TDM_APP_Data.UseV2Downlink ? "v2(DL2)" : "v1(text)");
            break;

        default:
            LORA_TDM_APP_Data.ErrCounter++;
            break;
    }
}

/* ---- Update FC state cache from SB message ---- */

void LORA_TDM_APP_UpdateCacheFromMsg(CFE_SB_Buffer_t *SBBufPtr, LORA_TDM_APP_Data_t *AppData)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE)))
    {
        /* Layout must match source app telemetry struct */
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 TimestampMs; uint32 Seq;
            uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 Reserved;
            float RollRad; float PitchRad; float YawRad;
            float RollspeedRps; float PitchspeedRps; float YawspeedRps;
        } AttMsg_t;
        const AttMsg_t *M = (const AttMsg_t *)SBBufPtr;
        AppData->FcState.TimestampMs   = M->TimestampMs;
        AppData->FcState.AttitudeValid = M->Valid;
        AppData->FcState.RollRad       = M->RollRad;
        AppData->FcState.PitchRad      = M->PitchRad;
        AppData->FcState.YawRad        = M->YawRad;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE)))
    {
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 TimestampMs; uint32 Seq;
            uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 Reserved;
            float X_m; float Y_m; float Z_m;
            float Vx_mps; float Vy_mps; float Vz_mps;
        } LocalMsg_t;
        const LocalMsg_t *M = (const LocalMsg_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.LocalValid  = M->Valid;
        AppData->FcState.PosX        = M->X_m;
        AppData->FcState.PosY        = M->Y_m;
        AppData->FcState.PosZ        = M->Z_m;
        AppData->FcState.VelX        = M->Vx_mps;
        AppData->FcState.VelY        = M->Vy_mps;
        AppData->FcState.VelZ        = M->Vz_mps;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE)))
    {
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 TimestampMs; uint32 Seq;
            uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 FixType;
            uint8 SatellitesVisible; uint8 Reserved;
            int32 LatE7; int32 LonE7; int32 AltMm;
        } GpsMsg_t;
        const GpsMsg_t *M = (const GpsMsg_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.GpsValid    = M->Valid;
        AppData->FcState.LatE7       = M->LatE7;
        AppData->FcState.LonE7       = M->LonE7;
        AppData->FcState.AltMm       = M->AltMm;
        AppData->FcState.GpsFix      = M->FixType;
        AppData->FcState.SatellitesVisible = M->SatellitesVisible;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE)))
    {
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 Seq; uint32 TimestampMs; uint32 LastValidInputTimestampMs;
            uint8 HealthState; uint8 FaultCode; uint8 RecoveryRequested; uint8 Reserved;
        } SHMsg_t;
        const SHMsg_t *M = (const SHMsg_t *)SBBufPtr;
        AppData->SystemHealth.TimestampMs       = M->TimestampMs;
        AppData->SystemHealth.SystemHealthState = M->HealthState;
        AppData->SystemHealth.FaultCode         = M->FaultCode;
        AppData->PacketType = LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE)))
    {
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 TimestampMs; uint32 Seq;
            uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 Reserved;
        } EkfMsg_t;
        const EkfMsg_t *M = (const EkfMsg_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.EkfValid    = M->Valid;
        AppData->PacketType = LORA_TDM_APP_FC_STATE_PACKET_TYPE;
    }
}

void LORA_TDM_APP_ProcessDiagnosticCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    const LORA_TDM_APP_DiagnosticCmdTlm_t *Msg = (const LORA_TDM_APP_DiagnosticCmdTlm_t *)SBBufPtr;

    LORA_TDM_APP_Data.CmdCounter++;

    switch (Msg->DiagAction)
    {
        case LORA_TDM_APP_DIAG_ACTION_LINK_STATUS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag LINK_STATUS seq=%u link=%u noack=%lu dlseq=%lu token=%lu",
                              (unsigned int)Msg->SourceSequence, (unsigned int)LORA_TDM_APP_Data.LinkState,
                              (unsigned long)LORA_TDM_APP_Data.NoAckCount,
                              (unsigned long)LORA_TDM_APP_Data.DownlinkSeq,
                              (unsigned long)Msg->RequestToken);
            break;

        case LORA_TDM_APP_DIAG_ACTION_RX_STATS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag RX_STATS seq=%u rxcmd=%lu rxack=%lu rxerr=%u token=%lu",
                              (unsigned int)Msg->SourceSequence,
                              (unsigned long)LORA_TDM_APP_Data.RxCmdCount,
                              (unsigned long)LORA_TDM_APP_Data.RxAckCount,
                              (unsigned int)LORA_TDM_APP_Data.RxErrorCount,
                              (unsigned long)Msg->RequestToken);
            break;

        case LORA_TDM_APP_DIAG_ACTION_TX_STATS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag TX_STATS seq=%u txcount=%lu token=%lu",
                              (unsigned int)Msg->SourceSequence,
                              (unsigned long)LORA_TDM_APP_Data.TxCount,
                              (unsigned long)Msg->RequestToken);
            break;

        default:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: diag UNKNOWN action=%u seq=%u",
                              (unsigned int)Msg->DiagAction, (unsigned int)Msg->SourceSequence);
            break;
    }
}

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
 * Format: FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat>,<lon>,<alt>,<fix>,<ufb>\n
 */
int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    int Len;

    Len = snprintf(Buf, BufLen,
                   "FC,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%ld,%ld,%ld,%u,%u\n",
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
                   (unsigned)AppData->PendingUplinkFeedback);

    if (Len <= 0 || (size_t)Len >= BufLen)
    {
        return -1;
    }
    return Len;
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

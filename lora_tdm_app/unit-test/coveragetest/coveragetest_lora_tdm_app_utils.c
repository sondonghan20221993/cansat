#include "lora_tdm_app_coveragetest_common.h"
#include "system_health_msg.h"
#include "fc_state_msg.h"

/* ---- Helper: build a valid UP frame dynamically using the CRC function under test ---- */
static void BuildValidUpFrame(char *Buf, size_t BufLen, uint8 Ver, uint8 Class, uint16 Seq, uint8 Flags,
                               const char *PayloadHex)
{
    char   Body[LORA_TDM_APP_LINE_BUF_LEN];
    uint16 Crc;
    int    BodyLen;

    BodyLen = snprintf(Body, sizeof(Body), "UP,%u,%u,%u,%u,%s",
                       (unsigned)Ver, (unsigned)Class, (unsigned)Seq, (unsigned)Flags, PayloadHex);
    Crc = LORA_TDM_APP_Crc16((const uint8 *)Body, (size_t)BodyLen);
    snprintf(Buf, BufLen, "%s,%04X\n", Body, (unsigned)Crc);
}

/* ---- CRC-16 smoke test ---- */

void Test_Crc16_KnownVector(void)
{
    /* CRC-16/CCITT-FALSE of "123456789" = 0x29B1 */
    const uint8 Data[] = "123456789";
    uint16      Result = LORA_TDM_APP_Crc16(Data, sizeof(Data) - 1);
    UtAssert_INT32_EQ(Result, 0x29B1);
}

/* ---- ParseAckFrame ---- */

void Test_ParseAckFrame_Valid(void)
{
    uint32           SeqEcho = 0;
    LORA_TDM_AckResult_t Result;

    Result = LORA_TDM_APP_ParseAckFrame("ACK,42\n", &SeqEcho);
    UtAssert_INT32_EQ(Result, LORA_TDM_ACK_OK);
    UtAssert_INT32_EQ((int)SeqEcho, 42);
}

void Test_ParseAckFrame_ZeroSeq(void)
{
    uint32           SeqEcho = 99;
    LORA_TDM_AckResult_t Result;

    Result = LORA_TDM_APP_ParseAckFrame("ACK,0\n", &SeqEcho);
    UtAssert_INT32_EQ(Result, LORA_TDM_ACK_OK);
    UtAssert_INT32_EQ((int)SeqEcho, 0);
}

void Test_ParseAckFrame_WrongPrefix(void)
{
    uint32           SeqEcho = 0;
    LORA_TDM_AckResult_t Result;

    Result = LORA_TDM_APP_ParseAckFrame("NAK,42\n", &SeqEcho);
    UtAssert_INT32_EQ(Result, LORA_TDM_ACK_INVALID);
}

void Test_ParseAckFrame_MalformedNoSeq(void)
{
    uint32           SeqEcho = 0;
    LORA_TDM_AckResult_t Result;

    Result = LORA_TDM_APP_ParseAckFrame("ACK,\n", &SeqEcho);
    UtAssert_INT32_EQ(Result, LORA_TDM_ACK_INVALID);
}

/* ---- BuildFcDownlinkLine ---- */

void Test_BuildFcDownlinkLine_Basic(void)
{
    char Buf[LORA_TDM_APP_LINE_BUF_LEN];
    int  Len;

    LORA_TDM_APP_Data.DownlinkSeq = 1;
    LORA_TDM_APP_Data.FcState.TimestampMs  = 1000;
    LORA_TDM_APP_Data.FcState.RollRad      = 0.1f;
    LORA_TDM_APP_Data.FcState.PitchRad     = 0.2f;
    LORA_TDM_APP_Data.FcState.YawRad       = 0.3f;
    LORA_TDM_APP_Data.FcState.PosX         = 1.0f;
    LORA_TDM_APP_Data.FcState.PosY         = 2.0f;
    LORA_TDM_APP_Data.FcState.PosZ         = 3.0f;
    LORA_TDM_APP_Data.FcState.VelX         = 0.5f;
    LORA_TDM_APP_Data.FcState.VelY         = 0.6f;
    LORA_TDM_APP_Data.FcState.VelZ         = 0.7f;
    LORA_TDM_APP_Data.FcState.LatE7        = 374530000;
    LORA_TDM_APP_Data.FcState.LonE7        = 1269850000;
    LORA_TDM_APP_Data.FcState.AltMm        = 50000;
    LORA_TDM_APP_Data.FcState.GpsFix       = 3;
    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;

    Len = LORA_TDM_APP_BuildFcDownlinkLine(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_True(Len > 0, "BuildFcDownlinkLine returned positive length");
    UtAssert_True(strncmp(Buf, "FC,", 3) == 0, "Line starts with FC,");
    UtAssert_True(Buf[Len - 1] == '\n', "Line ends with newline");
}

void Test_BuildFcDownlinkLine_UplinkFeedbackField(void)
{
    char Buf[LORA_TDM_APP_LINE_BUF_LEN];
    char *UfbPtr;

    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
    LORA_TDM_APP_Data.FcState.SatellitesVisible = 0;
    LORA_TDM_APP_BuildFcDownlinkLine(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    /* ufb는 이제 마지막-1 필드(sats가 새 마지막 필드, 2026-07-13 추가).
     * ",1,<sats>\n" 형태로 등장해야 한다. */
    UfbPtr = strstr(Buf, ",1,0\n");
    UtAssert_True(UfbPtr != NULL, "CRC_FAIL feedback (1) encoded before sats field in FC line");
}

/* sats(SatellitesVisible)가 새 마지막 필드로 인코딩되는지 확인 (2026-07-13 추가) */
void Test_BuildFcDownlinkLine_SatellitesField(void)
{
    char Buf[LORA_TDM_APP_LINE_BUF_LEN];
    char *SatsPtr;

    LORA_TDM_APP_Data.PendingUplinkFeedback     = LORA_TDM_APP_UPLINK_FB_OK;
    LORA_TDM_APP_Data.FcState.SatellitesVisible = 12;
    LORA_TDM_APP_BuildFcDownlinkLine(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    /* 마지막 필드가 sats=12로 줄 끝(",12\n")에 나와야 한다 */
    SatsPtr = strstr(Buf, ",12\n");
    UtAssert_True(SatsPtr != NULL, "sats=12 encoded as last field in FC line");
}

void Test_BuildFcDownlinkLine_BufferTooSmall(void)
{
    char SmallBuf[4];
    int  Len;

    Len = LORA_TDM_APP_BuildFcDownlinkLine(SmallBuf, sizeof(SmallBuf), &LORA_TDM_APP_Data);
    UtAssert_True(Len < 0, "BuildFcDownlinkLine returns <0 for undersized buffer");
}

/* ---- BuildShDownlinkLine ---- */

void Test_BuildShDownlinkLine_Basic(void)
{
    char Buf[LORA_TDM_APP_LINE_BUF_LEN];
    int  Len;

    LORA_TDM_APP_Data.DownlinkSeq                   = 2;
    LORA_TDM_APP_Data.SystemHealth.TimestampMs       = 2000;
    LORA_TDM_APP_Data.SystemHealth.SystemHealthState = 1;
    LORA_TDM_APP_Data.SystemHealth.FaultCode         = 0;
    LORA_TDM_APP_Data.LinkState                      = LORA_TDM_APP_LINK_CONNECTED;
    LORA_TDM_APP_Data.PendingUplinkFeedback          = LORA_TDM_APP_UPLINK_FB_OK;

    Len = LORA_TDM_APP_BuildShDownlinkLine(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_True(Len > 0, "BuildShDownlinkLine returned positive length");
    UtAssert_True(strncmp(Buf, "SH,", 3) == 0, "Line starts with SH,");
    UtAssert_True(Buf[Len - 1] == '\n', "Line ends with newline");
}

/* ---- UpdateLinkState ---- */

void Test_UpdateLinkState_Connected(void)
{
    LORA_TDM_APP_Data.NoAckCount          = 0;
    LORA_TDM_APP_Data.LastAckTimestampMs  = 1000;

    LORA_TDM_APP_UpdateLinkState(&LORA_TDM_APP_Data, 2000); /* 1 s gap, within timeout */

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkState, LORA_TDM_APP_LINK_CONNECTED);
}

void Test_UpdateLinkState_Degraded(void)
{
    LORA_TDM_APP_Data.NoAckCount         = LORA_TDM_APP_LINK_LOSS_THRESHOLD;
    LORA_TDM_APP_Data.LastAckTimestampMs = 1000;

    LORA_TDM_APP_UpdateLinkState(&LORA_TDM_APP_Data, 2000);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkState, LORA_TDM_APP_LINK_DEGRADED);
}

void Test_UpdateLinkState_Disconnected(void)
{
    LORA_TDM_APP_Data.NoAckCount         = LORA_TDM_APP_LINK_LOSS_THRESHOLD;
    LORA_TDM_APP_Data.LastAckTimestampMs = 1000;

    /* Advance time beyond LINK_TIMEOUT_MS */
    LORA_TDM_APP_UpdateLinkState(&LORA_TDM_APP_Data, 1000 + LORA_TDM_APP_LINK_TIMEOUT_MS + 1);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkState, LORA_TDM_APP_LINK_DISCONNECTED);
}

/* ---- ProcessRxLine: ACK ---- */

void Test_ProcessRxLine_Ack(void)
{
    LORA_TDM_APP_Data.DownlinkSeq        = 7;
    LORA_TDM_APP_Data.LastSentSeq        = 7;
    LORA_TDM_APP_Data.NoAckCount         = 2;
    LORA_TDM_APP_Data.RxAckCount         = 0;
    LORA_TDM_APP_Data.LastAckTimestampMs = 0;

    LORA_TDM_APP_ProcessRxLine("ACK,7\n", &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.NoAckCount, 0);
}

/* 실전 타이밍 재현: TX 성공 후 DownlinkSeq는 이미 다음 값(N+1)으로 증가돼 있고,
 * LastSentSeq만 전송된 프레임의 seq(N)를 보존한다 — ACK,N 수신 시 SeqFailCount가
 * 잘못 증가하면 안 된다. (DownlinkSeq와 비교하던 과거 버그의 회귀 방지) */
void Test_ProcessRxLine_Ack_SeqMatch_NoFalseFail(void)
{
    LORA_TDM_APP_Data.LastSentSeq  = 7;
    LORA_TDM_APP_Data.DownlinkSeq  = 8; /* TX 성공 직후 이미 증가된 상태 */
    LORA_TDM_APP_Data.SeqFailCount = 0;

    LORA_TDM_APP_ProcessRxLine("ACK,7\n", &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SeqFailCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
}

void Test_ProcessRxLine_Ack_SeqMismatch(void)
{
    LORA_TDM_APP_Data.LastSentSeq  = 7;
    LORA_TDM_APP_Data.DownlinkSeq  = 8;
    LORA_TDM_APP_Data.SeqFailCount = 0;

    LORA_TDM_APP_ProcessRxLine("ACK,3\n", &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SeqFailCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
}

/* ---- ProcessRxLine: UP with CRC fail ---- */

void Test_ProcessRxLine_CrcFail(void)
{
    /* Deliberately wrong CRC (DEAD is not the real CRC) */
    LORA_TDM_APP_Data.RxErrorCount         = 0;
    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;

    LORA_TDM_APP_ProcessRxLine("UP,1,1,1,0,,DEAD\n", &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.PendingUplinkFeedback, LORA_TDM_APP_UPLINK_FB_CRC_FAIL);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxErrorCount, 1);
}

/* ---- ProcessRxLine: valid UP frame ---- */

void Test_ProcessRxLine_ValidUp(void)
{
    char Line[LORA_TDM_APP_LINE_BUF_LEN];

    BuildValidUpFrame(Line, sizeof(Line), 1, 5 /* CLASS_RECOVERY */, 1, 0, "");

    LORA_TDM_APP_Data.RxCmdCount           = 0;
    LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;

    LORA_TDM_APP_ProcessRxLine(Line, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxCmdCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.PendingUplinkFeedback, LORA_TDM_APP_UPLINK_FB_OK);
}

/* ---- UpdateCacheFromMsg ---- */

void Test_UpdateCacheFromMsg_Attitude(void)
{
    uint8               Storage[sizeof(FC_ATTITUDE_TLM_t)];
    CFE_SB_Buffer_t    *Buffer;
    FC_ATTITUDE_TLM_t *Msg;
    CFE_SB_MsgId_t      MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (FC_ATTITUDE_TLM_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs = 1111;
    Msg->Valid       = 1;
    Msg->RollRad     = 0.1f;
    Msg->PitchRad    = 0.2f;
    Msg->YawRad      = 0.3f;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.TimestampMs, 1111);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.AttitudeValid, 1);
    UtAssert_True(LORA_TDM_APP_Data.FcState.RollRad == 0.1f, "RollRad == 0.1");
    UtAssert_True(LORA_TDM_APP_Data.FcState.PitchRad == 0.2f, "PitchRad == 0.2");
    UtAssert_True(LORA_TDM_APP_Data.FcState.YawRad == 0.3f, "YawRad == 0.3");
}

void Test_UpdateCacheFromMsg_EkfLocal(void)
{
    uint8                Storage[sizeof(FC_EKF_LOCAL_TLM_t)];
    CFE_SB_Buffer_t     *Buffer;
    FC_EKF_LOCAL_TLM_t  *Msg;
    CFE_SB_MsgId_t       MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (FC_EKF_LOCAL_TLM_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs = 2222;
    Msg->Valid       = 1;
    Msg->X_m  = 1.0f; Msg->Y_m = 2.0f; Msg->Z_m = 3.0f;
    Msg->Vx_mps = 0.5f; Msg->Vy_mps = 0.6f; Msg->Vz_mps = 0.7f;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.TimestampMs, 2222);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.LocalValid, 1);
    UtAssert_True(LORA_TDM_APP_Data.FcState.PosX == 1.0f, "PosX == 1.0");
    UtAssert_True(LORA_TDM_APP_Data.FcState.VelX == 0.5f, "VelX == 0.5");
}

void Test_UpdateCacheFromMsg_Gps(void)
{
    uint8             Storage[sizeof(FC_GPS_RAW_TLM_t)];
    CFE_SB_Buffer_t  *Buffer;
    FC_GPS_RAW_TLM_t *Msg;
    CFE_SB_MsgId_t    MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (FC_GPS_RAW_TLM_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs       = 3333;
    Msg->Valid             = 1;
    Msg->FixType           = 3;
    Msg->SatellitesVisible = 12;
    Msg->LatE7             = 374530000;
    Msg->LonE7             = 1269850000;
    Msg->AltMm             = 50000;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.LatE7, 374530000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.LonE7, 1269850000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.AltMm, 50000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.GpsFix, 3);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.SatellitesVisible, 12);
}

void Test_UpdateCacheFromMsg_SystemHealth(void)
{
    uint8                   Storage[sizeof(SYSTEM_HEALTH_TLM_t)];
    CFE_SB_Buffer_t        *Buffer;
    SYSTEM_HEALTH_TLM_t    *Msg;
    CFE_SB_MsgId_t          MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (SYSTEM_HEALTH_TLM_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs = 4444;
    Msg->HealthState = 3;
    Msg->FaultCode   = 7;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SystemHealth.TimestampMs, 4444);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SystemHealth.SystemHealthState, 3);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SystemHealth.FaultCode, 7);
}

void Test_UpdateCacheFromMsg_EkfStatus(void)
{
    uint8                  Storage[sizeof(FC_STATE_PREFIX_t)];
    CFE_SB_Buffer_t       *Buffer;
    FC_STATE_PREFIX_t *Msg;
    CFE_SB_MsgId_t         MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (FC_STATE_PREFIX_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs = 5555;
    Msg->Valid       = 1;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.EkfValid, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.PacketType, LORA_TDM_APP_FC_STATE_PACKET_TYPE);
}

/* ---- BuildDl2Frame — lora_protocol_v2_spec.md §4 ---- */

static uint16 GetU16LE(const uint8 *P)
{
    return (uint16)(P[0] | ((uint16)P[1] << 8));
}

static int16 GetI16LE(const uint8 *P)
{
    return (int16)GetU16LE(P);
}

static int32 GetI32LE(const uint8 *P)
{
    uint32 V = (uint32)P[0] | ((uint32)P[1] << 8) | ((uint32)P[2] << 16) | ((uint32)P[3] << 24);
    return (int32)V;
}

void Test_BuildDl2Frame_Basic(void)
{
    uint8  Buf[LORA_TDM_APP_DL2_FRAME_LEN];
    int    Len;
    uint16 ExpectedCrc;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.DownlinkSeq              = 7;
    LORA_TDM_APP_Data.PendingUplinkFeedback    = LORA_TDM_APP_UPLINK_FB_OK;
    LORA_TDM_APP_Data.FcState.TimestampMs      = 123456;
    LORA_TDM_APP_Data.FcState.RollRad          = 0.1f;
    LORA_TDM_APP_Data.FcState.PitchRad         = -0.2f;
    LORA_TDM_APP_Data.FcState.YawRad           = 3.0f;
    LORA_TDM_APP_Data.FcState.PosX             = 1.5f;
    LORA_TDM_APP_Data.FcState.PosY             = -2.5f;
    LORA_TDM_APP_Data.FcState.PosZ             = 0.25f;
    LORA_TDM_APP_Data.FcState.VelX             = 0.5f;
    LORA_TDM_APP_Data.FcState.VelY             = -0.6f;
    LORA_TDM_APP_Data.FcState.VelZ             = 0.0f;
    LORA_TDM_APP_Data.FcState.LatE7            = 374530000;
    LORA_TDM_APP_Data.FcState.LonE7            = -1269850000;
    LORA_TDM_APP_Data.FcState.AltMm            = 50000;
    LORA_TDM_APP_Data.FcState.GpsFix           = 3;
    LORA_TDM_APP_Data.FcState.SatellitesVisible = 11;
    LORA_TDM_APP_Data.SystemHealth.SystemHealthState = 1;
    LORA_TDM_APP_Data.SystemHealth.FaultCode         = 0;
    LORA_TDM_APP_Data.LinkState                      = 1;

    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ(Len, (int)LORA_TDM_APP_DL2_FRAME_LEN);
    UtAssert_INT32_EQ(Buf[0], LORA_TDM_APP_DL2_MAGIC);
    UtAssert_INT32_EQ(Buf[1], LORA_TDM_APP_DL2_LEN_FIELD);
    UtAssert_INT32_EQ(GetU16LE(&Buf[2]), 7);
    UtAssert_INT32_EQ(Buf[4], 0); /* flags: saturation 없음 */
    UtAssert_INT32_EQ(Buf[5], LORA_TDM_APP_UPLINK_FB_OK);
    UtAssert_INT32_EQ(GetI32LE(&Buf[6]), 123456); /* ts (FcState.TimestampMs) */
    UtAssert_INT32_EQ(GetI16LE(&Buf[10]), 1000);   /* roll 0.1rad*1e4 */
    UtAssert_INT32_EQ(GetI16LE(&Buf[12]), -2000);  /* pitch -0.2rad*1e4 */
    UtAssert_INT32_EQ(GetI16LE(&Buf[16]), 150);    /* x 1.5m -> 150cm */
    UtAssert_INT32_EQ(GetI16LE(&Buf[18]), -250);   /* y -2.5m -> -250cm */
    UtAssert_INT32_EQ(GetI32LE(&Buf[28]), 374530000);
    UtAssert_INT32_EQ(GetI32LE(&Buf[32]), -1269850000);
    UtAssert_INT32_EQ(GetI32LE(&Buf[36]), 50000);
    UtAssert_INT32_EQ(Buf[40], 3);  /* fix */
    UtAssert_INT32_EQ(Buf[41], 11); /* sats */
    UtAssert_INT32_EQ(Buf[42], 1);  /* health */
    UtAssert_INT32_EQ(Buf[44], 1);  /* linkstate */

    ExpectedCrc = LORA_TDM_APP_Crc16(Buf, LORA_TDM_APP_DL2_LEN_FIELD);
    UtAssert_INT32_EQ(GetU16LE(&Buf[LORA_TDM_APP_DL2_LEN_FIELD]), ExpectedCrc);
}

/* 위치가 ±327.67m를 초과하면 clamp + flags bit1(0x02) 세팅 — §4.1 */
void Test_BuildDl2Frame_PositionSaturation(void)
{
    uint8 Buf[LORA_TDM_APP_DL2_FRAME_LEN];
    int   Len;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.FcState.PosX = 5000.0f; /* 초과 */

    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ(Len, (int)LORA_TDM_APP_DL2_FRAME_LEN);
    UtAssert_INT32_EQ(Buf[4], 0x02); /* flags bit1 */
    UtAssert_INT32_EQ(GetI16LE(&Buf[16]), 32767); /* clamp */
}

void Test_BuildDl2Frame_BufferTooSmall(void)
{
    uint8 Buf[LORA_TDM_APP_DL2_FRAME_LEN - 1];
    int   Len;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_True(Len < 0, "BufLen 부족 시 음수 반환");
}

/* FcState.TimeValid면 flags bit0(DL2_FLAG_SYSTIME) 세우고 55B로 확장,
 * TimeUnixUsec을 base 뒤(offset 45)에 u64 LE로 덧붙임 */
void Test_BuildDl2Frame_SysTimeIncluded(void)
{
    uint8  Buf[LORA_TDM_APP_DL2_MAX_FRAME_LEN];
    int    Len;
    uint16 ExpectedCrc;
    uint64 Got;
    int    i;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.FcState.TimeValid    = 1;
    LORA_TDM_APP_Data.FcState.TimeUnixUsec = 1752480000123456ULL;

    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ(Len, (int)LORA_TDM_APP_DL2_MAX_FRAME_LEN);
    UtAssert_INT32_EQ(Buf[1], (int)(LORA_TDM_APP_DL2_LEN_FIELD + LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN));
    UtAssert_True((Buf[4] & LORA_TDM_APP_DL2_FLAG_SYSTIME) != 0, "flags bit0 set");

    Got = 0;
    for (i = 7; i >= 0; i--)
    {
        Got = (Got << 8) | Buf[LORA_TDM_APP_DL2_LEN_FIELD + (uint8)i];
    }
    UtAssert_True(Got == 1752480000123456ULL, "TimeUnixUsec round-trip");

    ExpectedCrc = LORA_TDM_APP_Crc16(Buf, LORA_TDM_APP_DL2_LEN_FIELD + LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN);
    UtAssert_INT32_EQ(GetU16LE(&Buf[LORA_TDM_APP_DL2_LEN_FIELD + LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN]), ExpectedCrc);
}

/* TimeValid=0(기본)이면 기존과 동일하게 47B, flags bit0 미설정 — 회귀 확인 */
void Test_BuildDl2Frame_SysTimeNotValid_Excluded(void)
{
    uint8 Buf[LORA_TDM_APP_DL2_MAX_FRAME_LEN];
    int   Len;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.FcState.TimeValid    = 0;
    LORA_TDM_APP_Data.FcState.TimeUnixUsec = 999999ULL; /* stale/이전 값 남아있어도 무시돼야 함 */

    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ(Len, (int)LORA_TDM_APP_DL2_FRAME_LEN);
    UtAssert_INT32_EQ(Buf[1], (int)LORA_TDM_APP_DL2_LEN_FIELD);
    UtAssert_True((Buf[4] & LORA_TDM_APP_DL2_FLAG_SYSTIME) == 0, "flags bit0 not set");
}

/* SysTime 필요하지만 버퍼가 base(47B)만큼만 있으면 SysTime 없이 폴백(크래시 대신) */
void Test_BuildDl2Frame_SysTimeValid_BufferOnlyBaseSize_Fallback(void)
{
    uint8 Buf[LORA_TDM_APP_DL2_FRAME_LEN];
    int   Len;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.FcState.TimeValid    = 1;
    LORA_TDM_APP_Data.FcState.TimeUnixUsec = 123ULL;

    Len = LORA_TDM_APP_BuildDl2Frame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ(Len, (int)LORA_TDM_APP_DL2_FRAME_LEN);
    UtAssert_INT32_EQ(Buf[1], (int)LORA_TDM_APP_DL2_LEN_FIELD);
    UtAssert_True((Buf[4] & LORA_TDM_APP_DL2_FLAG_SYSTIME) == 0, "buffer 부족 시 SysTime 생략");
}

/* ---- UpdateCacheFromMsg: FC_SYS_TIME_MID ---- */

void Test_UpdateCacheFromMsg_SysTime(void)
{
    typedef struct {
        CFE_MSG_TelemetryHeader_t TelemetryHeader;
        uint32 TimestampMs; uint32 Seq;
        uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 Reserved;
        uint64 TimeUnixUsec;
    } TEST_SysTimeTlm_t;

    uint8               Storage[sizeof(TEST_SysTimeTlm_t)];
    CFE_SB_Buffer_t     *Buffer;
    TEST_SysTimeTlm_t   *Msg;
    CFE_SB_MsgId_t       MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_SysTimeTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_SYS_TIME_MID_VALUE), sizeof(*Msg));
    Msg->Valid        = 1;
    Msg->TimeUnixUsec = 1752480000123456ULL;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_SYS_TIME_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.TimeValid, 1);
    UtAssert_True(LORA_TDM_APP_Data.FcState.TimeUnixUsec == 1752480000123456ULL, "TimeUnixUsec cached");
}

/* ---- ParseAck2Frame — lora_protocol_v2_spec.md §6 ---- */

static void PutU16LE_Test(uint8 *P, uint16 V)
{
    P[0] = (uint8)(V & 0xFFu);
    P[1] = (uint8)((V >> 8) & 0xFFu);
}

void Test_ParseAck2Frame_Valid(void)
{
    uint8  Buf[5];
    uint16 Crc;
    uint32 SeqEcho = 0;

    Buf[0] = LORA_TDM_APP_ACK2_MAGIC;
    PutU16LE_Test(&Buf[1], 42);
    Crc = LORA_TDM_APP_Crc16(Buf, 3);
    PutU16LE_Test(&Buf[3], Crc);

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseAck2Frame(Buf, sizeof(Buf), &SeqEcho), LORA_TDM_ACK_OK);
    UtAssert_INT32_EQ((int)SeqEcho, 42);
}

void Test_ParseAck2Frame_WrongMagic(void)
{
    uint8  Buf[5] = {0x00, 0, 0, 0, 0};
    uint32 SeqEcho = 0;

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseAck2Frame(Buf, sizeof(Buf), &SeqEcho), LORA_TDM_ACK_INVALID);
}

void Test_ParseAck2Frame_CrcFail(void)
{
    uint8  Buf[5];
    uint32 SeqEcho = 0;

    Buf[0] = LORA_TDM_APP_ACK2_MAGIC;
    PutU16LE_Test(&Buf[1], 42);
    PutU16LE_Test(&Buf[3], 0xDEAD); /* 틀린 CRC */

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseAck2Frame(Buf, sizeof(Buf), &SeqEcho), LORA_TDM_ACK_INVALID);
}

void Test_ParseAck2Frame_TooShort(void)
{
    uint8  Buf[4] = {LORA_TDM_APP_ACK2_MAGIC, 0, 0, 0};
    uint32 SeqEcho = 0;

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseAck2Frame(Buf, sizeof(Buf), &SeqEcho), LORA_TDM_ACK_INVALID);
}

/* ---- ProcessRxBinaryFrame: ACK2 seq 검증 (TX 증가 후 타이밍 재현) ---- */

void Test_ProcessRxBinaryFrame_Ack2_SeqMatch_NoFalseFail(void)
{
    uint8  Buf[5];
    uint16 Crc;

    Buf[0] = LORA_TDM_APP_ACK2_MAGIC;
    PutU16LE_Test(&Buf[1], 7);
    Crc = LORA_TDM_APP_Crc16(Buf, 3);
    PutU16LE_Test(&Buf[3], Crc);

    LORA_TDM_APP_Data.LastSentSeq  = 7;
    LORA_TDM_APP_Data.DownlinkSeq  = 8; /* TX 성공 직후 이미 증가된 상태 */
    LORA_TDM_APP_Data.SeqFailCount = 0;

    LORA_TDM_APP_ProcessRxBinaryFrame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SeqFailCount, 0);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
}

void Test_ProcessRxBinaryFrame_Ack2_SeqMismatch(void)
{
    uint8  Buf[5];
    uint16 Crc;

    Buf[0] = LORA_TDM_APP_ACK2_MAGIC;
    PutU16LE_Test(&Buf[1], 3);
    Crc = LORA_TDM_APP_Crc16(Buf, 3);
    PutU16LE_Test(&Buf[3], Crc);

    LORA_TDM_APP_Data.LastSentSeq  = 7;
    LORA_TDM_APP_Data.DownlinkSeq  = 8;
    LORA_TDM_APP_Data.SeqFailCount = 0;

    LORA_TDM_APP_ProcessRxBinaryFrame(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.SeqFailCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
}

/* ---- ParseUp2Frame — lora_protocol_v2_spec.md §5 ---- */

void Test_ParseUp2Frame_ValidWithPayload(void)
{
    uint8                     Buf[16];
    uint16                    Crc;
    LORA_TDM_APP_Up2Decoded_t Out;

    Buf[0] = LORA_TDM_APP_UP2_MAGIC;
    Buf[1] = 3;    /* plen */
    Buf[2] = 2;    /* version */
    Buf[3] = 1;    /* command_class */
    PutU16LE_Test(&Buf[4], 99); /* seq */
    Buf[6] = 0;    /* flags */
    Buf[7] = 0xAA;
    Buf[8] = 0xBB;
    Buf[9] = 0xCC;
    Crc = LORA_TDM_APP_Crc16(Buf, 7 + 3);
    PutU16LE_Test(&Buf[10], Crc);

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseUp2Frame(Buf, 12, &Out), LORA_TDM_ACK_OK);
    UtAssert_INT32_EQ(Out.Version, 2);
    UtAssert_INT32_EQ(Out.CommandClass, 1);
    UtAssert_INT32_EQ((int)Out.Seq, 99);
    UtAssert_INT32_EQ(Out.PayloadLen, 3);
    UtAssert_INT32_EQ(Out.Payload[0], 0xAA);
    UtAssert_INT32_EQ(Out.Payload[2], 0xCC);
}

void Test_ParseUp2Frame_ZeroPayload(void)
{
    uint8                     Buf[9];
    uint16                    Crc;
    LORA_TDM_APP_Up2Decoded_t Out;

    Buf[0] = LORA_TDM_APP_UP2_MAGIC;
    Buf[1] = 0;
    Buf[2] = 2;
    Buf[3] = 4;
    PutU16LE_Test(&Buf[4], 7);
    Buf[6] = 0;
    Crc = LORA_TDM_APP_Crc16(Buf, 7);
    PutU16LE_Test(&Buf[7], Crc);

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseUp2Frame(Buf, sizeof(Buf), &Out), LORA_TDM_ACK_OK);
    UtAssert_INT32_EQ(Out.PayloadLen, 0);
}

void Test_ParseUp2Frame_CrcFail(void)
{
    /* plen=0 프레임(8B: magic,plen,ver,class,seq_lo,seq_hi,flags,crc_lo)에
     * 틀린 CRC(0xDEAD)를 심음 */
    uint8                     Buf[9] = {LORA_TDM_APP_UP2_MAGIC, 0, 2, 1, 0, 0, 0, 0xDE, 0xAD};
    LORA_TDM_APP_Up2Decoded_t Out;

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseUp2Frame(Buf, sizeof(Buf), &Out), LORA_TDM_ACK_INVALID);
}

void Test_ParseUp2Frame_TooShort(void)
{
    uint8                     Buf[4] = {LORA_TDM_APP_UP2_MAGIC, 0, 2, 1};
    LORA_TDM_APP_Up2Decoded_t Out;

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseUp2Frame(Buf, sizeof(Buf), &Out), LORA_TDM_ACK_INVALID);
}

void Test_ParseUp2Frame_PayloadLenExceedsBuf(void)
{
    /* plen이 실제 Len보다 큰 값을 주장 — 자를 판단 없이 거부해야 함 */
    uint8                     Buf[8] = {LORA_TDM_APP_UP2_MAGIC, 250, 2, 1, 0, 0, 0, 0};
    LORA_TDM_APP_Up2Decoded_t Out;

    UtAssert_INT32_EQ(LORA_TDM_APP_ParseUp2Frame(Buf, sizeof(Buf), &Out), LORA_TDM_ACK_INVALID);
}

/* ---- ProcessConfigCommand — openMCT fc_serial_ws_server.py _build_config_payload와 동일 레이아웃 ---- */

static uint16 CalcConfigChecksumTest(uint8 scope, uint8 version, uint16 param_id,
                                      uint8 value_type, uint8 value_len, const uint8 *value_bytes)
{
    uint16 sum = 0;
    uint8  i;
    sum += scope;
    sum += version;
    sum += (uint16)(param_id & 0xFFU);
    sum += (uint16)((param_id >> 8U) & 0xFFU);
    sum += value_type;
    sum += value_len;
    for (i = 0; i < value_len; i++) { sum += value_bytes[i]; }
    return sum;
}

static void BuildConfigMsgTest(LORA_TDM_APP_ConfigCmdTlm_t *Msg, uint8 scope, uint8 version,
                                uint16 param_id, uint32 value)
{
    LORA_TDM_APP_ConfigPayloadHdr_t *Hdr;
    uint8                            vbytes[4];

    memset(Msg, 0, sizeof(*Msg));
    Hdr = (LORA_TDM_APP_ConfigPayloadHdr_t *)Msg->Payload;
    Hdr->ConfigScope   = scope;
    Hdr->ConfigVersion = version;
    Hdr->ParameterId   = param_id;
    Hdr->ValueType     = 0;
    Hdr->ValueLength   = (uint8)sizeof(uint32);
    memcpy(Msg->Payload + sizeof(*Hdr), &value, sizeof(value));
    memcpy(vbytes, &value, sizeof(value));
    Hdr->Checksum = CalcConfigChecksumTest(scope, version, param_id, 0, (uint8)sizeof(uint32), vbytes);
    Msg->PayloadLength = (uint8)(sizeof(*Hdr) + sizeof(uint32));
}

void Test_ProcessConfigCommand_SetV2(void)
{
    LORA_TDM_APP_ConfigCmdTlm_t Msg;

    LORA_TDM_APP_Data.UseV2Downlink = 0;
    LORA_TDM_APP_Data.CmdCounter    = 0;
    BuildConfigMsgTest(&Msg, LORA_TDM_APP_CONFIG_SCOPE, LORA_TDM_APP_CONFIG_VERSION,
                       LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL, 1U);

    LORA_TDM_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ProcessConfigCommand_WrongScopeIgnoredSilently(void)
{
    LORA_TDM_APP_ConfigCmdTlm_t Msg;

    LORA_TDM_APP_Data.UseV2Downlink = 0;
    LORA_TDM_APP_Data.ErrCounter    = 0;
    /* scope=1(cfs_core_app 대상) — lora_tdm_app은 조용히 무시해야 함(에러 아님) */
    BuildConfigMsgTest(&Msg, 1U, LORA_TDM_APP_CONFIG_VERSION,
                       LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL, 1U);

    LORA_TDM_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 0);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 0);
}

void Test_ProcessConfigCommand_BadChecksumRejected(void)
{
    LORA_TDM_APP_ConfigCmdTlm_t Msg;

    LORA_TDM_APP_Data.UseV2Downlink = 0;
    LORA_TDM_APP_Data.ErrCounter    = 0;
    BuildConfigMsgTest(&Msg, LORA_TDM_APP_CONFIG_SCOPE, LORA_TDM_APP_CONFIG_VERSION,
                       LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL, 1U);
    ((LORA_TDM_APP_ConfigPayloadHdr_t *)Msg.Payload)->Checksum ^= 0xFFFFU; /* 손상 */

    LORA_TDM_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.UseV2Downlink, 0);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 1);
}

void Test_ProcessConfigCommand_UnknownParamRejected(void)
{
    LORA_TDM_APP_ConfigCmdTlm_t Msg;

    LORA_TDM_APP_Data.ErrCounter = 0;
    BuildConfigMsgTest(&Msg, LORA_TDM_APP_CONFIG_SCOPE, LORA_TDM_APP_CONFIG_VERSION, 99U, 1U);

    LORA_TDM_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.ErrCounter, 1);
}

/* ---- ProcessDiagnosticCommand (A-3.3, notes/temp/a3_unittest_gap_implementation.md) ---- */

static void BuildDiagnosticMsg(LORA_TDM_APP_DiagnosticCmdTlm_t *Msg, uint8 DiagAction)
{
    memset(Msg, 0, sizeof(*Msg));
    Msg->SourceSequence = 1;
    Msg->DiagAction     = DiagAction;
    Msg->DiagTarget     = 0;
    Msg->RequestToken   = 0x5A5A5A5A;
}

void Test_ProcessDiagnosticCommand_LinkStatus(void)
{
    LORA_TDM_APP_DiagnosticCmdTlm_t Msg;

    BuildDiagnosticMsg(&Msg, LORA_TDM_APP_DIAG_ACTION_LINK_STATUS);
    LORA_TDM_APP_Data.CmdCounter = 0;
    LORA_TDM_APP_Data.LinkState  = LORA_TDM_APP_LINK_CONNECTED;

    LORA_TDM_APP_ProcessDiagnosticCommand((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ProcessDiagnosticCommand_RxStats(void)
{
    LORA_TDM_APP_DiagnosticCmdTlm_t Msg;

    BuildDiagnosticMsg(&Msg, LORA_TDM_APP_DIAG_ACTION_RX_STATS);
    LORA_TDM_APP_Data.CmdCounter = 0;

    LORA_TDM_APP_ProcessDiagnosticCommand((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ProcessDiagnosticCommand_TxStats(void)
{
    LORA_TDM_APP_DiagnosticCmdTlm_t Msg;

    BuildDiagnosticMsg(&Msg, LORA_TDM_APP_DIAG_ACTION_TX_STATS);
    LORA_TDM_APP_Data.CmdCounter = 0;

    LORA_TDM_APP_ProcessDiagnosticCommand((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void Test_ProcessDiagnosticCommand_UnknownAction(void)
{
    LORA_TDM_APP_DiagnosticCmdTlm_t Msg;

    BuildDiagnosticMsg(&Msg, 0xFF);
    LORA_TDM_APP_Data.CmdCounter = 0;

    /* 매칭 case 없어도 크래시 없이 default(ERROR 이벤트)로 빠져야 함 */
    LORA_TDM_APP_ProcessDiagnosticCommand((CFE_SB_Buffer_t *)&Msg);

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.CmdCounter, 1);
}

void UtTest_Setup(void)
{
    ADD_TEST(Crc16_KnownVector);
    ADD_TEST(BuildDl2Frame_Basic);
    ADD_TEST(BuildDl2Frame_PositionSaturation);
    ADD_TEST(BuildDl2Frame_BufferTooSmall);
    ADD_TEST(BuildDl2Frame_SysTimeIncluded);
    ADD_TEST(BuildDl2Frame_SysTimeNotValid_Excluded);
    ADD_TEST(BuildDl2Frame_SysTimeValid_BufferOnlyBaseSize_Fallback);
    ADD_TEST(UpdateCacheFromMsg_SysTime);
    ADD_TEST(ParseAck2Frame_Valid);
    ADD_TEST(ParseAck2Frame_WrongMagic);
    ADD_TEST(ParseAck2Frame_CrcFail);
    ADD_TEST(ParseAck2Frame_TooShort);
    ADD_TEST(ProcessRxBinaryFrame_Ack2_SeqMatch_NoFalseFail);
    ADD_TEST(ProcessRxBinaryFrame_Ack2_SeqMismatch);
    ADD_TEST(ParseUp2Frame_ValidWithPayload);
    ADD_TEST(ParseUp2Frame_ZeroPayload);
    ADD_TEST(ParseUp2Frame_CrcFail);
    ADD_TEST(ParseUp2Frame_TooShort);
    ADD_TEST(ParseUp2Frame_PayloadLenExceedsBuf);
    ADD_TEST(ProcessConfigCommand_SetV2);
    ADD_TEST(ProcessConfigCommand_WrongScopeIgnoredSilently);
    ADD_TEST(ProcessConfigCommand_BadChecksumRejected);
    ADD_TEST(ProcessConfigCommand_UnknownParamRejected);
    ADD_TEST(ParseAckFrame_Valid);
    ADD_TEST(ParseAckFrame_ZeroSeq);
    ADD_TEST(ParseAckFrame_WrongPrefix);
    ADD_TEST(ParseAckFrame_MalformedNoSeq);
    ADD_TEST(BuildFcDownlinkLine_Basic);
    ADD_TEST(BuildFcDownlinkLine_UplinkFeedbackField);
    ADD_TEST(BuildFcDownlinkLine_SatellitesField);
    ADD_TEST(BuildFcDownlinkLine_BufferTooSmall);
    ADD_TEST(BuildShDownlinkLine_Basic);
    ADD_TEST(UpdateLinkState_Connected);
    ADD_TEST(UpdateLinkState_Degraded);
    ADD_TEST(UpdateLinkState_Disconnected);
    ADD_TEST(ProcessRxLine_Ack);
    ADD_TEST(ProcessRxLine_Ack_SeqMatch_NoFalseFail);
    ADD_TEST(ProcessRxLine_Ack_SeqMismatch);
    ADD_TEST(ProcessRxLine_CrcFail);
    ADD_TEST(ProcessRxLine_ValidUp);
    ADD_TEST(UpdateCacheFromMsg_Attitude);
    ADD_TEST(UpdateCacheFromMsg_EkfLocal);
    ADD_TEST(UpdateCacheFromMsg_Gps);
    ADD_TEST(UpdateCacheFromMsg_SystemHealth);
    ADD_TEST(UpdateCacheFromMsg_EkfStatus);
    ADD_TEST(ProcessDiagnosticCommand_LinkStatus);
    ADD_TEST(ProcessDiagnosticCommand_RxStats);
    ADD_TEST(ProcessDiagnosticCommand_TxStats);
    ADD_TEST(ProcessDiagnosticCommand_UnknownAction);
}

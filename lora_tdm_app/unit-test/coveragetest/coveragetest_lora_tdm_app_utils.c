#include "lora_tdm_app_coveragetest_common.h"

/* ---- Telemetry structs matching the source apps (used with UpdateCacheFromMsg) ---- */

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     RollRad;
    float                     PitchRad;
    float                     YawRad;
    float                     RollspeedRps;
    float                     PitchspeedRps;
    float                     YawspeedRps;
} TEST_AttitudeTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     X_m;
    float                     Y_m;
    float                     Z_m;
    float                     Vx_mps;
    float                     Vy_mps;
    float                     Vz_mps;
} TEST_EkfLocalTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     FixType;
    uint8                     SatellitesVisible;
    uint8                     Reserved;
    int32                     LatE7;
    int32                     LonE7;
    int32                     AltMm;
} TEST_GpsRawTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastValidInputTimestampMs;
    uint8                     HealthState;
    uint8                     FaultCode;
    uint8                     RecoveryRequested;
    uint8                     Reserved;
} TEST_SystemHealthTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} TEST_GenericStateTlm_t;

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
    LORA_TDM_APP_BuildFcDownlinkLine(Buf, sizeof(Buf), &LORA_TDM_APP_Data);

    /* The UFB field must appear in the line; scan for ",1," or ends with ",1\n" */
    UfbPtr = strstr(Buf, ",1\n");
    UtAssert_True(UfbPtr != NULL, "CRC_FAIL feedback (1) encoded in FC line");
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
    LORA_TDM_APP_Data.NoAckCount         = 2;
    LORA_TDM_APP_Data.RxAckCount         = 0;
    LORA_TDM_APP_Data.LastAckTimestampMs = 0;

    LORA_TDM_APP_ProcessRxLine("ACK,7\n", &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RxAckCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.NoAckCount, 0);
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
    uint8               Storage[sizeof(TEST_AttitudeTlm_t)];
    CFE_SB_Buffer_t    *Buffer;
    TEST_AttitudeTlm_t *Msg;
    CFE_SB_MsgId_t      MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_AttitudeTlm_t *)Storage;
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
    uint8                Storage[sizeof(TEST_EkfLocalTlm_t)];
    CFE_SB_Buffer_t     *Buffer;
    TEST_EkfLocalTlm_t  *Msg;
    CFE_SB_MsgId_t       MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_EkfLocalTlm_t *)Storage;
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
    uint8             Storage[sizeof(TEST_GpsRawTlm_t)];
    CFE_SB_Buffer_t  *Buffer;
    TEST_GpsRawTlm_t *Msg;
    CFE_SB_MsgId_t    MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_GpsRawTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Msg->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE), sizeof(*Msg));
    Msg->TimestampMs = 3333;
    Msg->Valid       = 1;
    Msg->FixType     = 3;
    Msg->LatE7       = 374530000;
    Msg->LonE7       = 1269850000;
    Msg->AltMm       = 50000;

    MsgId = CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_TDM_APP_UpdateCacheFromMsg(Buffer, &LORA_TDM_APP_Data);

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.LatE7, 374530000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.LonE7, 1269850000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.AltMm, 50000);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.FcState.GpsFix, 3);
}

void Test_UpdateCacheFromMsg_SystemHealth(void)
{
    uint8                   Storage[sizeof(TEST_SystemHealthTlm_t)];
    CFE_SB_Buffer_t        *Buffer;
    TEST_SystemHealthTlm_t *Msg;
    CFE_SB_MsgId_t          MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_SystemHealthTlm_t *)Storage;
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
    uint8                  Storage[sizeof(TEST_GenericStateTlm_t)];
    CFE_SB_Buffer_t       *Buffer;
    TEST_GenericStateTlm_t *Msg;
    CFE_SB_MsgId_t         MsgId;

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Msg    = (TEST_GenericStateTlm_t *)Storage;
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

void UtTest_Setup(void)
{
    ADD_TEST(Crc16_KnownVector);
    ADD_TEST(ParseAckFrame_Valid);
    ADD_TEST(ParseAckFrame_ZeroSeq);
    ADD_TEST(ParseAckFrame_WrongPrefix);
    ADD_TEST(ParseAckFrame_MalformedNoSeq);
    ADD_TEST(BuildFcDownlinkLine_Basic);
    ADD_TEST(BuildFcDownlinkLine_UplinkFeedbackField);
    ADD_TEST(BuildFcDownlinkLine_BufferTooSmall);
    ADD_TEST(BuildShDownlinkLine_Basic);
    ADD_TEST(UpdateLinkState_Connected);
    ADD_TEST(UpdateLinkState_Degraded);
    ADD_TEST(UpdateLinkState_Disconnected);
    ADD_TEST(ProcessRxLine_Ack);
    ADD_TEST(ProcessRxLine_CrcFail);
    ADD_TEST(ProcessRxLine_ValidUp);
    ADD_TEST(UpdateCacheFromMsg_Attitude);
    ADD_TEST(UpdateCacheFromMsg_EkfLocal);
    ADD_TEST(UpdateCacheFromMsg_Gps);
    ADD_TEST(UpdateCacheFromMsg_SystemHealth);
    ADD_TEST(UpdateCacheFromMsg_EkfStatus);
}

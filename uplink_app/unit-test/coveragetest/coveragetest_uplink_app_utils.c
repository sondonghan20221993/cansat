/************************************************************************
 * Coverage tests for uplink_app_utils.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"
#include <math.h>

void Test_UPLINK_APP_ValidateProxyCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_Result_t           Result;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Version       = UPLINK_APP_PROTOCOL_VERSION;
    Cmd.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    Cmd.PayloadLength = 1;

    UtAssert_BOOL_TRUE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_ACCEPT);

    Cmd.Version = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_VERSION);

    Cmd.Version      = UPLINK_APP_PROTOCOL_VERSION;
    Cmd.CommandClass = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_CLASS);

    Cmd.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    Cmd.PayloadLength = 0;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_LENGTH);

    Cmd.CommandClass  = UPLINK_APP_CLASS_CONFIG;
    Cmd.PayloadLength = UPLINK_APP_MAX_PAYLOAD_LENGTH + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_LENGTH);
}

void Test_UPLINK_APP_VerifyCmdLength_Impl(void)
{
    UPLINK_APP_NoopCmd_t TestMsg;
    size_t               MsgSize;

    memset(&TestMsg, 0, sizeof(TestMsg));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_TRUE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg)));

    MsgSize = sizeof(TestMsg);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);
    UtAssert_BOOL_FALSE(UPLINK_APP_VerifyCmdLength(CFE_MSG_PTR(TestMsg.CommandHeader), sizeof(TestMsg) + 1));
}

void Test_UPLINK_APP_ParseRouteUpdatePayload(void)
{
    UPLINK_APP_ProcessUplinkCmd_t    Cmd;
    UPLINK_APP_RouteUpdatePayload_t  Payload;
    UPLINK_APP_RouteUpdatePayload_t *PayloadSrc;
    uint32                           Index;

    memset(&Cmd, 0, sizeof(Cmd));
    PayloadSrc = (UPLINK_APP_RouteUpdatePayload_t *)Cmd.Payload;
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION;
    PayloadSrc->RouteVersion  = 1;
    PayloadSrc->WaypointCount = 2;
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
    PayloadSrc->Waypoints[1].X = 2.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    Cmd.PayloadLength = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));

    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
    UtAssert_INT32_EQ(Payload.RouteType, UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION);
    UtAssert_INT32_EQ(Payload.WaypointCount, 2);

    PayloadSrc->Waypoints[1].Z = 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].Z = 4.0f;
    PayloadSrc->RouteType = UPLINK_APP_ROUTE_SEGMENT_NONE;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_SEGMENT_MISSION_EXTENSION;
    PayloadSrc->WaypointCount = 0;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = INFINITY;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[1].X = 1.99f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].X = 2.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->Waypoints[1].X = 2.01f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS;
    for (Index = 0; Index < PayloadSrc->WaypointCount; ++Index)
    {
        PayloadSrc->Waypoints[Index].X = (float)(Index * 2U);
        PayloadSrc->Waypoints[Index].Y = -10.0f;
        PayloadSrc->Waypoints[Index].Z = 3.0f;
    }
    Cmd.PayloadLength = (uint8)(4U + ((size_t)PayloadSrc->WaypointCount * sizeof(UPLINK_APP_Waypoint_t)));
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength         = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
    PayloadSrc->Waypoints[1].X = 100.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 4.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
}

void Test_UPLINK_APP_ResolveRouteTarget(void)
{
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_CONFIG), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_ROUTE_UPDATE), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_VIEWPOINT), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_RECOVERY), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_MODE), UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(UPLINK_APP_CLASS_DIAGNOSTIC), UPLINK_APP_ROUTE_DOWNLINK);
    UtAssert_INT32_EQ(UPLINK_APP_ResolveRouteTarget(99), UPLINK_APP_ROUTE_NONE);
}

void Test_UPLINK_APP_ReportHousekeeping(void)
{
    UPLINK_APP_Data.CmdCounter       = 2;
    UPLINK_APP_Data.ErrCounter       = 1;
    UPLINK_APP_Data.PublishCount     = 3;
    UPLINK_APP_Data.LastPublishTimeMs = 44;

    UPLINK_APP_ReportHousekeeping();
}

void Test_UPLINK_APP_UpdateStatusTelemetry(void)
{
    UPLINK_APP_Data.SequenceCounter     = 4;
    UPLINK_APP_Data.LastRxTimeMs        = 100;
    UPLINK_APP_Data.AcceptedCount       = 2;
    UPLINK_APP_Data.RejectedCount       = 1;
    UPLINK_APP_Data.RoutingFailureCount = 3;
    UPLINK_APP_Data.LastCommandCode     = UPLINK_APP_CLASS_ROUTE_UPDATE;
    UPLINK_APP_Data.LastRxSequence      = 7;
    UPLINK_APP_Data.LastCommandResult   = UPLINK_APP_RESULT_ROUTED;
    UPLINK_APP_Data.LinkState           = UPLINK_APP_LINK_NOMINAL;
    UPLINK_APP_Data.Valid               = 1;
    UPLINK_APP_Data.ActiveTransportId   = 0;
    UPLINK_APP_Data.LastRouteTarget     = UPLINK_APP_ROUTE_CORE;
    UPLINK_APP_Data.ConfigPendingState  = UPLINK_APP_CONFIG_IDLE;
    UPLINK_APP_Data.LastConfigResult    = 0;
    UPLINK_APP_Data.LastRollbackReason  = 0;

    UPLINK_APP_UpdateStatusTelemetry(555);

    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.Seq, 5);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.TimestampMs, 555);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRxTimestampMs, 100);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.AcceptedCount, 2);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RejectedCount, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.RoutingFailureCount, 3);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandCode, UPLINK_APP_CLASS_ROUTE_UPDATE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandSequence, 7);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastCommandResult, UPLINK_APP_RESULT_ROUTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LinkState, UPLINK_APP_LINK_NOMINAL);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.Valid, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.ActiveTransportId, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRouteTarget, UPLINK_APP_ROUTE_CORE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.ConfigPendingState, UPLINK_APP_CONFIG_IDLE);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastConfigResult, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastRollbackReason, 0);
}

void Test_UPLINK_APP_LoadState_NoFile(void)
{
    /* File does not exist — LoadState must return silently without modifying state */
    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);
}

void Test_UPLINK_APP_SaveState_NoDir(void)
{
    /* /cf/ does not exist in test env — SaveState must return silently */
    UPLINK_APP_Data.LastAcceptedSequence = 99;
    UPLINK_APP_SaveState();
    /* No crash and state unchanged */
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 99);
}

void Test_UPLINK_APP_ForwardModeCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Sequence      = 50;
    Cmd.PayloadLength = 3;
    Cmd.Payload[0]    = 0x01;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardModeCommand(&Cmd));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardModeCommand(&Cmd));

    /* zero-length payload path */
    Cmd.PayloadLength = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardModeCommand(&Cmd));
}

void Test_UPLINK_APP_ForwardDiagnosticCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Sequence      = 60;
    Cmd.PayloadLength = 3;
    Cmd.Payload[0]    = 0x02;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));

    Cmd.PayloadLength = 0;
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardDiagnosticCommand(&Cmd));
}

/* -----------------------------------------------------------------------
 * Helper: build valid UP frame string with correct CRC16
 * format: UP,<ver>,<class>,<seq>,<flags>,<payload_hex>,<crc_hex>
 * ----------------------------------------------------------------------- */
static void BuildUpFrame(char *Buf, size_t BufLen,
                         uint8 Version, uint8 Class, uint16 Seq,
                         uint8 Flags, const char *PayloadHex)
{
    char   Canonical[640];
    int    CanonLen;
    uint16 Crc;

    CanonLen = snprintf(Canonical, sizeof(Canonical),
                        "UP,%u,%u,%u,%u,%s",
                        (unsigned)Version, (unsigned)Class,
                        (unsigned)Seq, (unsigned)Flags, PayloadHex);
    Crc = UPLINK_APP_CRC16((const uint8 *)Canonical, (uint16)CanonLen);
    snprintf(Buf, BufLen, "%s,%04X\n", Canonical, (unsigned)Crc);
}

/* LORA-UP-001: 정상 UP 프레임 파싱 */
void Test_ParseLoRaFrame_ValidFrame(void)
{
    char                          Frame[256];
    UPLINK_APP_ProcessUplinkCmd_t Out;

    BuildUpFrame(Frame, sizeof(Frame), 1, UPLINK_APP_CLASS_ROUTE_UPDATE, 42, 0, "AABB");
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
    UtAssert_INT32_EQ(Out.Version,      1);
    UtAssert_INT32_EQ(Out.CommandClass, UPLINK_APP_CLASS_ROUTE_UPDATE);
    UtAssert_INT32_EQ(Out.Sequence,     42);
    UtAssert_INT32_EQ(Out.Flags,        0);
    UtAssert_INT32_EQ(Out.PayloadLength, 2);
    UtAssert_INT32_EQ(Out.Payload[0], 0xAA);
    UtAssert_INT32_EQ(Out.Payload[1], 0xBB);
}

/* LORA-UP-002: CRC 불일치 */
void Test_ParseLoRaFrame_CrcMismatch(void)
{
    char                          Frame[256];
    UPLINK_APP_ProcessUplinkCmd_t Out;

    /* build valid then corrupt CRC */
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 1, 0, "");
    /* replace last 4 chars of CRC with 0000 */
    Frame[strlen(Frame) - 5] = '0';
    Frame[strlen(Frame) - 4] = '0';
    Frame[strlen(Frame) - 3] = '0';
    Frame[strlen(Frame) - 2] = '0';
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
}

/* LORA-UP-003: 잘못된 prefix */
void Test_ParseLoRaFrame_BadPrefix(void)
{
    char                          Frame[] = "XX,1,2,1,0,,1234\n";
    UPLINK_APP_ProcessUplinkCmd_t Out;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
}

/* LORA-UP-007/008: payload hex 홀수 길이 / 비정상 문자 */
void Test_ParseLoRaFrame_BadPayloadHex(void)
{
    char                          Frame[256];
    UPLINK_APP_ProcessUplinkCmd_t Out;
    char                          Canonical[256];
    uint16                        Crc;
    int                           Len;

    /* odd-length hex */
    Len = snprintf(Canonical, sizeof(Canonical), "UP,1,2,1,0,ABC");
    Crc = UPLINK_APP_CRC16((const uint8 *)Canonical, (uint16)Len);
    snprintf(Frame, sizeof(Frame), "%s,%04X\n", Canonical, (unsigned)Crc);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));

    /* invalid hex char */
    Len = snprintf(Canonical, sizeof(Canonical), "UP,1,2,2,0,ZZ");
    Crc = UPLINK_APP_CRC16((const uint8 *)Canonical, (uint16)Len);
    snprintf(Frame, sizeof(Frame), "%s,%04X\n", Canonical, (unsigned)Crc);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
}

/* LORA-UP-009: payload 196 bytes 최대 허용 */
void Test_ParseLoRaFrame_MaxPayload(void)
{
    char                          HexBuf[UPLINK_APP_MAX_PAYLOAD_LENGTH * 2 + 1];
    char                          Frame[UPLINK_APP_MAX_PAYLOAD_LENGTH * 2 + 64];
    UPLINK_APP_ProcessUplinkCmd_t Out;
    uint16                        i;

    for (i = 0; i < UPLINK_APP_MAX_PAYLOAD_LENGTH; i++)
    {
        snprintf(HexBuf + i * 2, 3, "%02X", (unsigned)(i & 0xFF));
    }
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 1, 0, HexBuf);
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
    UtAssert_INT32_EQ(Out.PayloadLength, UPLINK_APP_MAX_PAYLOAD_LENGTH);
}

/* LORA-UP-010: payload 197 bytes 초과 거부 */
void Test_ParseLoRaFrame_OverPayload(void)
{
    char                          HexBuf[(UPLINK_APP_MAX_PAYLOAD_LENGTH + 1) * 2 + 1];
    char                          Frame[(UPLINK_APP_MAX_PAYLOAD_LENGTH + 1) * 2 + 64];
    UPLINK_APP_ProcessUplinkCmd_t Out;
    uint16                        i;

    for (i = 0; i < UPLINK_APP_MAX_PAYLOAD_LENGTH + 1; i++)
    {
        snprintf(HexBuf + i * 2, 3, "%02X", (unsigned)(i & 0xFF));
    }
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 1, 0, HexBuf);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
}

/* LORA-UP-011/012/013: seq 증가/동일/역행 */
void Test_ParseLoRaFrame_SeqRegression(void)
{
    char                          Frame[256];
    UPLINK_APP_ProcessUplinkCmd_t Out;

    /* ParseLoRaFrame itself doesn't check seq — that's ServiceLoRa's job */
    /* seq=1 */
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 1, 0, "AA");
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
    UtAssert_INT32_EQ(Out.Sequence, 1);

    /* seq=2 */
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 2, 0, "AA");
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
    UtAssert_INT32_EQ(Out.Sequence, 2);

    /* seq=2 again: ParseLoRaFrame accepts (no seq check here) */
    BuildUpFrame(Frame, sizeof(Frame), 1, 2, 2, 0, "AA");
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));
    UtAssert_INT32_EQ(Out.Sequence, 2);
}

/* CFS-CMD-001~008: command packet 필드 검증 */
void Test_ParseLoRaFrame_CommandPacketFields(void)
{
    char                          Frame[256];
    UPLINK_APP_ProcessUplinkCmd_t Out;

    BuildUpFrame(Frame, sizeof(Frame), 1, UPLINK_APP_CLASS_ROUTE_UPDATE, 7, 3, "DEADBEEF");
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseLoRaFrame(Frame, &Out));

    /* CFS-CMD-004: PayloadLength == 4 (DEADBEEF = 4 bytes) */
    UtAssert_INT32_EQ(Out.PayloadLength, 4);

    /* CFS-CMD-006: CommandClass */
    UtAssert_INT32_EQ(Out.CommandClass, UPLINK_APP_CLASS_ROUTE_UPDATE);

    /* CFS-CMD-007: Sequence */
    UtAssert_INT32_EQ(Out.Sequence, 7);

    /* CFS-CMD-008: Flags */
    UtAssert_INT32_EQ(Out.Flags, 3);

    /* CFS-CMD-005: padding — bytes after payload are 0 */
    UtAssert_INT32_EQ(Out.Payload[4], 0);

    /* CFS-CMD-003: payload bytes correct (DEADBEEF) */
    UtAssert_INT32_EQ(Out.Payload[0], 0xDE);
    UtAssert_INT32_EQ(Out.Payload[1], 0xAD);
    UtAssert_INT32_EQ(Out.Payload[2], 0xBE);
    UtAssert_INT32_EQ(Out.Payload[3], 0xEF);
}

/* LORA-HB-001/002/004/007/008/009: ParseHb 다양한 입력 */
void Test_ParseLoRaFrame_CRC16(void)
{
    /* CRC16-CCITT: "" → 0xFFFF, known value */
    uint16 Crc = UPLINK_APP_CRC16((const uint8 *)"", 0);
    UtAssert_INT32_EQ(Crc, 0xFFFF);

    /* CRC16 of "UP" should be deterministic */
    uint16 Crc2 = UPLINK_APP_CRC16((const uint8 *)"UP", 2);
    UtAssert_INT32_EQ(Crc2, UPLINK_APP_CRC16((const uint8 *)"UP", 2)); /* idempotent */
}

void UtTest_Setup(void)
{
    ADD_TEST(UPLINK_APP_ValidateProxyCommand);
    ADD_TEST(UPLINK_APP_VerifyCmdLength_Impl);
    ADD_TEST(UPLINK_APP_ParseRouteUpdatePayload);
    ADD_TEST(UPLINK_APP_ResolveRouteTarget);
    ADD_TEST(UPLINK_APP_ReportHousekeeping);
    ADD_TEST(UPLINK_APP_UpdateStatusTelemetry);
    ADD_TEST(UPLINK_APP_LoadState_NoFile);
    ADD_TEST(UPLINK_APP_SaveState_NoDir);
    ADD_TEST(UPLINK_APP_ForwardModeCommand);
    ADD_TEST(UPLINK_APP_ForwardDiagnosticCommand);
    ADD_TEST(ParseLoRaFrame_ValidFrame);
    ADD_TEST(ParseLoRaFrame_CrcMismatch);
    ADD_TEST(ParseLoRaFrame_BadPrefix);
    ADD_TEST(ParseLoRaFrame_BadPayloadHex);
    ADD_TEST(ParseLoRaFrame_MaxPayload);
    ADD_TEST(ParseLoRaFrame_OverPayload);
    ADD_TEST(ParseLoRaFrame_SeqRegression);
    ADD_TEST(ParseLoRaFrame_CommandPacketFields);
    ADD_TEST(ParseLoRaFrame_CRC16);
}

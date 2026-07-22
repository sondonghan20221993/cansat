/************************************************************************
 * Coverage tests for uplink_app_utils.c
 ************************************************************************/

#include "uplink_app_coveragetest_common.h"
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

void Test_UPLINK_APP_ValidateProxyCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_Result_t           Result;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.Version       = UPLINK_APP_PROTOCOL_VERSION;
    Cmd.CommandClass  = UPLINK_APP_CLASS_ROUTE_UPDATE;
    Cmd.PayloadLength = 1;
    Cmd.Checksum      = UPLINK_APP_ComputeProxyCrc(&Cmd);

    UtAssert_BOOL_TRUE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_ACCEPT);

    /* bad checksum — all other fields valid */
    Cmd.Checksum ^= 0x5A5AU;
    UtAssert_BOOL_FALSE(UPLINK_APP_ValidateProxyCommand(&Cmd, &Result));
    UtAssert_INT32_EQ(Result, UPLINK_APP_RESULT_REJECT_CHECKSUM);
    Cmd.Checksum = UPLINK_APP_ComputeProxyCrc(&Cmd);

    /* bad version — short-circuits before CRC check */
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

    /* --- REPLACE: 기본 성공 케이스 --- */
    memset(&Cmd, 0, sizeof(Cmd));
    PayloadSrc = (UPLINK_APP_RouteUpdatePayload_t *)Cmd.Payload;
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_OP_REPLACE;
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
    UtAssert_INT32_EQ(Payload.RouteType, UPLINK_APP_ROUTE_OP_REPLACE);
    UtAssert_INT32_EQ(Payload.WaypointCount, 2);

    /* APPEND 타입도 동일 검증 경로 */
    PayloadSrc->RouteType = UPLINK_APP_ROUTE_OP_APPEND;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
    UtAssert_INT32_EQ(Payload.RouteType, UPLINK_APP_ROUTE_OP_APPEND);

    /* 잘못된 RouteType */
    PayloadSrc->RouteType = UPLINK_APP_ROUTE_OP_NONE;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
    PayloadSrc->RouteType = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* WaypointCount=0 거부 */
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_OP_REPLACE;
    PayloadSrc->WaypointCount = 0;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* WaypointCount > MAX 거부 */
    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* 좌표 검증: INF 거부 */
    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = INFINITY;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* 인접 waypoint 거리 부족 거부 */
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[1].X = 1.99f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* 최소 거리 정확히 충족 */
    PayloadSrc->Waypoints[1].X = 2.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 3.0f;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* 인접 거리 초과 거부 */
    PayloadSrc->Waypoints[1].X = 2.01f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* MAX waypoint 경계 성공 */
    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS;
    for (Index = 0; Index < PayloadSrc->WaypointCount; ++Index)
    {
        PayloadSrc->Waypoints[Index].X = (float)(Index * 2U);
        PayloadSrc->Waypoints[Index].Y = -10.0f;
        PayloadSrc->Waypoints[Index].Z = 3.0f;
    }
    Cmd.PayloadLength = (uint8)(4U + ((size_t)PayloadSrc->WaypointCount * sizeof(UPLINK_APP_Waypoint_t)));
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* No-fly zone 거부 */
    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength         = (uint8)(4U + (2U * sizeof(UPLINK_APP_Waypoint_t)));
    PayloadSrc->Waypoints[0].X = 0.0f;
    PayloadSrc->Waypoints[0].Y = -10.0f;
    PayloadSrc->Waypoints[0].Z = 3.0f;
    PayloadSrc->Waypoints[1].X = 100.0f;
    PayloadSrc->Waypoints[1].Y = -10.0f;
    PayloadSrc->Waypoints[1].Z = 4.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* --- DELETE: 정상 케이스 (waypoint 없는 payload) --- */
    memset(&Cmd, 0, sizeof(Cmd));
    PayloadSrc = (UPLINK_APP_RouteUpdatePayload_t *)Cmd.Payload;
    PayloadSrc->RouteType     = UPLINK_APP_ROUTE_OP_DELETE;
    PayloadSrc->WaypointCount = 2;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
    UtAssert_INT32_EQ(Payload.RouteType, UPLINK_APP_ROUTE_OP_DELETE);
    UtAssert_INT32_EQ(Payload.WaypointCount, 2);

    /* DELETE WaypointCount=0 거부 */
    PayloadSrc->WaypointCount = 0;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* DELETE WaypointCount > MAX 거부 */
    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* DELETE waypoint 데이터가 있는 경우 거부 (PayloadLength != 4) */
    PayloadSrc->WaypointCount = 1;
    Cmd.PayloadLength         = (uint8)(4U + sizeof(UPLINK_APP_Waypoint_t));
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));

    /* DELETE MAX 경계값 성공 */
    PayloadSrc->WaypointCount = UPLINK_APP_ROUTE_MAX_WAYPOINTS;
    Cmd.PayloadLength         = 4U;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseRouteUpdatePayload(&Cmd, &Payload));
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
    UPLINK_APP_Data.FcMissionResult             = 2;
    UPLINK_APP_Data.FcMissionUploadState        = 1;
    UPLINK_APP_Data.FcMissionUploadSuccessCount = 9;
    UPLINK_APP_Data.BootCount                   = 42;
    UPLINK_APP_Data.LastAcceptedSequence        = 1234;

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
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.FcMissionResult, 2);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.FcMissionUploadState, 1);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.FcMissionUploadSuccessCount, 9);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.BootCount, 42);
    UtAssert_INT32_EQ(UPLINK_APP_Data.StatusTlm.LastAcceptedSequence, 1234);
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

/* BL-17(2026-07-22) 커버리지 갭 해소: UPLINK_APP_STATE_FILE_PATH env var
 * 주입으로 truncated/bad-magic/checksum-mismatch 3개 분기를 /tmp의 실제
 * 파일로 실행 — 이전엔 /cf가 테스트 환경에 없어 ENOENT 경로만 탈 수 있었음. */
void Test_UPLINK_APP_LoadState_Truncated(void)
{
    const char *Path = "/tmp/uplink_app_ut_state_truncated.bin";
    int         Fd;
    uint8       Short[5] = {1, 2, 3, 4, 5};

    setenv("UPLINK_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Short, sizeof(Short));
    close(Fd);

    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);

    unlink(Path);
    unsetenv("UPLINK_APP_STATE_FILE_PATH");
}

void Test_UPLINK_APP_LoadState_BadMagic(void)
{
    const char *Path = "/tmp/uplink_app_ut_state_badmagic.bin";
    int         Fd;
    uint32      Garbage[4] = {0xDEADBEEFU, 1, 2, 3}; /* Magic 필드가 틀림 */

    setenv("UPLINK_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Garbage, sizeof(Garbage));
    close(Fd);

    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);

    unlink(Path);
    unsetenv("UPLINK_APP_STATE_FILE_PATH");
}

void Test_UPLINK_APP_LoadState_ChecksumMismatch(void)
{
    const char *Path = "/tmp/uplink_app_ut_state_badcrc.bin";
    int         Fd;
    /* Magic만 맞고 Checksum은 틀린 레코드(0x55504C4BU = "UPLK", uplink_app_utils.c 기준) */
    uint32      Rec[4] = {0x55504C4BU, 42U, 3U, 0U /* wrong checksum */};

    setenv("UPLINK_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Rec, sizeof(Rec));
    close(Fd);

    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);

    unlink(Path);
    unsetenv("UPLINK_APP_STATE_FILE_PATH");
}

void Test_UPLINK_APP_LoadState_OpenErrorNotEnoent(void)
{
    /* ENOENT가 아닌 open() 실패도 손상과 동일하게 취급 — 일반 파일을
     * 디렉터리처럼 취급해 그 "아래" 경로를 열게 하면 ENOTDIR로 실패 */
    const char *RegularFile = "/tmp/uplink_app_ut_not_a_dir.bin";
    const char *BogusPath   = "/tmp/uplink_app_ut_not_a_dir.bin/x";
    int         Fd;

    Fd = open(RegularFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    close(Fd);

    setenv("UPLINK_APP_STATE_FILE_PATH", BogusPath, 1);

    UPLINK_APP_Data.AcceptedCount        = 0;
    UPLINK_APP_Data.LastAcceptedSequence = 0;

    UPLINK_APP_LoadState();

    UtAssert_INT32_EQ(UPLINK_APP_Data.AcceptedCount, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 0);

    unlink(RegularFile);
    unsetenv("UPLINK_APP_STATE_FILE_PATH");
}

void Test_UPLINK_APP_SaveState_NoDir(void)
{
    /* /cf/ does not exist in test env — SaveState must return silently */
    UPLINK_APP_Data.LastAcceptedSequence = 99;
    UPLINK_APP_SaveState();
    /* No crash and state unchanged */
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastAcceptedSequence, 99);
}

void Test_UPLINK_APP_IncrementBootCount_FromZero(void)
{
    /* BL-12: 첫 부팅(복원값 없음, BootCount==0)에서 +1 */
    UPLINK_APP_Data.BootCount = 0;

    UPLINK_APP_IncrementBootCount();

    UtAssert_INT32_EQ((int)UPLINK_APP_Data.BootCount, 1);
}

void Test_UPLINK_APP_IncrementBootCount_WrapsAt256(void)
{
    /* BL-12: uint8 wrap — 255에서 +1하면 0으로 되돌아감 */
    UPLINK_APP_Data.BootCount = 255;

    UPLINK_APP_IncrementBootCount();

    UtAssert_INT32_EQ((int)UPLINK_APP_Data.BootCount, 0);
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

void Test_UPLINK_APP_ForwardCounterMgmtCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));

    /* payload too short */
    Cmd.PayloadLength = 5;
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* action != 0 rejected */
    Cmd.PayloadLength = 6;
    Cmd.Payload[0]     = UPLINK_APP_COUNTER_SCOPE_UPLINK;
    Cmd.Payload[1]     = 1;
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope out of range rejected */
    Cmd.Payload[0] = 5;
    Cmd.Payload[1] = 0;
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope=UPLINK: local reset, no SB traffic */
    UPLINK_APP_Data.CmdCounter = 7;
    UPLINK_APP_Data.ErrCounter = 3;
    Cmd.Payload[0] = UPLINK_APP_COUNTER_SCOPE_UPLINK;
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.CmdCounter, 0);
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, 0);

    /* scope=MAVLINK_BRIDGE: SetFcnCode fail */
    Cmd.Payload[0] = UPLINK_APP_COUNTER_SCOPE_MAVLINK_BRIDGE;
    UT_SetDefaultReturnValue(UT_KEY(CFE_MSG_SetFcnCode), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope=MAVLINK_BRIDGE: transmit fail */
    UT_SetDefaultReturnValue(UT_KEY(CFE_MSG_SetFcnCode), CFE_SUCCESS);
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope=MAVLINK_BRIDGE: success */
    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope=CFS_CORE: success */
    Cmd.Payload[0] = UPLINK_APP_COUNTER_SCOPE_CFS_CORE;
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));

    /* scope=LORA_TDM: success */
    Cmd.Payload[0] = UPLINK_APP_COUNTER_SCOPE_LORA_TDM;
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardCounterMgmtCommand(&Cmd));
}

void Test_UPLINK_APP_ParseViewpointPayload(void)
{
    UPLINK_APP_ProcessUplinkCmd_t  Cmd;
    UPLINK_APP_ViewpointPayload_t  Payload;
    UPLINK_APP_ViewpointPayload_t *Src;

    memset(&Cmd, 0, sizeof(Cmd));
    Src = (UPLINK_APP_ViewpointPayload_t *)Cmd.Payload;

    Cmd.PayloadLength       = (uint8)sizeof(UPLINK_APP_ViewpointPayload_t);
    Src->ViewpointType      = 0;
    Src->ViewpointVersion   = UPLINK_APP_VIEWPOINT_VERSION;
    Src->PositionFrame      = 0;
    Src->X                  = 0.0f;
    Src->Y                  = 0.0f;
    Src->Z                  = 4.0f;
    Src->Yaw                = 0.0f;
    Src->Pitch              = 0.0f;
    Src->HoldTimeMs         = 0;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));

    /* wrong payload size */
    Cmd.PayloadLength = (uint8)(sizeof(UPLINK_APP_ViewpointPayload_t) - 1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Cmd.PayloadLength = (uint8)sizeof(UPLINK_APP_ViewpointPayload_t);

    /* bad version */
    Src->ViewpointVersion = 99;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->ViewpointVersion = UPLINK_APP_VIEWPOINT_VERSION;

    /* bad type */
    Src->ViewpointType = UPLINK_APP_VIEWPOINT_MAX_TYPE + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->ViewpointType = 0;

    /* bad frame */
    Src->PositionFrame = UPLINK_APP_VIEWPOINT_MAX_FRAME + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->PositionFrame = 0;

    /* X out of range / non-finite */
    Src->X = UPLINK_APP_VIEWPOINT_MAX_X_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = UPLINK_APP_VIEWPOINT_MIN_X_M - 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = INFINITY;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->X = 0.0f;

    /* Y out of range */
    Src->Y = UPLINK_APP_VIEWPOINT_MAX_Y_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Y = 0.0f;

    /* Z out of range */
    Src->Z = UPLINK_APP_VIEWPOINT_MAX_ALT_M + 1.0f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Z = UPLINK_APP_VIEWPOINT_MIN_ALT_M - 0.5f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Z = 4.0f;

    /* Yaw out of range */
    Src->Yaw = UPLINK_APP_VIEWPOINT_MAX_YAW + 0.1f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Yaw = -(UPLINK_APP_VIEWPOINT_MAX_YAW + 0.1f);
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Yaw = 0.0f;

    /* Pitch out of range */
    Src->Pitch = UPLINK_APP_VIEWPOINT_MAX_PITCH + 0.1f;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->Pitch = 0.0f;

    /* HoldTimeMs too large */
    Src->HoldTimeMs = UPLINK_APP_VIEWPOINT_MAX_HOLD_MS + 1U;
    UtAssert_BOOL_FALSE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
    Src->HoldTimeMs = 0;

    /* boundary valid: max type and max hold */
    Src->ViewpointType = UPLINK_APP_VIEWPOINT_MAX_TYPE;
    Src->HoldTimeMs    = UPLINK_APP_VIEWPOINT_MAX_HOLD_MS;
    UtAssert_BOOL_TRUE(UPLINK_APP_ParseViewpointPayload(&Cmd, &Payload));
}

void Test_UPLINK_APP_ForwardViewpointCommand(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_ViewpointPayload_t Payload;

    memset(&Cmd, 0, sizeof(Cmd));
    memset(&Payload, 0, sizeof(Payload));
    Cmd.Sequence          = 70;
    Payload.ViewpointType = 0;
    Payload.PositionFrame = 0;
    Payload.X             = 1.0f;
    Payload.Y             = -5.0f;
    Payload.Z             = 3.0f;
    Payload.Yaw           = 0.0f;
    Payload.Pitch         = 0.0f;
    Payload.HoldTimeMs    = 1000;

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardViewpointCommand(&Cmd, &Payload));

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), -1);
    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardViewpointCommand(&Cmd, &Payload));
}

/* CONFIG_CMD_MID checksum 검증 (uplink_app_utils.c UPLINK_APP_ForwardConfigCommand) */

static void UT_BuildValidConfigCmd(UPLINK_APP_ProcessUplinkCmd_t *Cmd, uint32 Value)
{
    UPLINK_APP_ConfigPayloadHdr_t *Hdr;
    uint8                         *ValueBytes;

    memset(Cmd, 0, sizeof(*Cmd));
    Hdr        = (UPLINK_APP_ConfigPayloadHdr_t *)Cmd->Payload;
    ValueBytes = Cmd->Payload + sizeof(*Hdr);

    Hdr->ConfigScope   = 1;
    Hdr->ConfigVersion = 1;
    Hdr->ParameterId   = 0x1000;
    Hdr->ValueType     = 0; /* uint32 */
    Hdr->ValueLength   = (uint8)sizeof(uint32);
    memcpy(ValueBytes, &Value, sizeof(uint32));

    Hdr->Checksum = (uint16)(Hdr->ConfigScope + Hdr->ConfigVersion +
                              (Hdr->ParameterId & 0xFFU) + ((Hdr->ParameterId >> 8U) & 0xFFU) +
                              Hdr->ValueType + Hdr->ValueLength +
                              ValueBytes[0] + ValueBytes[1] + ValueBytes[2] + ValueBytes[3]);

    Cmd->PayloadLength = (uint8)(sizeof(*Hdr) + Hdr->ValueLength);
}

void Test_UPLINK_APP_ForwardConfigCommand_ChecksumValid(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    UT_BuildValidConfigCmd(&Cmd, 500U);

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_SUCCESS);
    UtAssert_BOOL_TRUE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ConfigPendingState, UPLINK_APP_CONFIG_IDLE);
}

void Test_UPLINK_APP_ForwardConfigCommand_ChecksumInvalid(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_ConfigPayloadHdr_t *Hdr;
    uint32                         ErrBefore;

    UT_BuildValidConfigCmd(&Cmd, 500U);
    Hdr           = (UPLINK_APP_ConfigPayloadHdr_t *)Cmd.Payload;
    Hdr->Checksum ^= 0x00FFU; /* 변조 */
    ErrBefore = UPLINK_APP_Data.ErrCounter;

    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, ErrBefore + 1U);
}

void Test_UPLINK_APP_ForwardConfigCommand_PayloadTooShort(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    uint32                         ErrBefore;

    UT_BuildValidConfigCmd(&Cmd, 500U);
    Cmd.PayloadLength = (uint8)(sizeof(UPLINK_APP_ConfigPayloadHdr_t) - 1U);
    ErrBefore = UPLINK_APP_Data.ErrCounter;

    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, ErrBefore + 1U);
}

void Test_UPLINK_APP_ForwardConfigCommand_InvalidValueLength(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_ConfigPayloadHdr_t *Hdr;
    uint32                         ErrBefore;

    UT_BuildValidConfigCmd(&Cmd, 500U);
    Hdr              = (UPLINK_APP_ConfigPayloadHdr_t *)Cmd.Payload;
    Hdr->ValueLength = 2; /* uint32(4)가 아닌 값 */
    ErrBefore = UPLINK_APP_Data.ErrCounter;

    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, ErrBefore + 1U);
}

void Test_UPLINK_APP_ForwardConfigCommand_PayloadOverflow(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;
    UPLINK_APP_ConfigPayloadHdr_t *Hdr;
    uint32                         ErrBefore;

    UT_BuildValidConfigCmd(&Cmd, 500U);
    Hdr = (UPLINK_APP_ConfigPayloadHdr_t *)Cmd.Payload;
    /* PayloadLength가 (Hdr+ValueLength) 미만이 되도록 축소 */
    Cmd.PayloadLength = (uint8)(sizeof(*Hdr) + Hdr->ValueLength - 1U);
    ErrBefore = UPLINK_APP_Data.ErrCounter;

    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ErrCounter, ErrBefore + 1U);
}

void Test_UPLINK_APP_ForwardConfigCommand_TransmitFail(void)
{
    UPLINK_APP_ProcessUplinkCmd_t Cmd;

    UT_BuildValidConfigCmd(&Cmd, 500U);

    UT_SetDefaultReturnValue(UT_KEY(CFE_SB_TransmitMsg), CFE_STATUS_EXTERNAL_RESOURCE_FAIL);
    UPLINK_APP_Data.ConfigPendingState = UPLINK_APP_CONFIG_IDLE;
    UPLINK_APP_Data.LastConfigResult   = 0;

    UtAssert_BOOL_FALSE(UPLINK_APP_ForwardConfigCommand(&Cmd));
    UtAssert_INT32_EQ(UPLINK_APP_Data.ConfigPendingState, UPLINK_APP_CONFIG_REJECTED);
    UtAssert_INT32_EQ(UPLINK_APP_Data.LastConfigResult, 1);
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
    ADD_TEST(UPLINK_APP_LoadState_Truncated);
    ADD_TEST(UPLINK_APP_LoadState_BadMagic);
    ADD_TEST(UPLINK_APP_LoadState_ChecksumMismatch);
    ADD_TEST(UPLINK_APP_LoadState_OpenErrorNotEnoent);
    ADD_TEST(UPLINK_APP_SaveState_NoDir);
    ADD_TEST(UPLINK_APP_IncrementBootCount_FromZero);
    ADD_TEST(UPLINK_APP_IncrementBootCount_WrapsAt256);
    ADD_TEST(UPLINK_APP_ForwardModeCommand);
    ADD_TEST(UPLINK_APP_ForwardDiagnosticCommand);
    ADD_TEST(UPLINK_APP_ForwardCounterMgmtCommand);
    ADD_TEST(UPLINK_APP_ParseViewpointPayload);
    ADD_TEST(UPLINK_APP_ForwardViewpointCommand);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_ChecksumValid);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_ChecksumInvalid);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_PayloadTooShort);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_InvalidValueLength);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_PayloadOverflow);
    ADD_TEST(UPLINK_APP_ForwardConfigCommand_TransmitFail);
}

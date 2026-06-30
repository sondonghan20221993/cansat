#include "mavlink_bridge_app_coveragetest_common.h"

/* -----------------------------------------------------------------------
 * UpdateFromHeartbeat 테스트
 * ----------------------------------------------------------------------- */

/* 정상: ARMED bit 설정 (base_mode bit7 = 1) */
void Test_UpdateFromHeartbeat_Armed(void)
{
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x80, 4);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed,        1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcBaseMode,     0x80);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcSystemStatus, 4);
}

/* 정상: DISARMED (base_mode bit7 = 0) */
void Test_UpdateFromHeartbeat_Disarmed(void)
{
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x00, 3);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed,        0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcBaseMode,     0x00);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcSystemStatus, 3);
}

/* base_mode의 다른 비트가 켜져도 bit7만 ARMED 판단 */
void Test_UpdateFromHeartbeat_OtherBitsIgnored(void)
{
    /* 0x7F = 모든 비트 세트 중 bit7만 0 → DISARMED */
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x7F, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed, 0);

    /* 0xFF = bit7 포함 → ARMED */
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0xFF, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed, 1);
}

/* ARMED → DISARMED 상태 전이 */
void Test_UpdateFromHeartbeat_StateTransition(void)
{
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x80, 4);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed, 1);

    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x00, 3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.IsArmed, 0);
}

/* system_status 값이 그대로 저장되는지 */
void Test_UpdateFromHeartbeat_SystemStatus(void)
{
    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x00, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcSystemStatus, 0); /* MAV_STATE_UNINIT */

    MAVLINK_BRIDGE_APP_UpdateFromHeartbeat(0x80, 4);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcSystemStatus, 4); /* MAV_STATE_ACTIVE */
}

/* -----------------------------------------------------------------------
 * StartMissionUpload ARMED 차단 테스트
 * ----------------------------------------------------------------------- */

/* ARMED 상태에서 mission upload → ARMED_WARN_EID 발생 후 차단 */
void Test_StartMissionUpload_BlockedWhenArmed(void)
{
    UT_CheckEvent_t Evt;
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.WaypointCount = 2;
    Msg.Waypoints[0].X = 1.0f; Msg.Waypoints[0].Y = 2.0f; Msg.Waypoints[0].Z = 3.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 1;
    MAVLINK_BRIDGE_APP_Data.FcBaseMode = 0x80;

    UT_CHECKEVENT_SETUP(&Evt, MAVLINK_BRIDGE_APP_ARMED_WARN_EID, NULL);
    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    /* upload 차단 — MissionUploadState는 변경되지 않아야 함 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
}

/* DISARMED 상태에서 mission upload → 정상 진행 (CLEARING 상태로 전이) */
void Test_StartMissionUpload_AllowedWhenDisarmed(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.WaypointCount = 2;
    Msg.Waypoints[0].X = 1.0f; Msg.Waypoints[0].Y = 2.0f; Msg.Waypoints[0].Z = 3.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 0;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_CLEARING);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
}

/* FC 링크 미연결 + ARMED → 링크 오류가 먼저 처리됨 */
void Test_StartMissionUpload_LinkNotConnectedBeforeArmedCheck(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;
    UT_CheckEvent_t Evt;

    memset(&Msg, 0, sizeof(Msg));
    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 1;

    UT_CHECKEVENT_SETUP(&Evt, MAVLINK_BRIDGE_APP_ARMED_WARN_EID, NULL);
    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    /* ARMED_WARN이 아니라 link not connected 오류여야 함 */
    UtAssert_INT32_EQ(Evt.MatchCount, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
}

/* -----------------------------------------------------------------------
 * StartMissionUpload REPLACE / APPEND / DELETE 동작 테스트
 * ----------------------------------------------------------------------- */

/* REPLACE: pending에 새 waypoint 복사 */
void Test_StartMissionUpload_Replace(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 1U; /* ROUTE_OP_REPLACE */
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].X = 10.0f; Msg.Waypoints[0].Y = 20.0f; Msg.Waypoints[0].Z = 5.0f;
    Msg.Waypoints[1].X = 30.0f; Msg.Waypoints[1].Y = 40.0f; Msg.Waypoints[1].Z = 5.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 3; /* active 무시하고 새 것으로 교체 */

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_CLEARING);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[0] == 10.0f, "Pending[0].X == 10");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[1] == 30.0f, "Pending[1].X == 30");
}

/* APPEND: active + 새 waypoint 결합 */
void Test_StartMissionUpload_Append(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 2U; /* ROUTE_OP_APPEND */
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].X = 50.0f; Msg.Waypoints[0].Y = 0.0f; Msg.Waypoints[0].Z = 5.0f;
    Msg.Waypoints[1].X = 60.0f; Msg.Waypoints[1].Y = 0.0f; Msg.Waypoints[1].Z = 5.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 1;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[0]  = 10.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointY[0]  = 0.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointZ[0]  = 5.0f;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 3);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[0] == 10.0f, "Pending[0]=Active[0]");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[1] == 50.0f, "Pending[1]=new[0]");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[2] == 60.0f, "Pending[2]=new[1]");
}

/* APPEND: active + 새 waypoint 합산이 MAX 초과 → 잘라냄 */
void Test_StartMissionUpload_AppendTruncated(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;
    uint8 i;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType     = 2U; /* ROUTE_OP_APPEND */
    Msg.WaypointCount = 4;
    for (i = 0; i < 4; i++) { Msg.Waypoints[i].X = (float)(i + 100); }

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = (uint8)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS - 1;
    for (i = 0; i < MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount; i++)
    {
        MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[i] = (float)i;
    }

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    /* max에서 잘려야 함 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount,
                      (int32)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS);
}

/* DELETE: active 뒤에서 N개 제거 */
void Test_StartMissionUpload_Delete(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 3U; /* ROUTE_OP_DELETE */
    Msg.WaypointCount  = 2;  /* 뒤에서 2개 삭제 */

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 4;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[0]  = 1.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[1]  = 2.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[2]  = 3.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointX[3]  = 4.0f;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[0] == 1.0f, "Pending[0]=Active[0]");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingX[1] == 2.0f, "Pending[1]=Active[1]");
}

/* DELETE: 삭제 수 >= active → WpCount=0 (CLEAR_ALL만) */
void Test_StartMissionUpload_DeleteAll(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType     = 3U; /* ROUTE_OP_DELETE */
    Msg.WaypointCount = 5;  /* active(3)보다 많음 */

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 3;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 0);
}

/* MISSION_ACK 성공 → Active 캐시 갱신 확인은 ParseMavlinkFrame 레벨에서
 * 이뤄지므로 여기서는 StartMissionUpload가 Pending을 올바르게 설정하는지만 검증.
 * (통합 검증은 Pi 실기 테스트에서 수행) */

/* -----------------------------------------------------------------------
 * MissionQuery 테스트
 * ----------------------------------------------------------------------- */

/* FC 링크 미연결 상태에서 MissionQuery → 오류 이벤트 + ErrCounter 증가 */
void Test_MissionQuery_LinkNotConnected(void)
{
    MAVLINK_BRIDGE_APP_MissionQueryCmd_t Cmd;
    UT_CheckEvent_t                      Evt;
    size_t                               MsgSize;

    memset(&Cmd, 0, sizeof(Cmd));
    MsgSize = sizeof(Cmd);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);

    MAVLINK_BRIDGE_APP_Data.LinkState  = (uint8)MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;

    UT_CHECKEVENT_SETUP(&Evt, MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_ERR_EID, NULL);
    MAVLINK_BRIDGE_APP_MissionQuery(&Cmd);

    UtAssert_INT32_EQ(Evt.MatchCount, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
}

/* FC 링크 연결 상태에서 MissionQuery → download 시작, CmdCounter 증가 */
void Test_MissionQuery_Connected(void)
{
    MAVLINK_BRIDGE_APP_MissionQueryCmd_t Cmd;
    size_t                               MsgSize;

    memset(&Cmd, 0, sizeof(Cmd));
    MsgSize = sizeof(Cmd);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);

    MAVLINK_BRIDGE_APP_Data.LinkState  = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.CmdCounter = 0;

    MAVLINK_BRIDGE_APP_MissionQuery(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 1);
}

/* VerifyCmdLength 실패 시 MissionQuery → 즉시 반환 (download 시작 안 됨) */
void Test_MissionQuery_LengthCheckFail(void)
{
    MAVLINK_BRIDGE_APP_MissionQueryCmd_t Cmd;
    size_t                               WrongSize;

    memset(&Cmd, 0, sizeof(Cmd));
    WrongSize = sizeof(Cmd) + 1U;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &WrongSize, sizeof(WrongSize), false);

    MAVLINK_BRIDGE_APP_Data.LinkState  = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.CmdCounter = 0;

    MAVLINK_BRIDGE_APP_MissionQuery(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 0);
}

/* -----------------------------------------------------------------------
 * ReportHousekeeping — HK 필드 복사 확인
 * ----------------------------------------------------------------------- */
void Test_ReportHousekeeping(void)
{
    MAVLINK_BRIDGE_APP_Data.CmdCounter              = 7;
    MAVLINK_BRIDGE_APP_Data.ErrCounter              = 3;
    MAVLINK_BRIDGE_APP_Data.LinkState               = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.LastErrorCode           = 2;
    MAVLINK_BRIDGE_APP_Data.BytesReceived           = 1024;
    MAVLINK_BRIDGE_APP_Data.ReconnectAttemptCount   = 1;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount         = 5;
    MAVLINK_BRIDGE_APP_Data.MissionUploadSuccessCount = 4;
    MAVLINK_BRIDGE_APP_Data.MissionUploadFailCount    = 1;
    MAVLINK_BRIDGE_APP_Data.LastUploadResult          = (uint8)MAVLINK_BRIDGE_UPLOAD_RESULT_SUCCESS;

    MAVLINK_BRIDGE_APP_ReportHousekeeping();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.CommandCounter,          7);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.CommandErrorCounter,     3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.LinkState,
                      (int32)MAVLINK_BRIDGE_LINK_CONNECTED);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.ParseErrorCount,         5);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.MissionUploadSuccessCount, 4);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.MissionUploadFailCount,    1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.HkTlm.LastUploadResult,
                      (int32)MAVLINK_BRIDGE_UPLOAD_RESULT_SUCCESS);
}

/* -----------------------------------------------------------------------
 * VerifyCmdLength — 실제 구현 테스트
 * ----------------------------------------------------------------------- */
void Test_VerifyCmdLength_Pass(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t Cmd;
    size_t                       MsgSize;

    memset(&Cmd, 0, sizeof(Cmd));
    MsgSize = sizeof(Cmd);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &MsgSize, sizeof(MsgSize), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    UtAssert_BOOL_TRUE(MAVLINK_BRIDGE_APP_VerifyCmdLength(CFE_MSG_PTR(Cmd.CommandHeader), sizeof(Cmd)));
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
}

void Test_VerifyCmdLength_Fail(void)
{
    MAVLINK_BRIDGE_APP_NoopCmd_t Cmd;
    size_t                       WrongSize;

    memset(&Cmd, 0, sizeof(Cmd));
    WrongSize = sizeof(Cmd) + 1U;
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetSize), &WrongSize, sizeof(WrongSize), false);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    UtAssert_BOOL_FALSE(MAVLINK_BRIDGE_APP_VerifyCmdLength(CFE_MSG_PTR(Cmd.CommandHeader), sizeof(Cmd)));
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
}

/* -----------------------------------------------------------------------
 * SetLinkState — LinkState 필드 갱신
 * ----------------------------------------------------------------------- */
void Test_SetLinkState_Connected(void)
{
    MAVLINK_BRIDGE_APP_Data.LinkState = (uint8)MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LinkState, (int32)MAVLINK_BRIDGE_LINK_CONNECTED);
}

void Test_SetLinkState_Disconnected(void)
{
    MAVLINK_BRIDGE_APP_Data.LinkState = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_DISCONNECTED);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LinkState, (int32)MAVLINK_BRIDGE_LINK_DISCONNECTED);
}

/* -----------------------------------------------------------------------
 * RecordParseError — 카운터 증가 + EVS 이벤트
 * ----------------------------------------------------------------------- */
void Test_RecordParseError(void)
{
    UT_CheckEvent_t Evt;

    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 0;
    MAVLINK_BRIDGE_APP_Data.LastErrorCode   = 0;

    UT_CHECKEVENT_SETUP(&Evt, MAVLINK_BRIDGE_APP_PARSE_EID, NULL);
    MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastErrorCode,
                      (int32)MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
    UtAssert_INT32_EQ(Evt.MatchCount, 1);
}

/* RecordParseError 반복 호출 시 카운터 누적 */
void Test_RecordParseError_Cumulative(void)
{
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount = 0;

    MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
    MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
    MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_GPS_NO_FIX);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount,  3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastErrorCode,
                      (int32)MAVLINK_BRIDGE_ERROR_GPS_NO_FIX);
}

/* -----------------------------------------------------------------------
 * ProcessConfigCommand — 이중버퍼 흐름 검증
 * ----------------------------------------------------------------------- */

static uint16 calc_mav_checksum(uint8 scope, uint8 version, uint16 param_id,
                                 uint8 vtype, uint8 vlen, const uint8 *vbytes)
{
    uint16 sum = 0;
    uint8  i;
    sum += scope; sum += version;
    sum += (uint16)(param_id & 0xFFU);
    sum += (uint16)((param_id >> 8U) & 0xFFU);
    sum += vtype; sum += vlen;
    for (i = 0; i < vlen; i++) { sum += vbytes[i]; }
    return sum;
}

static void build_mav_config_msg(MAVLINK_BRIDGE_APP_ConfigCmdTlm_t *Msg,
                                  uint8 scope, uint8 version,
                                  uint16 param_id, uint32 value)
{
    MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t *Hdr;
    uint8 vbytes[4];
    memset(Msg, 0, sizeof(*Msg));
    Hdr = (MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t *)Msg->Payload;
    Hdr->ConfigScope   = scope;
    Hdr->ConfigVersion = version;
    Hdr->ParameterId   = param_id;
    Hdr->ValueType     = 0;
    Hdr->ValueLength   = (uint8)sizeof(uint32);
    memcpy(Msg->Payload + sizeof(*Hdr), &value, sizeof(value));
    memcpy(vbytes, &value, sizeof(value));
    Hdr->Checksum = calc_mav_checksum(scope, version, param_id, 0,
                                      (uint8)sizeof(uint32), vbytes);
    Msg->PayloadLength = (uint8)(sizeof(*Hdr) + sizeof(uint32));
}

/* 정상 활성화: PendingConfig에 기록 → 검증 → ActiveConfig로 교체
   PreviousConfig에 이전 값 백업, ConfigGeneration 증가 확인 */
void Test_ProcessConfig_DualBuffer_Activate(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    uint32 OldVal = MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs;

    /* ActiveConfig 초기화 */
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 200000U;
    MAVLINK_BRIDGE_APP_Data.ConfigGeneration = 0;

    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US, 100000U);

    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    /* ActiveConfig에 새 값 반영 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 100000);
    /* PreviousConfig에 이전 값 백업 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.PreviousConfig.AttitudeIntervalUs, (int32)OldVal);
    /* 상태 검증 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ConfigPendingState,
                      (int32)MAVLINK_BRIDGE_CONFIG_PENDING_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_OK);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ConfigGeneration, 1);
    /* 활성화 부작용: StreamRequestPending 세팅 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.StreamRequestPending, 1);
}

/* 거부 시 ActiveConfig 불변 확인 — pending 버퍼에만 기록되고 버려짐 */
void Test_ProcessConfig_DualBuffer_Rejected_ActiveUnchanged(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    uint32 OldVal = MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs;

    /* 범위 초과 값 */
    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US,
                         MAVLINK_BRIDGE_APP_PARAM_INTERVAL_MAX_US + 1U);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    /* ActiveConfig 불변 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, (int32)OldVal);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ConfigPendingState,
                      (int32)MAVLINK_BRIDGE_CONFIG_PENDING_REJECTED);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_BAD_VALUE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
    /* StreamRequestPending은 세팅되지 않음 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.StreamRequestPending, 0);
}

/* 잘못된 checksum → REJECTED, ActiveConfig 불변 */
void Test_ProcessConfig_BadChecksum(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t      Msg;
    MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t *Hdr;
    uint32 OldVal = MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs;

    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US, 100000U);

    /* checksum 임의 변조 */
    Hdr = (MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t *)Msg.Payload;
    Hdr->Checksum ^= 0xBEEFU;

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_BAD_CHECKSUM);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ConfigPendingState,
                      (int32)MAVLINK_BRIDGE_CONFIG_PENDING_REJECTED);
    /* ActiveConfig 불변 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, (int32)OldVal);
}

/* 다른 scope(cfs_core_app용) → 조용히 무시, 아무 상태 변화 없음 */
void Test_ProcessConfig_WrongScope_Ignored(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    uint32 OldVal = MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs;

    build_mav_config_msg(&Msg, 1U /* cfs_core_app scope */,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US, 100000U);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, (int32)OldVal);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
}

/* 잘못된 version → REJECTED */
void Test_ProcessConfig_BadVersion(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE, 0xFF,
                         MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US, 100000U);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_BAD_VERSION);
}

/* 알 수 없는 param_id → REJECTED */
void Test_ProcessConfig_BadParam(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         0xFFFF, 100000U);

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_BAD_PARAM);
}

/* ms 단위 파라미터 정상 활성화 (ReconnectIntervalMs) */
void Test_ProcessConfig_ReconnectInterval(void)
{
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t Msg;
    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE,
                         MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_RECONNECT_INTERVAL_MS, 2000U);

    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs, 2000);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult,
                      (int32)MAVLINK_BRIDGE_CONFIG_RESULT_OK);
}

void UtTest_Setup(void)
{
    ADD_TEST(UpdateFromHeartbeat_Armed);
    ADD_TEST(UpdateFromHeartbeat_Disarmed);
    ADD_TEST(UpdateFromHeartbeat_OtherBitsIgnored);
    ADD_TEST(UpdateFromHeartbeat_StateTransition);
    ADD_TEST(UpdateFromHeartbeat_SystemStatus);
    ADD_TEST(StartMissionUpload_BlockedWhenArmed);
    ADD_TEST(StartMissionUpload_AllowedWhenDisarmed);
    ADD_TEST(StartMissionUpload_LinkNotConnectedBeforeArmedCheck);
    ADD_TEST(StartMissionUpload_Replace);
    ADD_TEST(StartMissionUpload_Append);
    ADD_TEST(StartMissionUpload_AppendTruncated);
    ADD_TEST(StartMissionUpload_Delete);
    ADD_TEST(StartMissionUpload_DeleteAll);
    ADD_TEST(MissionQuery_LinkNotConnected);
    ADD_TEST(MissionQuery_Connected);
    ADD_TEST(MissionQuery_LengthCheckFail);
    ADD_TEST(ProcessConfig_DualBuffer_Activate);
    ADD_TEST(ProcessConfig_BadChecksum);
    ADD_TEST(ProcessConfig_DualBuffer_Rejected_ActiveUnchanged);
    ADD_TEST(ProcessConfig_WrongScope_Ignored);
    ADD_TEST(ProcessConfig_BadVersion);
    ADD_TEST(ProcessConfig_BadParam);
    ADD_TEST(ProcessConfig_ReconnectInterval);
    ADD_TEST(ReportHousekeeping);
    ADD_TEST(VerifyCmdLength_Pass);
    ADD_TEST(VerifyCmdLength_Fail);
    ADD_TEST(SetLinkState_Connected);
    ADD_TEST(SetLinkState_Disconnected);
    ADD_TEST(RecordParseError);
    ADD_TEST(RecordParseError_Cumulative);
}

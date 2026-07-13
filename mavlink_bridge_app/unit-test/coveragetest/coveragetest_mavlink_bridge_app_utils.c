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

/* -----------------------------------------------------------------------
 * SYSTEM_TIME (SYS_TIME) 수신 테스트
 *
 * static 파서에 직접 접근할 수 없으므로 non-blocking pipe를 SerialFd로
 * 물려 ServiceSerial() 경유로 프레임 바이트를 주입한다.
 * ----------------------------------------------------------------------- */

#include <unistd.h>
#include <fcntl.h>

#define UT_SYS_TIME_MSGID     2U
#define UT_SYS_TIME_CRC_EXTRA 137U

static void UT_MavCrcAccumulate(uint8 Data, uint16 *Crc)
{
    uint8 Tmp = Data ^ (uint8)(*Crc & 0xFFU);
    Tmp ^= (uint8)(Tmp << 4);
    *Crc = (uint16)((*Crc >> 8) ^ ((uint16)Tmp << 8) ^ ((uint16)Tmp << 3) ^ ((uint16)Tmp >> 4));
}

/* MAVLink v2 SYS_TIME 프레임 생성. CRC 바이트가 STX(0xFD/0xFE)와 겹치면
 * 파서가 리셋되므로 seq를 바꿔가며 안전한 프레임을 찾는다. */
static size_t UT_BuildSysTimeFrame(uint8 *Frame, const uint8 *Payload, uint8 PayloadLen, bool CorruptCrc)
{
    uint16 Crc;
    uint8  Seq;
    uint8  i;

    for (Seq = 0;; Seq++)
    {
        Crc = 0xFFFFU;
        UT_MavCrcAccumulate(PayloadLen, &Crc);
        UT_MavCrcAccumulate(0, &Crc); /* incompat */
        UT_MavCrcAccumulate(0, &Crc); /* compat */
        UT_MavCrcAccumulate(Seq, &Crc);
        UT_MavCrcAccumulate(1, &Crc);  /* sysid */
        UT_MavCrcAccumulate(1, &Crc);  /* compid */
        UT_MavCrcAccumulate(UT_SYS_TIME_MSGID, &Crc);
        UT_MavCrcAccumulate(0, &Crc);  /* msgid mid */
        UT_MavCrcAccumulate(0, &Crc);  /* msgid high */
        for (i = 0; i < PayloadLen; i++)
        {
            UT_MavCrcAccumulate(Payload[i], &Crc);
        }
        UT_MavCrcAccumulate(UT_SYS_TIME_CRC_EXTRA, &Crc);

        if (CorruptCrc)
        {
            Crc ^= 0x0101U; /* 양쪽 바이트를 깨뜨리되 STX와 겹치지 않게 아래에서 재검사 */
        }

        if ((Crc & 0xFFU) != 0xFDU && (Crc & 0xFFU) != 0xFEU &&
            (Crc >> 8)    != 0xFDU && (Crc >> 8)    != 0xFEU)
        {
            break;
        }
    }

    Frame[0] = 0xFD; /* STX v2 */
    Frame[1] = PayloadLen;
    Frame[2] = 0; /* incompat */
    Frame[3] = 0; /* compat */
    Frame[4] = Seq;
    Frame[5] = 1; /* sysid */
    Frame[6] = 1; /* compid */
    Frame[7] = UT_SYS_TIME_MSGID;
    Frame[8] = 0;
    Frame[9] = 0;
    memcpy(&Frame[10], Payload, PayloadLen);
    Frame[10 + PayloadLen]     = (uint8)(Crc & 0xFFU);
    Frame[10 + PayloadLen + 1] = (uint8)(Crc >> 8);

    return (size_t)(10 + PayloadLen + 2);
}

/* 프레임 바이트를 non-blocking pipe로 주입하고 ServiceSerial()로 소비 */
static void UT_FeedSerial(const uint8 *Bytes, size_t Length)
{
    int Fds[2];

    UtAssert_INT32_EQ(pipe(Fds), 0);
    UtAssert_INT32_EQ(fcntl(Fds[0], F_SETFL, O_NONBLOCK), 0);
    UtAssert_True(write(Fds[1], Bytes, Length) == (ssize_t)Length, "pipe write");

    MAVLINK_BRIDGE_APP_Data.SerialFd = Fds[0];
    /* NowMs=0(스텁)에서 heartbeat/stream request 부수 동작이 없도록 */
    MAVLINK_BRIDGE_APP_Data.TargetSystemId       = 0;
    MAVLINK_BRIDGE_APP_Data.StreamRequestPending = 0;

    MAVLINK_BRIDGE_APP_ServiceSerial();

    close(Fds[0]);
    close(Fds[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;
}

/* 정상: 12바이트 전체 페이로드, unix_usec != 0 → 필드 갱신 */
void Test_SysTime_FullPayload(void)
{
    uint8  Payload[12] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, /* unix_usec */
                          0x10, 0x20, 0x30, 0x01};                        /* boot_ms */
    uint8  Frame[32];
    size_t Len;

    Len = UT_BuildSysTimeFrame(Frame, Payload, sizeof(Payload), false);
    UT_FeedSerial(Frame, Len);

    UtAssert_True(MAVLINK_BRIDGE_APP_Data.LastSysTimeUnixUsec == 0x0102030405060708ULL,
                  "LastSysTimeUnixUsec == 0x0102030405060708");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* MAVLink v2 zero-trimming: 8바이트로 잘린 페이로드 → zero-extend 후 정상 갱신 */
void Test_SysTime_TrimmedPayload(void)
{
    uint8  Payload[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    uint8  Frame[32];
    size_t Len;

    Len = UT_BuildSysTimeFrame(Frame, Payload, sizeof(Payload), false);
    UT_FeedSerial(Frame, Len);

    UtAssert_True(MAVLINK_BRIDGE_APP_Data.LastSysTimeUnixUsec == 0x0102030405060708ULL,
                  "trimmed frame decoded via zero-extend");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* unix_usec == 0 (GPS 시각 없음) → 무시, 필드 미갱신 */
void Test_SysTime_ZeroClockIgnored(void)
{
    /* unix_usec=0, boot_ms=5 → trailing-zero trim 시 9바이트 */
    uint8  Payload[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0x05};
    uint8  Frame[32];
    size_t Len;

    Len = UT_BuildSysTimeFrame(Frame, Payload, sizeof(Payload), false);
    UT_FeedSerial(Frame, Len);

    UtAssert_True(MAVLINK_BRIDGE_APP_Data.LastSysTimeUnixUsec == 0ULL,
                  "zero unix time ignored");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastSysTimeRxMs, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* CRC 불일치 → parse error 기록, 필드 미갱신 */
void Test_SysTime_CrcFail(void)
{
    uint8  Payload[12] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
                          0x10, 0x20, 0x30, 0x01};
    uint8  Frame[32];
    size_t Len;

    Len = UT_BuildSysTimeFrame(Frame, Payload, sizeof(Payload), true);
    UT_FeedSerial(Frame, Len);

    UtAssert_True(MAVLINK_BRIDGE_APP_Data.LastSysTimeUnixUsec == 0ULL,
                  "crc fail leaves fields untouched");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.ParseErrorCount >= 1, "parse error recorded");
}

/* -----------------------------------------------------------------------
 * SendMissionItemInt — GLOBAL_RELATIVE_ALT frame 변환 검증 (2026-07-13)
 * [[mission_item_int_frame_gap]]
 *
 * RefLatE7/RefLonE7는 static이라 직접 접근 불가 — GLOBAL_POSITION_INT를
 * 먼저 주입해 실제 파서 경로로 설정한다. SendMissionItemInt의 출력(write)은
 * 같은 SerialFd로 나가므로, 단방향 pipe(UT_FeedSerial)가 아니라 socketpair로
 * 주입 fd와 캡처 fd를 겸용한다. ServiceSerial()이 매 호출 처음에 무조건
 * companion heartbeat도 함께 쓰므로, 캡처된 바이트에서 msgid로 원하는 프레임만
 * 스캔해서 찾는다.
 * ----------------------------------------------------------------------- */

#include <math.h>
#include <sys/socket.h>

/* Read/WriteXxxLE는 production 쪽도 static이라 여기서 로컬 재구현한다. */
static uint32 UT_ReadU32LE(const uint8 *Data)
{
    return ((uint32)Data[0]) | ((uint32)Data[1] << 8) | ((uint32)Data[2] << 16) | ((uint32)Data[3] << 24);
}

static uint16 UT_ReadU16LE(const uint8 *Data)
{
    return (uint16)(((uint16)Data[0]) | ((uint16)Data[1] << 8));
}

static float UT_ReadFloatLE(const uint8 *Data)
{
    uint32 RawValue = UT_ReadU32LE(Data);
    float  Value;

    memcpy(&Value, &RawValue, sizeof(Value));
    return Value;
}

static void UT_WriteU16LE(uint8 *Data, uint16 Value)
{
    Data[0] = (uint8)(Value & 0xFFU);
    Data[1] = (uint8)((Value >> 8) & 0xFFU);
}

static void UT_WriteU32LE(uint8 *Data, uint32 Value)
{
    Data[0] = (uint8)(Value & 0xFFU);
    Data[1] = (uint8)((Value >> 8) & 0xFFU);
    Data[2] = (uint8)((Value >> 16) & 0xFFU);
    Data[3] = (uint8)((Value >> 24) & 0xFFU);
}

#define UT_GLOBAL_POSITION_INT_MSGID     33U
#define UT_GLOBAL_POSITION_INT_CRC_EXTRA 104U
#define UT_GLOBAL_POSITION_INT_PAYLOAD_LEN 28U
#define UT_MISSION_REQUEST_INT_MSGID     51U
#define UT_MISSION_REQUEST_INT_CRC_EXTRA 196U
#define UT_MISSION_ITEM_INT_MSGID        73U

#define UT_EARTH_RADIUS_M 6371000.0f
#define UT_DEG_TO_RAD      0.01745329251994f
#define UT_RAD_TO_DEG      57.29577951308f

/* 임의 msgid/crc_extra/payload로 MAVLink v2 프레임을 만든다 (msgid는 1바이트 범위만 지원 — 이 테스트에서 쓰는 메시지는 모두 255 이하). */
static size_t UT_BuildMavFrameGeneric(uint8 *Frame, uint8 MsgId, uint8 CrcExtra, const uint8 *Payload, uint8 PayloadLen)
{
    uint16 Crc;
    uint8  Seq;
    uint8  i;

    for (Seq = 0;; Seq++)
    {
        Crc = 0xFFFFU;
        UT_MavCrcAccumulate(PayloadLen, &Crc);
        UT_MavCrcAccumulate(0, &Crc);
        UT_MavCrcAccumulate(0, &Crc);
        UT_MavCrcAccumulate(Seq, &Crc);
        UT_MavCrcAccumulate(1, &Crc);
        UT_MavCrcAccumulate(1, &Crc);
        UT_MavCrcAccumulate(MsgId, &Crc);
        UT_MavCrcAccumulate(0, &Crc);
        UT_MavCrcAccumulate(0, &Crc);
        for (i = 0; i < PayloadLen; i++)
        {
            UT_MavCrcAccumulate(Payload[i], &Crc);
        }
        UT_MavCrcAccumulate(CrcExtra, &Crc);

        if ((Crc & 0xFFU) != 0xFDU && (Crc & 0xFFU) != 0xFEU &&
            (Crc >> 8)    != 0xFDU && (Crc >> 8)    != 0xFEU)
        {
            break;
        }
    }

    Frame[0] = 0xFD;
    Frame[1] = PayloadLen;
    Frame[2] = 0;
    Frame[3] = 0;
    Frame[4] = Seq;
    Frame[5] = 1;
    Frame[6] = 1;
    Frame[7] = MsgId;
    Frame[8] = 0;
    Frame[9] = 0;
    memcpy(&Frame[10], Payload, PayloadLen);
    Frame[10 + PayloadLen]     = (uint8)(Crc & 0xFFU);
    Frame[10 + PayloadLen + 1] = (uint8)(Crc >> 8);

    return (size_t)(10 + PayloadLen + 2);
}

/* 캡처된 바이트열에서 msgid가 일치하는 첫 프레임의 payload 시작 포인터를 찾는다. */
static const uint8 *UT_FindMavFrame(const uint8 *Buf, size_t Len, uint8 MsgId, uint8 *OutPayloadLen)
{
    size_t i = 0;

    while (i + 10 <= Len)
    {
        if (Buf[i] == 0xFD)
        {
            uint8  PayloadLen = Buf[i + 1];
            uint8  Mid        = Buf[i + 7];
            size_t FrameLen   = 10U + (size_t)PayloadLen + 2U;

            if (i + FrameLen <= Len)
            {
                if (Mid == MsgId)
                {
                    *OutPayloadLen = PayloadLen;
                    return &Buf[i + 10];
                }
                i += FrameLen;
                continue;
            }
        }
        i++;
    }
    return NULL;
}

/* 주입 프레임(InBytes)을 소켓에 미리 써 두고 ServiceSerial()을 호출한 뒤,
 * 그 결과로 SendMavlinkV2가 같은 fd에 되돌려 쓴 바이트를 OutBuf로 캡처한다. */
static size_t UT_FeedSerialCaptureTx(const uint8 *InBytes, size_t InLen, uint8 *OutBuf, size_t OutBufLen)
{
    int     Sv[2];
    ssize_t Rc;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[0], F_SETFL, O_NONBLOCK), 0);
    if (InLen > 0)
    {
        UtAssert_True(write(Sv[1], InBytes, InLen) == (ssize_t)InLen, "socketpair inject write");
    }

    MAVLINK_BRIDGE_APP_Data.SerialFd             = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId       = 0;
    MAVLINK_BRIDGE_APP_Data.StreamRequestPending = 0;

    MAVLINK_BRIDGE_APP_ServiceSerial();

    Rc = read(Sv[1], OutBuf, OutBufLen);

    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    return (Rc > 0) ? (size_t)Rc : 0;
}

/* GLOBAL_POSITION_INT를 주입해 RefLatE7/RefLonE7을 설정한다 (static이라 직접 접근 불가). */
static void UT_SetGpsReference(int32 LatE7, int32 LonE7)
{
    uint8  Payload[UT_GLOBAL_POSITION_INT_PAYLOAD_LEN];
    uint8  Frame[64];
    uint8  OutBuf[256];
    size_t Len;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[4], (uint32)LatE7);
    UT_WriteU32LE(&Payload[8], (uint32)LonE7);

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_GLOBAL_POSITION_INT_MSGID,
                                  (uint8)UT_GLOBAL_POSITION_INT_CRC_EXTRA, Payload, sizeof(Payload));
    (void)UT_FeedSerialCaptureTx(Frame, Len, OutBuf, sizeof(OutBuf));
}

/* SendMissionItemInt가 GLOBAL_RELATIVE_ALT + lat/lon degE7로 인코딩하는지 확인.
 * §13.1 실측(ArduPilot이 LOCAL_NED 미션 아이템을 거부)에 따른 수정 검증 —
 * legacy MISSION_ITEM과 동일한 변환 공식을 그대로 재현해 기대값과 비교한다. */
void Test_SendMissionItemInt_GlobalRelativeAltFrame(void)
{
    uint8        ReqPayload[2];
    uint8        ReqFrame[16];
    uint8        OutBuf[256];
    size_t       ReqLen;
    size_t       CapLen;
    const uint8 *ItemPayload;
    uint8        ItemPayloadLen;
    int32        RefLatE7 = 100000000; /* 10.0 deg */
    int32        RefLonE7 = 1000000000; /* 100.0 deg */
    float        WpX = 10.0f; /* local north, m */
    float        WpY = 20.0f; /* local east, m */
    float        WpZ = 5.0f;  /* relative altitude, m */
    float        RefLatDeg, RefLonDeg, RefLatRad, DeltaLatDeg, DeltaLonDeg, ExpLat, ExpLon;
    int32        ExpLatE7, ExpLonE7;
    int32        ActLatE7, ActLonE7;
    float        ActAlt;
    uint16       ActSeq, ActCmd;
    uint8        ActFrameByte;

    UT_SetGpsReference(RefLatE7, RefLonE7);

    MAVLINK_BRIDGE_APP_Data.MissionUploadState    = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount  = 1;
    MAVLINK_BRIDGE_APP_Data.MissionPendingX[0]    = WpX;
    MAVLINK_BRIDGE_APP_Data.MissionPendingY[0]    = WpY;
    MAVLINK_BRIDGE_APP_Data.MissionPendingZ[0]    = WpZ;

    UT_WriteU16LE(&ReqPayload[0], 0U); /* seq=0 */
    ReqLen = UT_BuildMavFrameGeneric(ReqFrame, (uint8)UT_MISSION_REQUEST_INT_MSGID,
                                     (uint8)UT_MISSION_REQUEST_INT_CRC_EXTRA, ReqPayload, sizeof(ReqPayload));

    CapLen = UT_FeedSerialCaptureTx(ReqFrame, ReqLen, OutBuf, sizeof(OutBuf));

    ItemPayload = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_ITEM_INT_MSGID, &ItemPayloadLen);
    UtAssert_True(ItemPayload != NULL, "MISSION_ITEM_INT frame captured");
    if (ItemPayload == NULL)
    {
        return;
    }

    /* legacy MISSION_ITEM과 동일한 변환 공식을 재현해 기대값 산출 (하드코딩 상수 대신
     * 같은 수식으로 계산 — float 반올림 경로 일치, tolerance만 소폭 허용) */
    RefLatDeg   = (float)RefLatE7 / 1e7f;
    RefLonDeg   = (float)RefLonE7 / 1e7f;
    RefLatRad   = RefLatDeg * UT_DEG_TO_RAD;
    DeltaLatDeg = WpX / UT_EARTH_RADIUS_M * UT_RAD_TO_DEG;
    DeltaLonDeg = WpY / (UT_EARTH_RADIUS_M * cosf(RefLatRad)) * UT_RAD_TO_DEG;
    ExpLat      = RefLatDeg + DeltaLatDeg;
    ExpLon      = RefLonDeg + DeltaLonDeg;
    ExpLatE7    = (int32)(ExpLat * 1e7f);
    ExpLonE7    = (int32)(ExpLon * 1e7f);

    ActLatE7     = (int32)UT_ReadU32LE(&ItemPayload[16]);
    ActLonE7     = (int32)UT_ReadU32LE(&ItemPayload[20]);
    ActAlt       = UT_ReadFloatLE(&ItemPayload[24]);
    ActSeq       = UT_ReadU16LE(&ItemPayload[28]);
    ActCmd       = UT_ReadU16LE(&ItemPayload[30]);
    ActFrameByte = ItemPayload[34];

    /* 핵심 회귀 방지: LOCAL_NED(1)가 아니라 GLOBAL_RELATIVE_ALT(3)여야 함 */
    UtAssert_INT32_EQ((int32)ActFrameByte, 3);
    UtAssert_True(abs((int)(ActLatE7 - ExpLatE7)) <= 2, "lat degE7 matches formula (tol=2)");
    UtAssert_True(abs((int)(ActLonE7 - ExpLonE7)) <= 2, "lon degE7 matches formula (tol=2)");
    UtAssert_True(ActAlt == WpZ, "alt no sign flip (GLOBAL_RELATIVE_ALT is already altitude-positive)");
    UtAssert_INT32_EQ((int32)ActSeq, 0);
    UtAssert_INT32_EQ((int32)ActCmd, 16); /* MAV_CMD_NAV_WAYPOINT */
}

/* -----------------------------------------------------------------------
 * FC 값 finite 검증 (설계안 A — 입구 차단, 2026-07-13) [[fc_value_validation_gap]]
 *
 * CRC는 전송 오류만 잡고 EKF 발산/센서 고장으로 인한 NaN/Inf는 통과시키므로,
 * PublishAttitude/PublishEkfLocal 파싱 직후 isfinite()로 걸러 SB 게시 자체를
 * 막는지 확인한다.
 * ----------------------------------------------------------------------- */

#define UT_ATTITUDE_MSGID              30U
#define UT_ATTITUDE_CRC_EXTRA           39U
#define UT_LOCAL_POSITION_NED_MSGID     32U
#define UT_LOCAL_POSITION_NED_CRC_EXTRA 185U

/* roll에 NaN 주입 → ATTITUDE 미게시, NonFiniteValueCount 증가, 기존 캐시 불변 */
void Test_PublishAttitude_NaNRejected(void)
{
    uint8  Payload[28];
    uint8  Frame[64];
    size_t Len;
    float  NaNValue = NAN;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 12345U); /* time_boot_ms */
    memcpy(&Payload[4], &NaNValue, sizeof(float)); /* roll = NaN */

    MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid       = 0;
    MAVLINK_BRIDGE_APP_Data.AttitudeTlm.TimestampMs = 999U; /* 이전 값 — 거부 시 불변 확인용 */
    MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount     = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_ATTITUDE_MSGID, (uint8)UT_ATTITUDE_CRC_EXTRA,
                                  Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid, 0);
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.TimestampMs, 999);
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount, 1);
}

/* vz에 +Inf 주입 → LOCAL_POSITION_NED 미게시, NonFiniteValueCount 증가 */
void Test_PublishEkfLocal_InfRejected(void)
{
    uint8  Payload[28];
    uint8  Frame[64];
    size_t Len;
    float  InfValue = INFINITY;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 12345U);
    memcpy(&Payload[24], &InfValue, sizeof(float)); /* vz = +Inf */

    MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid       = 0;
    MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.TimestampMs = 888U;
    MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount     = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_LOCAL_POSITION_NED_MSGID,
                                  (uint8)UT_LOCAL_POSITION_NED_CRC_EXTRA, Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid, 0);
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.TimestampMs, 888);
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount, 1);
}

/* 정상 값이면 그대로 통과 (finite 검증이 정상 케이스를 막지 않는지 회귀 확인) */
void Test_PublishAttitude_FiniteValuesAccepted(void)
{
    uint8  Payload[28];
    uint8  Frame[64];
    size_t Len;
    float  Roll = 0.1f;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 5000U);
    memcpy(&Payload[4], &Roll, sizeof(float));

    MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_ATTITUDE_MSGID, (uint8)UT_ATTITUDE_CRC_EXTRA,
                                  Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid, 1);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.AttitudeTlm.RollRad == Roll, "RollRad passed through");
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount, 0);
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
    ADD_TEST(SysTime_FullPayload);
    ADD_TEST(SysTime_TrimmedPayload);
    ADD_TEST(SysTime_ZeroClockIgnored);
    ADD_TEST(SysTime_CrcFail);
    ADD_TEST(SendMissionItemInt_GlobalRelativeAltFrame);
    ADD_TEST(PublishAttitude_NaNRejected);
    ADD_TEST(PublishEkfLocal_InfRejected);
    ADD_TEST(PublishAttitude_FiniteValuesAccepted);
}

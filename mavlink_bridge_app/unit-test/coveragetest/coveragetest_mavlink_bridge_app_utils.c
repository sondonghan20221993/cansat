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
}

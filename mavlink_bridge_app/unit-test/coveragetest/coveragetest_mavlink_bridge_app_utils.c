#include "mavlink_bridge_app_coveragetest_common.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * StartMissionUpload ARMED 정책 테스트(BL-56, 2026-07-25: ARMED 차단 전면 폐지)
 * ----------------------------------------------------------------------- */

/* ARMED 상태에서도 mission upload가 그대로 진행됨(차단 폐지 회귀 방지) */
void Test_StartMissionUpload_AllowedWhenArmed(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 1U; /* REPLACE */
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].LatE7 = 10; Msg.Waypoints[0].LonE7 = 20; Msg.Waypoints[0].Z = 3.0f;
    Msg.Waypoints[1].LatE7 = 30; Msg.Waypoints[1].LonE7 = 40; Msg.Waypoints[1].Z = 3.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState  = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed    = 1;
    MAVLINK_BRIDGE_APP_Data.FcBaseMode = 0x80;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_CLEARING);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
}

/* DISARMED 상태에서도 동일하게 정상 진행(회귀 확인) */
void Test_StartMissionUpload_AllowedWhenDisarmed(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 1U;
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].LatE7 = 1; Msg.Waypoints[0].LonE7 = 2; Msg.Waypoints[0].Z = 3.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 0;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_CLEARING);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
}

/* FC 링크 미연결 → 링크 오류로 차단(ARMED 여부 무관) */
void Test_StartMissionUpload_LinkNotConnected(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 1U;
    Msg.WaypointCount  = 1;
    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 1;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
}

/* BL-85(2026-07-28 감사) 회귀: readback(WAIT_COUNT/WAIT_ITEM) 진행 중엔
 * ROUTE_UPDATE를 무시해야 함 — 두 MISSION 프로토콜 상태머신이 동시에 굴러가는
 *것을 막는다. */
void Test_StartMissionUpload_IgnoredWhenReadbackInProgress(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType     = 1U;
    Msg.WaypointCount = 1;
    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
}

/* -----------------------------------------------------------------------
 * StartMissionUpload REPLACE / ADD / DELETE / MODIFY 동작 테스트(BL-56)
 * ----------------------------------------------------------------------- */

/* REPLACE: pending에 새 waypoint 복사 */
void Test_StartMissionUpload_Replace(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 1U; /* ROUTE_OP_REPLACE */
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].LatE7 = 10; Msg.Waypoints[0].LonE7 = 20; Msg.Waypoints[0].Z = 5.0f;
    Msg.Waypoints[1].LatE7 = 30; Msg.Waypoints[1].LonE7 = 40; Msg.Waypoints[1].Z = 5.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 3; /* active 무시하고 새 것으로 교체 */

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_CLEARING);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 2);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0], 10);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[1], 30);
}

/* ADD: active 끝에 새 waypoint 추가(구 APPEND 개명, 중간 삽입 없음) */
void Test_StartMissionUpload_Add(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 2U; /* ROUTE_OP_ADD */
    Msg.WaypointCount  = 2;
    Msg.Waypoints[0].LatE7 = 50; Msg.Waypoints[0].Z = 5.0f;
    Msg.Waypoints[1].LatE7 = 60; Msg.Waypoints[1].Z = 5.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed   = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 1;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[0] = 10;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointZ[0]     = 5.0f;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex      = 0;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0], 10);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[1], 50);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[2], 60);
    /* ActiveResumeIndex는 ADD로 불변 */
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex, 0);
}

/* ADD: active + 새 waypoint 합산이 MAX 초과 → 잘라냄 */
void Test_StartMissionUpload_AddTruncated(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;
    uint8 i;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType     = 2U; /* ROUTE_OP_ADD */
    Msg.WaypointCount = 4;
    for (i = 0; i < 4; i++) { Msg.Waypoints[i].LatE7 = (int32)(i + 100); }

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = (uint8)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS - 1;
    for (i = 0; i < MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount; i++)
    {
        MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[i] = (int32)i;
    }

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    /* max에서 잘려야 함 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount,
                      (int32)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS);
}

/* BL-84/C-12(2026-07-28 감사) 회귀: ActiveCount(16)+WaypointCount(250, 검증 없는
 * wire 값)=266이 uint8 NewCount에 그대로 대입되면 266&0xFF=10으로 조용히
 * 절단돼 MAX(37) 이하로 보여서 절단 경고 분기 자체가 안 타고, 기존 16개마저
 * 10개로 줄어드는 조용한 미션 손상이 발생했다. uint16으로 넓혀 계산하면
 * MAX로 클램프돼야 한다(10이 아니라 37). */
void Test_StartMissionUpload_Add_Uint8OverflowClampedNotWrapped(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;
    uint8 i;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType     = 2U; /* ROUTE_OP_ADD */
    Msg.WaypointCount = 250; /* IndexOrCount — ActiveCount(16)와 합하면 266, uint8 wrap 시 10 */

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 16U;
    for (i = 0; i < 16U; i++)
    {
        MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[i] = (int32)i;
    }

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    /* 버그였다면 10(=266&0xFF, 기존 16개보다도 적음), 수정 후엔 MAX로 클램프 */
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount != 10,
                  "uint8 wrap으로 10이 되면 안 됨 (실제 %d)",
                  (int)MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount,
                      (int32)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS);
}

/* DELETE(index): 대상 인덱스를 제거하고 뒤 인덱스를 당김 */
void Test_StartMissionUpload_Delete(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 3U; /* ROUTE_OP_DELETE */
    Msg.WaypointCount  = 1;  /* index=1 삭제 */

    MAVLINK_BRIDGE_APP_Data.LinkState              = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed                = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount    = 4;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[0] = 1;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[1] = 2;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[2] = 3;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[3] = 4;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex      = 3; /* index(1) < ActiveResumeIndex(3) */

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 3);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0], 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[1], 3); /* index 1 제거, 뒤가 당겨짐 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[2], 4);
    /* index < ActiveResumeIndex이므로 ActiveResumeIndex -= 1 */
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex, 2);
}

/* DELETE(index): 범위 밖 인덱스 → 거부, MissionUploadState 불변 */
void Test_StartMissionUpload_Delete_IndexOutOfRange(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 3U;
    Msg.WaypointCount  = 5; /* active(3)보다 큼 */

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 3;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState  = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
}

/* DELETE(index==ActiveResumeIndex) → 거부(진행 중인 목표점은 삭제 불가) */
void Test_StartMissionUpload_Delete_RejectedWhenTargetIsActiveResumeIndex(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType      = 3U; /* ROUTE_OP_DELETE */
    Msg.WaypointCount  = 1;  /* index=1 */

    MAVLINK_BRIDGE_APP_Data.LinkState           = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed             = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount = 4;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex   = 1; /* 삭제 대상과 동일 -> 거부 */
    MAVLINK_BRIDGE_APP_Data.MissionUploadState  = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
    /* ActiveResumeIndex도 그대로 유지 */
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex, 1);
}

/* MODIFY(index): 전체 레코드(CmdType+Param+좌표) 덮어쓰기 — 좌표뿐 아니라 CmdType/Param도 교체 */
void Test_StartMissionUpload_Modify_FullRecordOverwrite(void)
{
    MAVLINK_BRIDGE_APP_RouteUpdateMirror_t Msg;

    memset(&Msg, 0, sizeof(Msg));
    Msg.RouteType             = 4U; /* ROUTE_OP_MODIFY */
    Msg.WaypointCount         = 1;  /* index=1 */
    Msg.Waypoints[0].CmdType  = 17U; /* NAV_LOITER_UNLIM */
    Msg.Waypoints[0].Param1   = 9.0f;
    Msg.Waypoints[0].Param2   = 8.0f;
    Msg.Waypoints[0].LatE7    = 999;
    Msg.Waypoints[0].LonE7    = 888;
    Msg.Waypoints[0].Z        = 6.0f;

    MAVLINK_BRIDGE_APP_Data.LinkState                = MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.IsArmed                  = 0;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCount      = 3;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCmdType[0] = 16U;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[0]   = 1;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCmdType[1] = 16U;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[1]   = 2;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointCmdType[2] = 16U;
    MAVLINK_BRIDGE_APP_Data.ActiveWaypointLatE7[2]   = 3;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex        = 1; /* MODIFY는 ActiveResumeIndex 대상도 허용 */

    MAVLINK_BRIDGE_APP_StartMissionUpload(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount, 3);
    /* index 0, 2는 그대로 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0], 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[2], 3);
    /* index 1은 CmdType/Param/좌표 전부 교체 */
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.MissionPendingCmdType[1], 17);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingParam1[1] == 9.0f, "Param1 replaced");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingParam2[1] == 8.0f, "Param2 replaced");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[1], 999);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionPendingLonE7[1], 888);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.MissionPendingZ[1] == 6.0f, "Z replaced");
    /* ActiveResumeIndex는 MODIFY로 불변(대상이어도 허용) */
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex, 1);
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
    Msg.SourceSequence = 33; /* BL-08 */

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
    /* BL-08(2026-07-22): EXEC_RESULT OK 회신 확인 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence, 33);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceApp, (int32)EXEC_RESULT_SOURCE_MAVLINK_BRIDGE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
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
    Msg.SourceSequence = 34; /* BL-08 */

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
    /* BL-08(2026-07-22): EXEC_RESULT FAILED 회신 확인(reject_value 경로) */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence, 34);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
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
    Msg.SourceSequence = 35; /* BL-08 */

    MAVLINK_BRIDGE_APP_Data.ErrCounter = 0;
    MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence = 0; /* 이전 값과 구분 */
    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, (int32)OldVal);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ErrCounter, 0);
    /* BL-08(2026-07-22): 다른 앱 대상이라 EXEC_RESULT 발행 안 됨 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence, 0);
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
 * BL-41(2026-07-23): CONFIG 지속 상태(신규 상태파일) — cfs_core_app/
 * uplink_app과 동일한 매직+체크섬+ConfigVersion+원자적 rename 패턴.
 * 이 앱은 기존 상태파일이 없어 SaveState/LoadState/PersistentState_t가
 * 전부 신규 — 테스트가 아직 없는 인터페이스를 요구하는 TDD red 상태.
 * ----------------------------------------------------------------------- */

void Test_LoadState_NoFile(void)
{
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 500;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 500);
}

void Test_SaveState_NoDir(void)
{
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 500;

    MAVLINK_BRIDGE_APP_SaveState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 500);
}

void Test_SaveLoadState_RoundTrip(void)
{
    const char *Path = "/tmp/mavlink_bridge_app_ut_state_roundtrip.bin";

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs       = 1111;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.LocalPositionIntervalUs  = 2222;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.GlobalPositionIntervalUs = 3333;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.GpsRawIntervalUs         = 4444;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.EkfStatusIntervalUs      = 5555;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs      = 6666;
    MAVLINK_BRIDGE_APP_Data.ActiveConfig.HeartbeatIntervalMs      = 7777;

    MAVLINK_BRIDGE_APP_SaveState();

    memset(&MAVLINK_BRIDGE_APP_Data.ActiveConfig, 0, sizeof(MAVLINK_BRIDGE_APP_Data.ActiveConfig));

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 1111);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.LocalPositionIntervalUs, 2222);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.GlobalPositionIntervalUs, 3333);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.GpsRawIntervalUs, 4444);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.EkfStatusIntervalUs, 5555);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs, 6666);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.HeartbeatIntervalMs, 7777);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_LoadState_Truncated(void)
{
    const char *Path = "/tmp/mavlink_bridge_app_ut_state_truncated.bin";
    int         Fd;
    uint8       Short[5] = {1, 2, 3, 4, 5};

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Short, sizeof(Short));
    close(Fd);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 999;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 999);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_LoadState_BadMagic(void)
{
    const char *Path = "/tmp/mavlink_bridge_app_ut_state_badmagic.bin";
    int         Fd;
    uint32      Garbage[12] = {0xDEADBEEFU, 0};

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, Garbage, sizeof(MAVLINK_BRIDGE_APP_PersistentState_t));
    close(Fd);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 999;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 999);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

/* 매직/체크섬은 맞지만 ConfigVersion만 다른 구버전 파일 → 전체 폴백 */
void Test_LoadState_ConfigVersionMismatch(void)
{
    const char                         *Path = "/tmp/mavlink_bridge_app_ut_state_badversion.bin";
    int                                  Fd;
    MAVLINK_BRIDGE_APP_PersistentState_t State;

    memset(&State, 0, sizeof(State));
    State.Magic                    = MAVLINK_BRIDGE_APP_STATE_MAGIC;
    State.ConfigVersion            = (uint8)(MAVLINK_BRIDGE_APP_CONFIG_VERSION + 1U);
    State.ActiveConfig.AttitudeIntervalUs = 8888;
    State.Checksum                 = State.Magic + (uint32)State.ConfigVersion +
                                      State.ActiveConfig.AttitudeIntervalUs;

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, &State, sizeof(State));
    close(Fd);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 100;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 100);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_LoadState_ChecksumMismatch(void)
{
    const char                         *Path = "/tmp/mavlink_bridge_app_ut_state_badcrc.bin";
    int                                  Fd;
    MAVLINK_BRIDGE_APP_PersistentState_t State;

    memset(&State, 0, sizeof(State));
    State.Magic         = MAVLINK_BRIDGE_APP_STATE_MAGIC;
    State.ConfigVersion = MAVLINK_BRIDGE_APP_CONFIG_VERSION;
    State.Checksum      = 0; /* 틀린 체크섬 */

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    Fd = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    write(Fd, &State, sizeof(State));
    close(Fd);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 999;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 999);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_LoadState_OpenErrorNotEnoent(void)
{
    const char *RegularFile = "/tmp/mavlink_bridge_app_ut_not_a_dir.bin";
    const char *BogusPath   = "/tmp/mavlink_bridge_app_ut_not_a_dir.bin/x";
    int         Fd;

    Fd = open(RegularFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    UtAssert_True(Fd >= 0, "test fixture file created");
    close(Fd);

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", BogusPath, 1);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs = 999;

    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.AttitudeIntervalUs, 999);

    unlink(RegularFile);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_SaveState_WriteFail(void)
{
    const char   *Path = "/tmp/mavlink_bridge_app_ut_state_writefail.bin";
    struct rlimit OldLimit, NewLimit;
    void        (*OldHandler)(int);

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);

    getrlimit(RLIMIT_FSIZE, &OldLimit);
    NewLimit.rlim_cur = 1;
    NewLimit.rlim_max = OldLimit.rlim_max;
    setrlimit(RLIMIT_FSIZE, &NewLimit);
    OldHandler = signal(SIGXFSZ, SIG_IGN);

    MAVLINK_BRIDGE_APP_SaveState();

    signal(SIGXFSZ, OldHandler);
    setrlimit(RLIMIT_FSIZE, &OldLimit);

    UtAssert_True(access(Path, F_OK) != 0, "write 실패 시 최종 상태파일 생성 안 됨");

    unlink("/tmp/mavlink_bridge_app_ut_state_writefail.bin.tmp");
    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_SaveState_RenameFail(void)
{
    const char *Path = "/tmp/mavlink_bridge_app_ut_state_renamefail_dir";

    mkdir(Path, 0755);
    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);

    MAVLINK_BRIDGE_APP_SaveState();

    UtAssert_True(access(Path, F_OK) == 0, "목적지 경로 존재(디렉터리 그대로)");

    unlink("/tmp/mavlink_bridge_app_ut_state_renamefail_dir.tmp");
    rmdir(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_ProcessConfigCommand_PersistsOnSuccess(void)
{
    const char                        *Path = "/tmp/mavlink_bridge_app_ut_state_configwire.bin";
    MAVLINK_BRIDGE_APP_ConfigCmdTlm_t  Msg;

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    build_mav_config_msg(&Msg, MAVLINK_BRIDGE_APP_CONFIG_SCOPE, MAVLINK_BRIDGE_APP_CONFIG_VERSION,
                         MAVLINK_BRIDGE_PARAM_RECONNECT_INTERVAL_MS, 4242U);

    MAVLINK_BRIDGE_APP_ProcessConfigCommand(&Msg);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.LastConfigResult, (int32)MAVLINK_BRIDGE_CONFIG_RESULT_OK);

    MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs = 0;
    MAVLINK_BRIDGE_APP_LoadState();

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ActiveConfig.ReconnectIntervalMs, 4242);

    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_SaveState_DirFsync_NoSlashInPath(void)
{
    const char *Path = "mavlink_bridge_app_ut_bare_state.bin";

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);
    unlink(Path);

    MAVLINK_BRIDGE_APP_SaveState();

    UtAssert_True(access(Path, F_OK) == 0, "슬래시 없는 경로에서도 저장 완료");

    unlink("mavlink_bridge_app_ut_bare_state.bin.tmp");
    unlink(Path);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
}

void Test_SaveState_DirFsync_ParentOpenFail(void)
{
    const char *Dir = "/tmp/mavlink_bridge_app_ut_dirfsync_noread";
    char        Path[256];

    mkdir(Dir, 0755);
    chmod(Dir, 0300);
    snprintf(Path, sizeof(Path), "%s/state.bin", Dir);

    setenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH", Path, 1);

    MAVLINK_BRIDGE_APP_SaveState();

    chmod(Dir, 0755);
    UtAssert_True(access(Path, F_OK) == 0, "디렉터리 fsync 실패해도 최종 파일은 저장됨");

    unlink(Path);
    {
        char TmpPath[280];
        snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", Path);
        unlink(TmpPath);
    }
    rmdir(Dir);
    unsetenv("MAVLINK_BRIDGE_APP_STATE_FILE_PATH");
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

/* 임의 msgid/crc_extra로 MAVLink v2 프레임 생성 (UT_BuildSysTimeFrame의 범용판) —
 * BL-75/76/77 회귀테스트가 SYS_TIME 외 msgid(COMMAND_ACK/ATTITUDE/HEARTBEAT)로
 * 프레임을 만들 때 사용. CRC 바이트가 STX와 겹치면 seq를 바꿔가며 재시도. */
static size_t UT_BuildV2Frame(uint8 *Frame, uint8 MsgId, uint8 CrcExtra, const uint8 *Payload, uint8 PayloadLen,
                               bool CorruptCrc)
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
        UT_MavCrcAccumulate(MsgId, &Crc);
        UT_MavCrcAccumulate(0, &Crc);  /* msgid mid */
        UT_MavCrcAccumulate(0, &Crc);  /* msgid high */
        for (i = 0; i < PayloadLen; i++)
        {
            UT_MavCrcAccumulate(Payload[i], &Crc);
        }
        UT_MavCrcAccumulate(CrcExtra, &Crc);

        if (CorruptCrc)
        {
            Crc ^= 0x0101U;
        }

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

/* mavlink_bridge_app_utils.c의 동일 상수(파일-로컬 #define이라 테스트에서
 * 직접 참조 불가 — 값만 미러링, 어긋나면 두 곳 다 확인) */
#define UT_COMMAND_ACK_MSGID     77U
#define UT_COMMAND_ACK_CRC_EXTRA 143U

#define UT_HEARTBEAT_MSGID     0U
#define UT_HEARTBEAT_CRC_EXTRA 50U

/* ---- HEARTBEAT CRC 검증 회귀테스트 (BL-77, 2026-07-28 감사) ----
 * 유효한 CRC의 PX4(autopilot=12) 하트비트 → TargetSystemId 락온 +
 * CONNECTED 승격 + IsArmed 갱신까지 정상 수행돼야 함(정상 경로 회귀 확인). */
void Test_Heartbeat_ValidCrc_LocksTargetAndConnects(void)
{
    uint8  Payload[9];
    uint8  Frame[32];
    size_t Len;

    memset(Payload, 0, sizeof(Payload));
    Payload[4] = 2U;    /* type = quadrotor */
    Payload[5] = 12U;   /* autopilot = PX4 */
    Payload[6] = 0x80U; /* base_mode: armed bit set */
    Payload[7] = 4U;    /* system_status = ACTIVE */
    Payload[8] = 3U;    /* mavlink_version */

    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = 0;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 0;
    MAVLINK_BRIDGE_APP_Data.LinkState         = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount   = 0;

    Len = UT_BuildV2Frame(Frame, UT_HEARTBEAT_MSGID, UT_HEARTBEAT_CRC_EXTRA, Payload, sizeof(Payload), false);
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.TargetSystemId, 1);
    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.LinkState, (int)MAVLINK_BRIDGE_LINK_CONNECTED);
    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.IsArmed, 1);
}

/* CRC 불일치 하트비트 → TargetSystemId 락온/IsArmed 갱신은 거부돼야 함
 * (이전엔 CRC 검증 자체가 없어 노이즈에도 락온됐음). LinkState는 여기서
 * 검증하지 않음 — `HandleReceivedBytes()`가 CRC/msgid 무관하게 아무 바이트나
 * 받으면 CONNECTED로 승격시키는 별개의 버그(BL-84/C-10, 미수정)가 있어
 * HEARTBEAT CRC 검증과 무관하게 이 필드는 항상 CONNECTED로 관측됨 —
 * BL-84에서 수정 후 이 테스트에 LinkState 검증 추가할 것. */
void Test_Heartbeat_BadCrc_RejectedNoLockNoConnect(void)
{
    uint8  Payload[9];
    uint8  Frame[32];
    size_t Len;

    memset(Payload, 0, sizeof(Payload));
    Payload[4] = 2U;
    Payload[5] = 12U;
    Payload[6] = 0x80U;
    Payload[7] = 4U;
    Payload[8] = 3U;

    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = 0;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 0;
    MAVLINK_BRIDGE_APP_Data.LinkState         = 0;
    MAVLINK_BRIDGE_APP_Data.IsArmed           = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount   = 0;

    Len = UT_BuildV2Frame(Frame, UT_HEARTBEAT_MSGID, UT_HEARTBEAT_CRC_EXTRA, Payload, sizeof(Payload), true);
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.TargetSystemId, 0);
    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.IsArmed, 0);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.ParseErrorCount >= 1, "parse error recorded");
    /* BL-84/C-10(2026-07-28) 수정 후: HandleReceivedBytes가 더 이상 무조건
     * CONNECTED로 승격시키지 않으므로, CRC 실패한 HEARTBEAT만으로는 LinkState가
     * 그대로 유지돼야 함(BL-77 작성 시엔 C-10 미수정이라 이 assert를 보류했었음) */
    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.LinkState, 0);
}

/* ---- COMMAND_ACK 필드 오프셋 회귀테스트 (BL-75, 2026-07-28 감사) ----
 * 이전 코드는 command@Payload[8]/result@Payload[0]으로 읽었으나 실제 wire는
 * command(u16)@0-1/result(u8)@2. Command=MAVLINK_CMD_SET_MESSAGE_INTERVAL(511)/
 * Result=ACCEPTED(0)로 프레임을 만들어 StreamRequestAckCount가 실제로
 * 증가하는지 확인한다(오프셋이 틀리면 Command!=511로 읽혀 이 분기를 안 탐). */
void Test_CommandAck_FieldOffsetCorrect(void)
{
    uint8  Payload[10] = {0xFF, 0x01, /* command = 511 = MAVLINK_CMD_SET_MESSAGE_INTERVAL (LE) */
                          0,           /* result = MAV_RESULT_ACCEPTED */
                          0, 0, 0, 0,  /* progress + result_param2 */
                          0, 0};       /* target_system, target_component */
    uint8  Frame[32];
    size_t Len;

    MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount = 0;

    Len = UT_BuildV2Frame(Frame, UT_COMMAND_ACK_MSGID, UT_COMMAND_ACK_CRC_EXTRA, Payload, sizeof(Payload),
                          false);
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount, 1);
}

/* base-only(3바이트, v2 확장필드 trim) ACK도 정상 처리돼야 함 — 예전엔
 * `>= 10U` 조건 때문에 전부 RecordParseError로 계상됐음. */
void Test_CommandAck_BaseOnly3ByteAccepted(void)
{
    uint8  Payload[3] = {0xFF, 0x01, 0}; /* command=511, result=ACCEPTED */
    uint8  Frame[32];
    size_t Len;

    MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount       = 0;

    Len = UT_BuildV2Frame(Frame, UT_COMMAND_ACK_MSGID, UT_COMMAND_ACK_CRC_EXTRA, Payload, sizeof(Payload),
                          false);
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int)MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount, 1);
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
 * BL-56(2026-07-25): waypoint가 항상 절대좌표라 로컬->전역 변환이 사라졌으므로,
 * 수신한 LatE7/LonE7을 그대로 MISSION_ITEM_INT에 기입하는지 직접 검증한다.
 * 또한 offset 0~15(Param1~4)/30~31(CmdType)/35(current 플래그)도 함께 확인. */
void Test_SendMissionItemInt_GlobalRelativeAltFrame(void)
{
    uint8        ReqPayload[2];
    uint8        ReqFrame[16];
    uint8        OutBuf[256];
    size_t       ReqLen;
    size_t       CapLen;
    const uint8 *ItemPayload;
    uint8        ItemPayloadLen;
    int32        WpLatE7 = 123456780;
    int32        WpLonE7 = 987654320;
    float        WpZ = 5.0f;
    int32        ActLatE7, ActLonE7;
    float        ActAlt, ActP1;
    uint16       ActSeq, ActCmd;
    uint8        ActFrameByte, ActCurrent;

    MAVLINK_BRIDGE_APP_Data.MissionUploadState        = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount      = 1;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex         = 0; /* seq 0 == ActiveResumeIndex -> current=1 */
    MAVLINK_BRIDGE_APP_Data.MissionPendingCmdType[0]  = 16U; /* NAV_WAYPOINT */
    MAVLINK_BRIDGE_APP_Data.MissionPendingParam1[0]   = 3.5f;
    MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0]    = WpLatE7;
    MAVLINK_BRIDGE_APP_Data.MissionPendingLonE7[0]    = WpLonE7;
    MAVLINK_BRIDGE_APP_Data.MissionPendingZ[0]        = WpZ;

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

    ActP1        = UT_ReadFloatLE(&ItemPayload[0]);
    ActLatE7     = (int32)UT_ReadU32LE(&ItemPayload[16]);
    ActLonE7     = (int32)UT_ReadU32LE(&ItemPayload[20]);
    ActAlt       = UT_ReadFloatLE(&ItemPayload[24]);
    ActSeq       = UT_ReadU16LE(&ItemPayload[28]);
    ActCmd       = UT_ReadU16LE(&ItemPayload[30]);
    ActFrameByte = ItemPayload[34];
    ActCurrent   = ItemPayload[35];

    /* 핵심 회귀 방지: LOCAL_NED(1)가 아니라 GLOBAL_RELATIVE_ALT(3)여야 함 */
    UtAssert_INT32_EQ((int32)ActFrameByte, 3);
    UtAssert_INT32_EQ(ActLatE7, WpLatE7);
    UtAssert_INT32_EQ(ActLonE7, WpLonE7);
    UtAssert_True(ActAlt == WpZ, "alt passthrough, always-absolute now");
    UtAssert_True(ActP1 == 3.5f, "Param1 encoded at offset 0");
    UtAssert_INT32_EQ((int32)ActSeq, 0);
    UtAssert_INT32_EQ((int32)ActCmd, 16); /* CmdType 그대로 전달 */
    UtAssert_INT32_EQ((int32)ActCurrent, 1); /* seq(0) == ActiveResumeIndex(0) */
}

/* current 플래그는 ActiveResumeIndex에 해당하는 seq에만 1, 나머지는 0 */
void Test_SendMissionItemInt_CurrentFlagOnlyOnActiveResumeIndex(void)
{
    uint8        ReqPayload[2];
    uint8        ReqFrame[16];
    uint8        OutBuf[256];
    size_t       ReqLen;
    size_t       CapLen;
    const uint8 *ItemPayload;
    uint8        ItemPayloadLen;

    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount = 3;
    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex    = 2;

    /* seq=1 (ActiveResumeIndex가 아님) -> current=0 */
    UT_WriteU16LE(&ReqPayload[0], 1U);
    ReqLen = UT_BuildMavFrameGeneric(ReqFrame, (uint8)UT_MISSION_REQUEST_INT_MSGID,
                                     (uint8)UT_MISSION_REQUEST_INT_CRC_EXTRA, ReqPayload, sizeof(ReqPayload));
    CapLen = UT_FeedSerialCaptureTx(ReqFrame, ReqLen, OutBuf, sizeof(OutBuf));
    ItemPayload = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_ITEM_INT_MSGID, &ItemPayloadLen);
    UtAssert_True(ItemPayload != NULL, "seq=1 MISSION_ITEM_INT frame captured");
    if (ItemPayload != NULL)
    {
        UtAssert_INT32_EQ((int32)ItemPayload[35], 0);
    }

    /* seq=2 (ActiveResumeIndex) -> current=1 */
    UT_WriteU16LE(&ReqPayload[0], 2U);
    ReqLen = UT_BuildMavFrameGeneric(ReqFrame, (uint8)UT_MISSION_REQUEST_INT_MSGID,
                                     (uint8)UT_MISSION_REQUEST_INT_CRC_EXTRA, ReqPayload, sizeof(ReqPayload));
    CapLen = UT_FeedSerialCaptureTx(ReqFrame, ReqLen, OutBuf, sizeof(OutBuf));
    ItemPayload = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_ITEM_INT_MSGID, &ItemPayloadLen);
    UtAssert_True(ItemPayload != NULL, "seq=2 MISSION_ITEM_INT frame captured");
    if (ItemPayload != NULL)
    {
        UtAssert_INT32_EQ((int32)ItemPayload[35], 1);
    }
}

/* MISSION_CURRENT(msg #42) 파싱 -> ActiveResumeIndex 갱신 */
void Test_MissionCurrent_UpdatesActiveResumeIndex(void)
{
    uint8  Payload[2];
    uint8  Frame[16];
    size_t Len;

    MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex = 0;

    UT_WriteU16LE(&Payload[0], 5U);
    Len = UT_BuildMavFrameGeneric(Frame, 42U, 28U, Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.ActiveResumeIndex, 5);
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

/* ---- BL-76(2026-07-28) 회귀: v2 trailing-zero trim된 프레임도 정상 디코드 ----
 * 마지막 필드(yawspeed/vz/hdg)가 정확히 0.0이면 MAVLink v2 송신측이 그 바이트를
 * 트림해 페이로드가 짧게 옴 — 실제 규격 동작. 예전 코드는 `PayloadLen != 28`을
 * 엄격 비교해 이런 정상 프레임을 폐기했다. */

#define UT_GLOBAL_POSITION_INT_MSGID     33U
#define UT_GLOBAL_POSITION_INT_CRC_EXTRA 104U

/* yawspeed(offset 24, 마지막 필드) == 0 → 24바이트로 트림 → 정상 디코드돼야 함 */
void Test_PublishAttitude_TrimmedYawspeedZero_Accepted(void)
{
    uint8  Payload[24]; /* yawspeed 필드(마지막 4B) 트림됨 */
    uint8  Frame[64];
    size_t Len;
    float  Roll = 0.1f;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 7000U);
    memcpy(&Payload[4], &Roll, sizeof(float));

    MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid   = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount     = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_ATTITUDE_MSGID, (uint8)UT_ATTITUDE_CRC_EXTRA,
                                  Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid, 1);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.AttitudeTlm.RollRad == Roll, "trimmed frame decoded, roll passthrough");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.AttitudeTlm.YawspeedRps == 0.0f, "trimmed field zero-extended");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* vz(offset 24, 마지막 필드) == 0 → 24바이트로 트림 → 정상 디코드돼야 함 */
void Test_PublishEkfLocal_TrimmedVzZero_Accepted(void)
{
    uint8  Payload[24];
    uint8  Frame[64];
    size_t Len;
    float  X = 1.5f;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 7100U);
    memcpy(&Payload[4], &X, sizeof(float));

    MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount   = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_LOCAL_POSITION_NED_MSGID,
                                  (uint8)UT_LOCAL_POSITION_NED_CRC_EXTRA, Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid, 1);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.X_m == X, "trimmed frame decoded, x passthrough");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Vz_mps == 0.0f, "trimmed field zero-extended");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* hdg(offset 26, 마지막 필드, u16) == 0 → 26바이트로 트림 → 정상 디코드돼야 함 */
void Test_PublishGlobalPositionAsLocal_TrimmedHdgZero_Accepted(void)
{
    uint8  Payload[26]; /* hdg(u16) 트림됨 */
    uint8  Frame[64];
    size_t Len;

    memset(Payload, 0, sizeof(Payload));
    UT_WriteU32LE(&Payload[0], 7200U);
    UT_WriteU32LE(&Payload[16], 2000U); /* relative_alt = 2000mm (양수라 부호 무관) */

    MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid = 0;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount   = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_GLOBAL_POSITION_INT_MSGID,
                                  (uint8)UT_GLOBAL_POSITION_INT_CRC_EXTRA, Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid, 1);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Z_m == -2.0f, "trimmed frame decoded, alt passthrough");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ParseErrorCount, 0);
}

/* 페이로드 바이트에 STX_V1(0xFE)/STX_V2(0xFD) 값이 포함돼도 파서가 프레임
 * 중간에 재진입(리셋)하지 않고 정상 완주하는지 회귀 확인.
 * (파서가 상태 무관하게 모든 바이트를 STX로 검사하던 버그의 재발 방지) */
void Test_ProcessReceivedByte_StxByteInPayload_NoReentry(void)
{
    uint8  Payload[28];
    uint8  Frame[64];
    size_t Len;
    float  Roll = 0.25f;

    memset(Payload, 0, sizeof(Payload));
    Payload[0] = 0xFDU; /* MAVLINK_STX_V2 */
    Payload[1] = 0xFEU; /* MAVLINK_STX_V1 */
    Payload[2] = 0x00;
    Payload[3] = 0x00; /* time_boot_ms = 0x0000FEFD */
    memcpy(&Payload[4], &Roll, sizeof(float));

    MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid = 0;

    Len = UT_BuildMavFrameGeneric(Frame, (uint8)UT_ATTITUDE_MSGID, (uint8)UT_ATTITUDE_CRC_EXTRA,
                                  Payload, sizeof(Payload));
    UT_FeedSerial(Frame, Len);

    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid, 1);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.AttitudeTlm.RollRad == Roll, "RollRad passed through despite STX bytes in payload");
}

/* -----------------------------------------------------------------------
 * RequestTelemetryStreams — 스트림 요청 경로 테스트
 * MAVLINK_MSG_ID_COMMAND_LONG=76, COMMAND_LONG payload=33B -> frame=45B(10+33+2).
 * 6개 스트림(ATTITUDE/LOCAL_POSITION_NED/GLOBAL_POSITION_INT/GPS_RAW_INT/
 * EKF_STATUS_REPORT/SYS_TIME) 각 1개 COMMAND_LONG 요청 프레임을 보낸다.
 * ----------------------------------------------------------------------- */
#define UT_COMMAND_LONG_MSGID   76U
#define UT_COMMAND_LONG_FRAME_LEN 45U /* 10(header) + 33(payload) + 2(crc) */

void Test_RequestTelemetryStreams_SendsSixStreamRequests(void)
{
    int     Sv[2];
    uint8   OutBuf[512];
    ssize_t CapLen;
    size_t  i;
    int     FrameCount = 0;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[0], F_SETFL, O_NONBLOCK), 0);
    UtAssert_INT32_EQ(fcntl(Sv[1], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd       = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId = 1;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 1;

    MAVLINK_BRIDGE_APP_RequestTelemetryStreams();

    CapLen = read(Sv[1], OutBuf, sizeof(OutBuf));
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    UtAssert_True(CapLen == (ssize_t)(6U * UT_COMMAND_LONG_FRAME_LEN),
                  "6 stream request frames captured (got %ld bytes)", (long)CapLen);

    for (i = 0; i + UT_COMMAND_LONG_FRAME_LEN <= (size_t)CapLen; i += UT_COMMAND_LONG_FRAME_LEN)
    {
        UtAssert_True(OutBuf[i] == 0xFD, "frame starts with STX_V2");
        UtAssert_True(OutBuf[i + 7] == (uint8)UT_COMMAND_LONG_MSGID, "frame msgid == COMMAND_LONG");
        FrameCount++;
    }
    UtAssert_INT32_EQ(FrameCount, 6);
}

/* TargetSystemId==0(아직 FC heartbeat 미수신) -> 요청 생략, 아무 것도 전송하지 않음 */
void Test_RequestTelemetryStreams_SkippedWhenNoTarget(void)
{
    int     Sv[2];
    uint8   OutBuf[64];
    ssize_t CapLen;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[1], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd       = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId = 0;

    MAVLINK_BRIDGE_APP_RequestTelemetryStreams();

    CapLen = read(Sv[1], OutBuf, sizeof(OutBuf));
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    UtAssert_True(CapLen <= 0, "no frame written when TargetSystemId==0");
}

/* ground_controllable_capability_plan P1-a: PARSER_RESET/SERIAL_RECONNECT
 * cross-app 트리거. Parser/SerialFd 자체는 파일 static이라 직접 관측
 * 불가 — 관측 가능한 CmdCounter 증가와 크래시 없이 완주하는 것으로 검증
 * (SerialFd 재오픈은 테스트 환경에 실장치가 없어 open() 실패가 정상). */
void Test_ProcessParserResetCmd_IncrementsCmdCounter(void)
{
    MAVLINK_BRIDGE_APP_ParserResetCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    MAVLINK_BRIDGE_APP_Data.CmdCounter = 5;

    MAVLINK_BRIDGE_APP_ProcessParserResetCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 6);
}

void Test_ProcessSerialReconnectCmd_ClosesFdAndIncrementsCmdCounter(void)
{
    MAVLINK_BRIDGE_APP_SerialReconnectCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    MAVLINK_BRIDGE_APP_Data.CmdCounter = 5;
    MAVLINK_BRIDGE_APP_Data.SerialFd   = -1;

    MAVLINK_BRIDGE_APP_ProcessSerialReconnectCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.CmdCounter, 6);
    /* OpenSerial()은 테스트 환경에 실장치가 없어 실패 → SerialFd -1 유지 */
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.SerialFd, -1);
}

/* -----------------------------------------------------------------------
 * BL-41 route: FC 미션 readback — mavlink spec §10 재정의(2026-07-23) 계약
 * 검증. 트리거 3종(CONNECTED 전이/업로드 완료/MISSION_QUERY 기존), 완료 시
 * FC_MISSION_READBACK_MID(0x1914) 게시, timeout 지수 백오프(1→2→4→5s 상한)
 * 무한 재시도. 테스트가 요구하는 신규 인터페이스(TDD red):
 *  - Data 필드: MissionReadbackPending / MissionReadbackBackoffMs /
 *    MissionReadbackNextRetryMs / FcMissionReadbackTlm(ROUTE_UPDATE_TLM_t)
 *  - MID: MAVLINK_BRIDGE_APP_FC_MISSION_READBACK_MID (0x1914)
 *  - SetLinkState 엣지 트리거 / MISSION_ACK 후 자동 재조회 / ServiceSerial
 *    재시도 발화
 * ----------------------------------------------------------------------- */

#define UT_MISSION_REQUEST_LIST_MSGID 43U
#define UT_MISSION_COUNT_MSGID        44U
#define UT_MISSION_COUNT_CRC_EXTRA    221U
#define UT_MISSION_ACK_MSGID          47U
#define UT_MISSION_ACK_CRC_EXTRA      153U
#define UT_MISSION_ITEM_INT_CRC_EXTRA 38U

#define UT_READBACK_BACKOFF_INITIAL_MS 1000U
#define UT_READBACK_BACKOFF_CAP_MS     5000U

/* CFE_TIME_GetTime가 SecondsxMs를 반환하도록 설정 */
static void UT_SetFakeTimeMs(uint32 Ms)
{
    static CFE_TIME_SysTime_t FakeTime;

    FakeTime.Seconds    = Ms / 1000U;
    FakeTime.Subseconds = (uint32)(((uint64)(Ms % 1000U) << 32) / 1000U);
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);
}

/* SerialFd를 socketpair로 걸어두고 Fn 호출 동안 나간 바이트를 캡처 */
static size_t UT_CaptureTxDuring(void (*Fn)(void), uint8 *OutBuf, size_t OutBufLen)
{
    int     Sv[2];
    ssize_t Rc;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[0], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd       = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId = 1;

    Fn();

    Rc = read(Sv[1], OutBuf, OutBufLen);
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    return (Rc > 0) ? (size_t)Rc : 0;
}

static void UT_CallSetLinkStateConnected(void)
{
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
}

/* 트리거 1: DISCONNECTED→CONNECTED 엣지에서 readback 자동 시작 */
void Test_SetLinkState_ConnectedEdge_StartsReadback(void)
{
    uint8        OutBuf[256];
    size_t       CapLen;
    uint8        PayloadLen;
    const uint8 *Frame;

    MAVLINK_BRIDGE_APP_Data.LinkState            = (uint8)MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs = 5000U; /* 전이 시 리셋돼야 함 */
    MAVLINK_BRIDGE_APP_Data.MissionReadbackPending   = 1U;

    CapLen = UT_CaptureTxDuring(UT_CallSetLinkStateConnected, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs, UT_READBACK_BACKOFF_INITIAL_MS);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackPending, 0);
    Frame = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_REQUEST_LIST_MSGID, &PayloadLen);
    UtAssert_True(Frame != NULL, "MISSION_REQUEST_LIST sent on CONNECTED edge");
}

/* 트리거 1 억제: 업로드 진행 중이면 readback 시작 안 함 */
void Test_SetLinkState_ConnectedEdge_SkipWhenUploadActive(void)
{
    MAVLINK_BRIDGE_APP_Data.LinkState            = (uint8)MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;

    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
}

/* 트리거 1은 엣지에서만: CONNECTED→CONNECTED 재호출은 무동작 */
void Test_SetLinkState_NoEdge_NoReadback(void)
{
    MAVLINK_BRIDGE_APP_Data.LinkState            = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;

    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
}

/* DISCONNECTED 전이 시 대기 중 재시도 취소 */
void Test_SetLinkState_Disconnect_CancelsPendingRetry(void)
{
    MAVLINK_BRIDGE_APP_Data.LinkState              = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackPending = 1U;

    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_DISCONNECTED);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackPending, 0);
}

/* 트리거 2: MISSION_ACK ACCEPTED(업로드 완료) 직후 readback 자동 시작 */
void Test_UploadComplete_TriggersReadback(void)
{
    uint8        AckPayload[3];
    uint8        AckFrame[24];
    uint8        OutBuf[512];
    size_t       AckLen;
    size_t       CapLen;
    uint8        PayloadLen;
    const uint8 *Frame;

    MAVLINK_BRIDGE_APP_Data.LinkState            = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState   = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount = 1U;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionPendingLatE7[0] = 1;
    MAVLINK_BRIDGE_APP_Data.MissionPendingLonE7[0] = 2;
    MAVLINK_BRIDGE_APP_Data.MissionPendingZ[0]     = 3.0f;

    memset(AckPayload, 0, sizeof(AckPayload)); /* [2]=0 → MAV_MISSION_ACCEPTED */
    AckLen = UT_BuildMavFrameGeneric(AckFrame, (uint8)UT_MISSION_ACK_MSGID,
                                     (uint8)UT_MISSION_ACK_CRC_EXTRA, AckPayload, sizeof(AckPayload));

    CapLen = UT_FeedSerialCaptureTx(AckFrame, AckLen, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionUploadState,
                      (int32)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT);
    Frame = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_REQUEST_LIST_MSGID, &PayloadLen);
    UtAssert_True(Frame != NULL, "MISSION_REQUEST_LIST sent after upload complete");
}

/* 다운로드 완료 → 항목 그대로(절대좌표) 버퍼링 + 0x1914 게시 (트리거 3 공통 완료 경로).
 * BL-56(2026-07-25): 로컬 역변환이 없어져 수신값이 그대로 캐시에 반영되는지 검증. */
void Test_DownloadComplete_PublishesReadbackMid(void)
{
    uint8  CountPayload[2];
    uint8  CountFrame[24];
    uint8  ItemPayload[38];
    uint8  ItemFrame[64];
    uint8  OutBuf[512];
    size_t Len;
    int32  ExpLatE7 = 123456780, ExpLonE7 = 987654320;
    float  ExpZ = 5.0f, ExpP1 = 7.5f;
    uint32 TxCountBefore;
    uint32 FloatBits;

    MAVLINK_BRIDGE_APP_Data.LinkState                = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 0x40000000U;
    memset(&MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm, 0,
           sizeof(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm));

    /* MISSION_COUNT: 1개 */
    UT_WriteU16LE(&CountPayload[0], 1U);
    Len = UT_BuildMavFrameGeneric(CountFrame, (uint8)UT_MISSION_COUNT_MSGID,
                                  (uint8)UT_MISSION_COUNT_CRC_EXTRA, CountPayload, sizeof(CountPayload));
    (void)UT_FeedSerialCaptureTx(CountFrame, Len, OutBuf, sizeof(OutBuf));
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_ITEM);

    memset(ItemPayload, 0, sizeof(ItemPayload));
    memcpy(&FloatBits, &ExpP1, sizeof(FloatBits));
    UT_WriteU32LE(&ItemPayload[0], FloatBits); /* Param1 */
    UT_WriteU32LE(&ItemPayload[16], (uint32)ExpLatE7);
    UT_WriteU32LE(&ItemPayload[20], (uint32)ExpLonE7);
    memcpy(&FloatBits, &ExpZ, sizeof(FloatBits));
    UT_WriteU32LE(&ItemPayload[24], FloatBits);
    UT_WriteU16LE(&ItemPayload[28], 0U);  /* seq */
    UT_WriteU16LE(&ItemPayload[30], 16U); /* NAV_WAYPOINT */

    TxCountBefore = UT_GetStubCount(UT_KEY(CFE_SB_TransmitMsg));
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 0x40000000U;
    Len = UT_BuildMavFrameGeneric(ItemFrame, (uint8)UT_MISSION_ITEM_INT_MSGID,
                                  (uint8)UT_MISSION_ITEM_INT_CRC_EXTRA, ItemPayload, sizeof(ItemPayload));
    (void)UT_FeedSerialCaptureTx(ItemFrame, Len, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.WaypointCount, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.RouteType, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.SourceSequence, 0);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.Waypoints[0].LatE7, ExpLatE7);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.Waypoints[0].LonE7, ExpLonE7);
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.Waypoints[0].Z == ExpZ, "wp Z passthrough");
    UtAssert_True(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.Waypoints[0].Param1 == ExpP1,
                  "wp Param1 passthrough");
    UtAssert_INT32_EQ((int32)MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.Waypoints[0].CmdType, 16);
    UtAssert_True(UT_GetStubCount(UT_KEY(CFE_SB_TransmitMsg)) > TxCountBefore,
                  "FC_MISSION_READBACK_MID published on download complete");
}

/* 16개 초과 미션은 16개로 클램프 (버퍼 오버플로 없이 완주) */
void Test_DownloadCount_ClampedToMax(void)
{
    /* BL-70(2026-07-28): ROUTE_MAX_WAYPOINTS 16->37 확장 — 클램프 경계값도
     * 심볼릭 상수 기준으로 검증(하드코딩된 16 대신 MAX+여유분 항목 전송). */
    uint8  CountPayload[2];
    uint8  CountFrame[24];
    uint8  ItemPayload[38];
    uint8  ItemFrame[64];
    uint8  OutBuf[512];
    size_t Len;
    uint8  i;
    uint32 FloatBits;
    float  Alt = 1.0f;
    const uint16 SendCount = (uint16)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS + 3U;

    UT_SetGpsReference(100000000, 1000000000);

    MAVLINK_BRIDGE_APP_Data.LinkState                = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 0x40000000U;

    UT_WriteU16LE(&CountPayload[0], SendCount);
    Len = UT_BuildMavFrameGeneric(CountFrame, (uint8)UT_MISSION_COUNT_MSGID,
                                  (uint8)UT_MISSION_COUNT_CRC_EXTRA, CountPayload, sizeof(CountPayload));
    (void)UT_FeedSerialCaptureTx(CountFrame, Len, OutBuf, sizeof(OutBuf));

    memset(ItemPayload, 0, sizeof(ItemPayload));
    memcpy(&FloatBits, &Alt, sizeof(FloatBits));
    UT_WriteU32LE(&ItemPayload[24], FloatBits);
    UT_WriteU16LE(&ItemPayload[30], 16U);

    for (i = 0; i < (uint8)SendCount; i++)
    {
        UT_WriteU16LE(&ItemPayload[28], (uint16)i);
        MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 0x40000000U;
        Len = UT_BuildMavFrameGeneric(ItemFrame, (uint8)UT_MISSION_ITEM_INT_MSGID,
                                      (uint8)UT_MISSION_ITEM_INT_CRC_EXTRA, ItemPayload, sizeof(ItemPayload));
        (void)UT_FeedSerialCaptureTx(ItemFrame, Len, OutBuf, sizeof(OutBuf));
    }

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.FcMissionReadbackTlm.WaypointCount,
                      (int32)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS);
}

/* timeout → IDLE + 백오프 재시도 예약(간격 2배) */
void Test_DownloadTimeout_SchedulesBackoffRetry(void)
{
    uint8 OutBuf[256];

    UT_SetFakeTimeMs(10000U);
    MAVLINK_BRIDGE_APP_Data.LinkState                 = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState      = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs  = 1000U; /* 이미 지남 */
    MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs  = UT_READBACK_BACKOFF_INITIAL_MS;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackPending    = 0U;

    (void)UT_FeedSerialCaptureTx(NULL, 0, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackPending, 1);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackNextRetryMs, 10000U + UT_READBACK_BACKOFF_INITIAL_MS);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs, 2U * UT_READBACK_BACKOFF_INITIAL_MS);
}

/* 백오프 2배 증가, 5s 상한 고정 */
void Test_BackoffDoubles_To5sCap(void)
{
    uint8 OutBuf[256];

    /* 4000 → 5000 (2배=8000이지만 상한 클램프) */
    UT_SetFakeTimeMs(10000U);
    MAVLINK_BRIDGE_APP_Data.LinkState                = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 1000U;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs = 4000U;
    (void)UT_FeedSerialCaptureTx(NULL, 0, OutBuf, sizeof(OutBuf));
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs, UT_READBACK_BACKOFF_CAP_MS);

    /* 5000 → 5000 (상한 유지) */
    UT_SetFakeTimeMs(20000U);
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs = 11000U;
    (void)UT_FeedSerialCaptureTx(NULL, 0, OutBuf, sizeof(OutBuf));
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs, UT_READBACK_BACKOFF_CAP_MS);
}

/* 예약 시각 도달 시 ServiceSerial이 재시도 발화 */
void Test_RetryFires_WhenDue(void)
{
    uint8        OutBuf[512];
    size_t       CapLen;
    uint8        PayloadLen;
    const uint8 *Frame;

    UT_SetFakeTimeMs(10000U);
    MAVLINK_BRIDGE_APP_Data.LinkState                 = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState        = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState      = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackPending    = 1U;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackNextRetryMs = 5000U; /* 이미 지남 */

    CapLen = UT_FeedSerialCaptureTx(NULL, 0, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackPending, 0);
    Frame = UT_FindMavFrame(OutBuf, CapLen, (uint8)UT_MISSION_REQUEST_LIST_MSGID, &PayloadLen);
    UtAssert_True(Frame != NULL, "MISSION_REQUEST_LIST sent on retry");
}

/* 예약 시각 미도달이면 발화 안 함 */
void Test_RetryHolds_BeforeDue(void)
{
    uint8 OutBuf[256];

    UT_SetFakeTimeMs(3000U);
    MAVLINK_BRIDGE_APP_Data.LinkState                  = (uint8)MAVLINK_BRIDGE_LINK_CONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState         = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState       = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackPending     = 1U;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackNextRetryMs = 5000U; /* 아직 안 됨 */

    (void)UT_FeedSerialCaptureTx(NULL, 0, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionDownloadState,
                      (int32)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackPending, 1);
}

/* 재연결이 백오프를 초기값으로 리셋 (Test 1과 조합해 전체 사이클 커버) */
void Test_ReconnectResetsBackoff(void)
{
    uint8 OutBuf[256];

    MAVLINK_BRIDGE_APP_Data.LinkState                = (uint8)MAVLINK_BRIDGE_LINK_DISCONNECTED;
    MAVLINK_BRIDGE_APP_Data.MissionUploadState       = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs = UT_READBACK_BACKOFF_CAP_MS;

    (void)UT_CaptureTxDuring(UT_CallSetLinkStateConnected, OutBuf, sizeof(OutBuf));

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.MissionReadbackBackoffMs, UT_READBACK_BACKOFF_INITIAL_MS);
}

/* -----------------------------------------------------------------------
 * BL-44(2026-07-24, §18.4.6.8.1): flight mode base 명령 — PX4 DO_SET_MODE
 * (+WAYPOINT는 MISSION_SET_CURRENT) 송신.
 * ----------------------------------------------------------------------- */
#define UT_MISSION_SET_CURRENT_MSGID     41U
#define UT_MISSION_SET_CURRENT_FRAME_LEN 16U /* 10(header) + 4(payload) + 2(crc) */

/* HOVER: DO_SET_MODE 1프레임만, custom_mode=(AUTO<<16)|(LOITER<<24) */
/* BL-74(2026-07-28) 회귀: param2=custom_main_mode(u8을 float로, offset 14),
 * param3=custom_sub_mode(offset 18) — PX4 Commander가 실제로 읽는 필드.
 * HEARTBEAT.custom_mode 패킹 형식을 param2 하나에 우겨넣던 이전 인코딩은
 * PX4가 무효 모드로 판정해 MAV_RESULT_DENIED를 반환하는 원인이었다. */
void Test_ProcessSetFlightModeCmd_Hover(void)
{
    MAVLINK_BRIDGE_APP_SetFlightModeCmd_t Cmd;
    int                                   Sv[2];
    uint8                                 OutBuf[128];
    ssize_t                               CapLen;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[1], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd          = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = 1;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 1;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.SourceSequence = 50;
    Cmd.FlightMode      = 0; /* HOVER */

    MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(&Cmd);

    CapLen = read(Sv[1], OutBuf, sizeof(OutBuf));
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    UtAssert_True(CapLen == (ssize_t)UT_COMMAND_LONG_FRAME_LEN,
                  "DO_SET_MODE 1프레임만 전송 (got %ld bytes)", (long)CapLen);
    UtAssert_True(OutBuf[7] == (uint8)UT_COMMAND_LONG_MSGID, "msgid == COMMAND_LONG");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[14]) == 4.0f, "param2(custom_main_mode) == AUTO(4)");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[18]) == 3.0f, "param3(custom_sub_mode) == AUTO_LOITER(3)");
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence, 50);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

/* WAYPOINT: DO_SET_MODE + MISSION_SET_CURRENT 2프레임, param2=AUTO(4)/param3=MISSION(4) */
void Test_ProcessSetFlightModeCmd_Waypoint(void)
{
    MAVLINK_BRIDGE_APP_SetFlightModeCmd_t Cmd;
    int                                   Sv[2];
    uint8                                 OutBuf[128];
    ssize_t                               CapLen;
    uint16                                SeqEcho;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[1], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd          = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = 1;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 1;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.SourceSequence      = 51;
    Cmd.FlightMode          = 1; /* WAYPOINT */
    Cmd.WaypointStartIndex  = 7;

    MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(&Cmd);

    CapLen = read(Sv[1], OutBuf, sizeof(OutBuf));
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    UtAssert_True(CapLen == (ssize_t)(UT_COMMAND_LONG_FRAME_LEN + UT_MISSION_SET_CURRENT_FRAME_LEN),
                  "DO_SET_MODE + MISSION_SET_CURRENT 2프레임 (got %ld bytes)", (long)CapLen);

    UtAssert_True(OutBuf[7] == (uint8)UT_COMMAND_LONG_MSGID, "1st frame msgid == COMMAND_LONG");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[14]) == 4.0f, "param2(custom_main_mode) == AUTO(4)");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[18]) == 4.0f, "param3(custom_sub_mode) == AUTO_MISSION(4)");

    UtAssert_True(OutBuf[UT_COMMAND_LONG_FRAME_LEN + 7] == (uint8)UT_MISSION_SET_CURRENT_MSGID,
                  "2nd frame msgid == MISSION_SET_CURRENT");
    SeqEcho = (uint16)UT_ReadU32LE(&OutBuf[UT_COMMAND_LONG_FRAME_LEN + 10]) & 0xFFFFU;
    UtAssert_INT32_EQ((int)SeqEcho, 7);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_OK);
}

/* LAND: DO_SET_MODE 1프레임만, param2=AUTO(4)/param3=LAND(6) */
void Test_ProcessSetFlightModeCmd_Land(void)
{
    MAVLINK_BRIDGE_APP_SetFlightModeCmd_t Cmd;
    int                                   Sv[2];
    uint8                                 OutBuf[128];
    ssize_t                               CapLen;

    UtAssert_INT32_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, Sv), 0);
    UtAssert_INT32_EQ(fcntl(Sv[1], F_SETFL, O_NONBLOCK), 0);

    MAVLINK_BRIDGE_APP_Data.SerialFd          = Sv[0];
    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = 1;
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = 1;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.SourceSequence = 52;
    Cmd.FlightMode      = 2; /* LAND */

    MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(&Cmd);

    CapLen = read(Sv[1], OutBuf, sizeof(OutBuf));
    close(Sv[0]);
    close(Sv[1]);
    MAVLINK_BRIDGE_APP_Data.SerialFd = -1;

    UtAssert_True(CapLen == (ssize_t)UT_COMMAND_LONG_FRAME_LEN, "DO_SET_MODE 1프레임만");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[14]) == 4.0f, "param2(custom_main_mode) == AUTO(4)");
    UtAssert_True(UT_ReadFloatLE(&OutBuf[18]) == 6.0f, "param3(custom_sub_mode) == AUTO_LAND(6)");
}

/* 잘못된 flight_mode(>2) → 전송 없음, EXEC_RESULT FAILED */
void Test_ProcessSetFlightModeCmd_InvalidMode(void)
{
    MAVLINK_BRIDGE_APP_SetFlightModeCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.SourceSequence = 53;
    Cmd.FlightMode      = 3; /* invalid */

    MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult = (uint8)EXEC_RESULT_GENERIC_OK; /* 이전 값과 구분 */

    MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.SourceSequence, 53);
    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
}

/* COMMAND_LONG 전송 실패(SerialFd 닫힘) → EXEC_RESULT FAILED */
void Test_ProcessSetFlightModeCmd_SendFails(void)
{
    MAVLINK_BRIDGE_APP_SetFlightModeCmd_t Cmd;

    memset(&Cmd, 0, sizeof(Cmd));
    Cmd.SourceSequence = 54;
    Cmd.FlightMode      = 0; /* HOVER */

    MAVLINK_BRIDGE_APP_Data.SerialFd = -1; /* 닫힌 fd -> write 실패 */

    MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(&Cmd);

    UtAssert_INT32_EQ(MAVLINK_BRIDGE_APP_Data.ExecResultTlm.GenericResult, (int32)EXEC_RESULT_GENERIC_FAILED);
}

void UtTest_Setup(void)
{
    ADD_TEST(ProcessSetFlightModeCmd_Hover);
    ADD_TEST(ProcessSetFlightModeCmd_Waypoint);
    ADD_TEST(ProcessSetFlightModeCmd_Land);
    ADD_TEST(ProcessSetFlightModeCmd_InvalidMode);
    ADD_TEST(ProcessSetFlightModeCmd_SendFails);
    ADD_TEST(UpdateFromHeartbeat_Armed);
    ADD_TEST(UpdateFromHeartbeat_Disarmed);
    ADD_TEST(UpdateFromHeartbeat_OtherBitsIgnored);
    ADD_TEST(UpdateFromHeartbeat_StateTransition);
    ADD_TEST(UpdateFromHeartbeat_SystemStatus);
    ADD_TEST(StartMissionUpload_AllowedWhenArmed);
    ADD_TEST(StartMissionUpload_AllowedWhenDisarmed);
    ADD_TEST(StartMissionUpload_LinkNotConnected);
    ADD_TEST(StartMissionUpload_IgnoredWhenReadbackInProgress);
    ADD_TEST(StartMissionUpload_Replace);
    ADD_TEST(StartMissionUpload_Add);
    ADD_TEST(StartMissionUpload_AddTruncated);
    ADD_TEST(StartMissionUpload_Add_Uint8OverflowClampedNotWrapped);
    ADD_TEST(StartMissionUpload_Delete);
    ADD_TEST(StartMissionUpload_Delete_IndexOutOfRange);
    ADD_TEST(StartMissionUpload_Delete_RejectedWhenTargetIsActiveResumeIndex);
    ADD_TEST(StartMissionUpload_Modify_FullRecordOverwrite);
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
    ADD_TEST(LoadState_NoFile);
    ADD_TEST(SaveState_NoDir);
    ADD_TEST(SaveLoadState_RoundTrip);
    ADD_TEST(LoadState_Truncated);
    ADD_TEST(LoadState_BadMagic);
    ADD_TEST(LoadState_ConfigVersionMismatch);
    ADD_TEST(LoadState_ChecksumMismatch);
    ADD_TEST(LoadState_OpenErrorNotEnoent);
    ADD_TEST(SaveState_WriteFail);
    ADD_TEST(SaveState_RenameFail);
    ADD_TEST(ProcessConfigCommand_PersistsOnSuccess);
    ADD_TEST(SaveState_DirFsync_NoSlashInPath);
    ADD_TEST(SaveState_DirFsync_ParentOpenFail);
    ADD_TEST(ReportHousekeeping);
    ADD_TEST(VerifyCmdLength_Pass);
    ADD_TEST(VerifyCmdLength_Fail);
    ADD_TEST(SetLinkState_Connected);
    ADD_TEST(SetLinkState_Disconnected);
    ADD_TEST(SetLinkState_ConnectedEdge_StartsReadback);
    ADD_TEST(SetLinkState_ConnectedEdge_SkipWhenUploadActive);
    ADD_TEST(SetLinkState_NoEdge_NoReadback);
    ADD_TEST(SetLinkState_Disconnect_CancelsPendingRetry);
    ADD_TEST(UploadComplete_TriggersReadback);
    ADD_TEST(DownloadComplete_PublishesReadbackMid);
    ADD_TEST(DownloadCount_ClampedToMax);
    ADD_TEST(DownloadTimeout_SchedulesBackoffRetry);
    ADD_TEST(BackoffDoubles_To5sCap);
    ADD_TEST(RetryFires_WhenDue);
    ADD_TEST(RetryHolds_BeforeDue);
    ADD_TEST(ReconnectResetsBackoff);
    ADD_TEST(RecordParseError);
    ADD_TEST(RecordParseError_Cumulative);
    ADD_TEST(Heartbeat_ValidCrc_LocksTargetAndConnects);
    ADD_TEST(Heartbeat_BadCrc_RejectedNoLockNoConnect);
    ADD_TEST(CommandAck_FieldOffsetCorrect);
    ADD_TEST(CommandAck_BaseOnly3ByteAccepted);
    ADD_TEST(SysTime_FullPayload);
    ADD_TEST(SysTime_TrimmedPayload);
    ADD_TEST(SysTime_ZeroClockIgnored);
    ADD_TEST(SysTime_CrcFail);
    ADD_TEST(SendMissionItemInt_GlobalRelativeAltFrame);
    ADD_TEST(SendMissionItemInt_CurrentFlagOnlyOnActiveResumeIndex);
    ADD_TEST(MissionCurrent_UpdatesActiveResumeIndex);
    ADD_TEST(PublishAttitude_NaNRejected);
    ADD_TEST(PublishEkfLocal_InfRejected);
    ADD_TEST(PublishAttitude_FiniteValuesAccepted);
    ADD_TEST(PublishAttitude_TrimmedYawspeedZero_Accepted);
    ADD_TEST(PublishEkfLocal_TrimmedVzZero_Accepted);
    ADD_TEST(PublishGlobalPositionAsLocal_TrimmedHdgZero_Accepted);
    ADD_TEST(ProcessReceivedByte_StxByteInPayload_NoReentry);
    ADD_TEST(RequestTelemetryStreams_SendsSixStreamRequests);
    ADD_TEST(RequestTelemetryStreams_SkippedWhenNoTarget);
    ADD_TEST(ProcessParserResetCmd_IncrementsCmdCounter);
    ADD_TEST(ProcessSerialReconnectCmd_ClosesFdAndIncrementsCmdCounter);
}

#ifndef UPLINK_APP_H
#define UPLINK_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "uplink_app_mission_cfg.h"
#include "uplink_app_platform_cfg.h"
#include "uplink_app_internal_cfg.h"

#include "uplink_app_perfids.h"
#include "uplink_app_msg.h"
#include "uplink_app_msgids.h"

typedef struct
{
    uint8                  CmdCounter;
    uint8                  ErrCounter;
    uint16                 Reserved;
    uint32                 RunStatus;
    uint32                 SequenceCounter;
    uint32                 PublishCount;
    uint32                 LastPublishTimeMs;
    uint32                 LastRxTimeMs;
    uint32                 AcceptedCount;
    uint32                 RejectedCount;
    uint32                 DuplicateCount;
    uint32                 RoutingFailureCount;
    uint32                 LastAcceptedSequence;
    uint8                  BootCount; /* BL-12(2026-07-21): 부팅마다 +1, 8비트 wrap, /cf에 영속 */
    /* BL-43(2026-07-23): 부팅/오류 영속화 — "생존 마커" 재부팅 루프 감지 (spec §12.3) */
    uint8                  LastResetReason;  /* 이번 부팅의 PSP reset type */
    uint8                  SurvivedMark;     /* 이번 세션 120s 생존 여부 (파일 기록값) */
    uint8                  PrevSurvivedMark; /* 직전 세션 마커 (LoadState 복원) */
    uint8                  ShortBootStreak;  /* 연속 단명 부팅 횟수 */
    uint8                  BootLoopSuspect;  /* streak>=임계 → 1 (보고 전용) */
    uint32                 BootStartMs;      /* BL-43: 앱 시작 시각 (상대 uptime 기준점 — Pi에서 CFE_TIME이 0에서 시작 안 함) */
    uint16                 LastCommandCode;
    uint16                 LastRxSequence;
    uint8                  LastCommandResult;
    uint8                  LastRouteTarget;
    uint8                  LinkState;
    uint8                  Valid;
    uint8                  ActiveTransportId;
    uint8                  ConfigPendingState;
    uint8                  LastConfigResult;
    uint8                  LastRollbackReason;
    uint8                  CfsHealthState;
    uint8                  CfsHealthReceived;
    uint8                  FcMissionResult;
    uint8                  FcMissionUploadState;
    uint32                 FcMissionUploadSuccessCount;
    UPLINK_APP_FlightModeCtrlCmd_t FlightModeCtrlCmd; /* BL-44(2026-07-24): mavlink_bridge CMD_MID로 보낼 SET_FLIGHT_MODE_CC 캐시 (P1-a 패턴) */
    CFE_SB_PipeId_t        CommandPipe;
    UPLINK_APP_HkTlm_t     HkTlm;
    UPLINK_APP_StatusTlm_t StatusTlm;
} UPLINK_APP_Data_t;

extern UPLINK_APP_Data_t UPLINK_APP_Data;

void         UPLINK_APP_Main(void);
CFE_Status_t UPLINK_APP_Init(void);
void         UPLINK_APP_ReportHousekeeping(void);
bool         UPLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         UPLINK_APP_ServicePrototype(void);
void         UPLINK_APP_ProcessUplink(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ValidateProxyCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd, UPLINK_APP_Result_t *Result);
UPLINK_APP_RouteTarget_t UPLINK_APP_ResolveRouteTarget(uint8 CommandClass);
bool         UPLINK_APP_ParseRouteUpdatePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                                UPLINK_APP_RouteUpdatePayload_t *Payload);
bool         UPLINK_APP_PublishRouteUpdate(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                           const UPLINK_APP_RouteUpdatePayload_t *Payload);
bool         UPLINK_APP_ForwardRecoveryCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ParseViewpointPayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                              UPLINK_APP_ViewpointPayload_t *Payload);
bool         UPLINK_APP_ForwardViewpointCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                                const UPLINK_APP_ViewpointPayload_t *Payload);
bool         UPLINK_APP_ForwardConfigCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ForwardModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ForwardDiagnosticCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ForwardCounterMgmtCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd);
bool         UPLINK_APP_ParseFlightModePayload(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                               UPLINK_APP_FlightModePayload_t *Payload);
bool         UPLINK_APP_ForwardFlightModeCommand(const UPLINK_APP_ProcessUplinkCmd_t *Cmd,
                                                 const UPLINK_APP_FlightModePayload_t *Payload);
void         UPLINK_APP_LoadState(void);
void         UPLINK_APP_SaveState(void);

#endif


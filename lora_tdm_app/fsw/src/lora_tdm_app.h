#ifndef LORA_TDM_APP_H
#define LORA_TDM_APP_H

#include "cfe.h"
#include "lora_tdm_app_msg.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app_mission_cfg.h"
#include "lora_tdm_app_topicid_values.h"

/* FC state cache — updated from SB messages */
typedef struct
{
    float    RollRad;
    float    PitchRad;
    float    YawRad;
    float    PosX;
    float    PosY;
    float    PosZ;
    float    VelX;
    float    VelY;
    float    VelZ;
    int32    LatE7;
    int32    LonE7;
    int32    AltMm;
    uint8    GpsFix;
    uint8    SatellitesVisible;
    uint8    AttitudeValid;
    uint8    LocalValid;
    uint8    GpsValid;
    uint8    EkfValid;
    uint32   TimestampMs;
    uint64   TimeUnixUsec; /* GPS 기반 UNIX epoch (us), FC_SYS_TIME_MID 경유 */
    uint8    TimeValid;
} LORA_TDM_APP_FcStateCache_t;

typedef struct
{
    uint8   SystemHealthState;
    uint8   FaultCode;
    uint32  TimestampMs;
} LORA_TDM_APP_SystemHealthCache_t;

typedef struct
{
    /* Counters */
    uint8  CmdCounter;
    uint8  ErrCounter;
    uint8  LinkState;
    uint8  PacketType;

    /* TDM state */
    uint32 DownlinkSeq;
    uint32 LastSentSeq; /* 마지막으로 전송에 성공한 DownlinkSeq(증가 전) — ACK SeqEcho 비교 기준 */
    uint32 TxCount;
    uint32 RxAckCount;
    uint32 RxCmdCount;
    uint16 RxErrorCount;
    uint16 NoAckCount;
    uint16 SeqFailCount;
    uint32 LastAckTimestampMs;
    uint8  PendingUplinkFeedback;
    uint8  LinkStateInitialized; /* BL-04: 첫 관측(부팅 직후)은 전이 이벤트 대상 아님 */

    /* BL-03(2026-07-22): UPLINK_STATUS_MID에서 캐시 — DL2 프레임에 동봉해
     * 지상 자가복구(seq)/재부팅 감지(boot_count)에 사용. uplink_app이
     * 한 번도 상태를 보고하지 않았으면(부팅 직후) 0으로 유지. */
    uint16 UplinkLastAcceptedSequence;
    uint8  UplinkBootCount;

    /* Serial fd */
    int    LoRaFd;

    /* 다운링크 송신 프로토콜 선택 — 0=v1(ASCII, 기본값), 1=v2(DL2 바이너리).
     * §8 단계적 전환: CONFIG 명령(SET_DOWNLINK_PROTO_CC — lora_tdm_app_cmds.c,
     * CONFIG_CMD_MID의 PARAM_DOWNLINK_PROTOCOL — lora_tdm_app_utils.c)으로
     * 런타임 전환 가능. 부팅 기본값만 v1(memset(0)). */
    uint8  UseV2Downlink;

    /* RX line buffer — RX창(RunRxWindow) 호출 경계를 넘어 유지된다.
     * [[lora_tdm_serial_reopen_gap]] 관련 §11.1: 이전엔 RunRxWindow() 지역 변수라
     * 완성 안 된 줄(개행 미도달)이 창 경계에서 유실됐음 — 여기로 옮겨서 다음 창까지 보존. */
    char   RxLineBuf[LORA_TDM_APP_LINE_BUF_LEN];
    uint16 RxLineBufLen;

    /* RX 프레임 모드 — 첫 바이트로 v1(ASCII 줄) vs v2(매직바이트) 판별 §11.2/§8.
     * LORA_TDM_RxFrameMode_t 값 저장(uint8로 보관, enum 크기 이식성 회피). */
    uint8  RxFrameMode;
    uint16 RxFrameTargetLen; /* v2 모드에서 이 길이(바이트)까지 모이면 프레임 완성 */

    /* SB */
    uint32           RunStatus;
    CFE_SB_PipeId_t  CommandPipe;

    /* Message cache */
    LORA_TDM_APP_FcStateCache_t       FcState;
    LORA_TDM_APP_SystemHealthCache_t  SystemHealth;

    /* Telemetry messages */
    LORA_TDM_APP_HkTlm_t         HkTlm;
    LORA_TDM_APP_LinkStatusTlm_t LinkStatusTlm;
    LORA_TDM_APP_ExecResultTlm_t ExecResultTlm; /* BL-08(2026-07-22) */
} LORA_TDM_APP_Data_t;

/* Singleton global */
extern LORA_TDM_APP_Data_t LORA_TDM_APP_Data;

/* Entry point */
void LORA_TDM_APP_Main(void);

/* App-level functions */
CFE_Status_t LORA_TDM_APP_Init(void);
void         LORA_TDM_APP_RunCycle(void);
void         LORA_TDM_APP_ReportHousekeeping(void);
void         LORA_TDM_APP_ReportLinkStatus(void);

#endif

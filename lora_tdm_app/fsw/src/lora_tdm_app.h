#ifndef LORA_TDM_APP_H
#define LORA_TDM_APP_H

#include "cfe.h"
#include "lora_tdm_app_msg.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app_mission_cfg.h"
#include "default_lora_tdm_app_mission_cfg.h"

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
    uint8    AttitudeValid;
    uint8    LocalValid;
    uint8    GpsValid;
    uint8    EkfValid;
    uint32   TimestampMs;
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
    uint32 TxCount;
    uint32 RxAckCount;
    uint32 RxCmdCount;
    uint16 RxErrorCount;
    uint16 NoAckCount;
    uint32 LastAckTimestampMs;
    uint8  PendingUplinkFeedback;
    uint8  Reserved[3];

    /* Serial fd */
    int    LoRaFd;

    /* SB */
    uint32           RunStatus;
    CFE_SB_PipeId_t  CommandPipe;

    /* Message cache */
    LORA_TDM_APP_FcStateCache_t       FcState;
    LORA_TDM_APP_SystemHealthCache_t  SystemHealth;

    /* Telemetry messages */
    LORA_TDM_APP_HkTlm_t         HkTlm;
    LORA_TDM_APP_LinkStatusTlm_t LinkStatusTlm;
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

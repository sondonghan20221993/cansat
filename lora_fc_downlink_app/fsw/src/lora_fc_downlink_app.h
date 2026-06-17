/************************************************************************
 * LoRa FC Downlink App
 ************************************************************************/

#ifndef LORA_FC_DOWNLINK_APP_H
#define LORA_FC_DOWNLINK_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "lora_fc_downlink_app_mission_cfg.h"
#include "lora_fc_downlink_app_platform_cfg.h"
#include "lora_fc_downlink_app_perfids.h"
#include "lora_fc_downlink_app_msg.h"
#include "lora_fc_downlink_app_msgids.h"

typedef struct
{
    uint8                           CmdCounter;
    uint8                           ErrCounter;
    uint16                          Reserved;
    uint32                          RunStatus;
    uint32                          DownlinkSequence;
    uint32                          DownlinkCount;
    uint32                          LastAttitudeTimestampMs;
    uint32                          LastLocalTimestampMs;
    uint32                          LastGpsTimestampMs;
    uint32                          LastEkfTimestampMs;
    uint32                          LastSystemHealthTimestampMs;
    int                             LoRaFd;
    uint32                          LoRaTxCount;
    uint32                          LastLoRaTxMs;
    uint32                          HbLastRxMs;
    uint8                           HbLinkValid;
    char                            LoRaReadBuf[256];
    uint8                           LoRaReadLen;
    float                           AttitudeRollRad;
    float                           AttitudePitchRad;
    float                           AttitudeYawRad;
    float                           LocalX_m;
    float                           LocalY_m;
    float                           LocalZ_m;
    float                           LocalVx_mps;
    float                           LocalVy_mps;
    float                           LocalVz_mps;
    int32                           GpsLatE7;
    int32                           GpsLonE7;
    int32                           GpsAltMm;
    uint8                           GpsFixType;
    uint8                           SystemHealthFaultCode;
    uint8                           PacketType;
    uint8                           AttitudeValid;
    uint8                           LocalValid;
    uint8                           GpsValid;
    uint8                           EkfValid;
    uint8                           SystemHealthState;
    uint8                           Spare[1];
    CFE_SB_PipeId_t                 CommandPipe;
    LORA_FC_DOWNLINK_APP_HkTlm_t     HkTlm;
    LORA_FC_DOWNLINK_APP_UplinkRawTlm_t UplinkRawMsg;
} LORA_FC_DOWNLINK_APP_Data_t;

extern LORA_FC_DOWNLINK_APP_Data_t LORA_FC_DOWNLINK_APP_Data;

void         LORA_FC_DL_Main(void); /* entry symbol <=19 chars for OS_SymbolLookup */
CFE_Status_t LORA_FC_DOWNLINK_APP_Init(void);
bool         LORA_FC_DOWNLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *msg_ptr, size_t expected_length);
void         LORA_FC_DOWNLINK_APP_ReportHousekeeping(void);
void         LORA_FC_DOWNLINK_APP_ProcessInputMessage(const CFE_SB_Buffer_t *sb_buf_ptr);

#endif

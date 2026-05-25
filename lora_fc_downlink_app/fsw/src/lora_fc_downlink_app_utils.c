#include "lora_fc_downlink_app_utils.h"
#include "lora_fc_downlink_app.h"

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} LORA_FC_DOWNLINK_APP_GenericStateTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastValidInputTimestampMs;
    uint8                     HealthState;
    uint8                     FaultCode;
    uint8                     RecoveryRequested;
    uint8                     Reserved;
} LORA_FC_DOWNLINK_APP_SystemHealthMirror_t;

void LORA_FC_DOWNLINK_APP_ReportHousekeeping(void)
{
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.CommandCounter            = LORA_FC_DOWNLINK_APP_Data.CmdCounter;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.CommandErrorCounter       = LORA_FC_DOWNLINK_APP_Data.ErrCounter;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.DownlinkCount             = LORA_FC_DOWNLINK_APP_Data.DownlinkCount;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LastAttitudeTimestampMs   = LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LastLocalTimestampMs      = LORA_FC_DOWNLINK_APP_Data.LastLocalTimestampMs;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LastGpsTimestampMs        = LORA_FC_DOWNLINK_APP_Data.LastGpsTimestampMs;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LastEkfTimestampMs        = LORA_FC_DOWNLINK_APP_Data.LastEkfTimestampMs;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LastSystemHealthTimestampMs =
        LORA_FC_DOWNLINK_APP_Data.LastSystemHealthTimestampMs;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.AttitudeValid     = LORA_FC_DOWNLINK_APP_Data.AttitudeValid;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LocalValid        = LORA_FC_DOWNLINK_APP_Data.LocalValid;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.GpsValid          = LORA_FC_DOWNLINK_APP_Data.GpsValid;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.EkfValid          = LORA_FC_DOWNLINK_APP_Data.EkfValid;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.SystemHealthState = LORA_FC_DOWNLINK_APP_Data.SystemHealthState;
    LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.PacketType        = LORA_FC_DOWNLINK_APP_Data.PacketType;

    CFE_SB_TransmitMsg(CFE_MSG_PTR(LORA_FC_DOWNLINK_APP_Data.HkTlm.TelemetryHeader), true);
}

void LORA_FC_DOWNLINK_APP_ProcessInputMessage(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    CFE_SB_MsgId_t msg_id = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&sb_buf_ptr->Msg, &msg_id);

    if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.AttitudeValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.PacketType              = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastLocalTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.LocalValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.PacketType           = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastGpsTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.GpsValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.PacketType         = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastEkfTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.EkfValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.PacketType         = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_SystemHealthMirror_t *Msg =
            (const LORA_FC_DOWNLINK_APP_SystemHealthMirror_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastSystemHealthTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.SystemHealthState           = Msg->HealthState;
        LORA_FC_DOWNLINK_APP_Data.PacketType                  = LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_PACKET_TYPE;
    }

    LORA_FC_DOWNLINK_APP_Data.DownlinkCount++;
}

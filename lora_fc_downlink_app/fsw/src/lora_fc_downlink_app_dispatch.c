/************************************************************************
 * LoRa FC Downlink App dispatch
 ************************************************************************/

#include "lora_fc_downlink_app_dispatch.h"
#include "lora_fc_downlink_app.h"
#include "lora_fc_downlink_app_cmds.h"
#include "lora_fc_downlink_app_eventids.h"
#include "lora_fc_downlink_app_msg.h"

bool LORA_FC_DOWNLINK_APP_VerifyCmdLength(const CFE_MSG_Message_t *msg_ptr, size_t expected_length)
{
    bool              result        = true;
    size_t            actual_length = 0;
    CFE_SB_MsgId_t    msg_id        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t fcn_code      = 0;

    CFE_MSG_GetSize(msg_ptr, &actual_length);

    if (expected_length != actual_length)
    {
        CFE_MSG_GetMsgId(msg_ptr, &msg_id);
        CFE_MSG_GetFcnCode(msg_ptr, &fcn_code);

        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_CMD_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X, CC = %u, Len = %u, Expected = %u",
                          (unsigned int)CFE_SB_MsgIdToValue(msg_id), (unsigned int)fcn_code,
                          (unsigned int)actual_length, (unsigned int)expected_length);
        result = false;
        LORA_FC_DOWNLINK_APP_Data.ErrCounter++;
    }

    return result;
}

static void LORA_FC_DOWNLINK_APP_ProcessGroundCommand(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    CFE_MSG_FcnCode_t command_code = 0;

    CFE_MSG_GetFcnCode(&sb_buf_ptr->Msg, &command_code);

    switch (command_code)
    {
        case LORA_FC_DOWNLINK_APP_NOOP_CC:
            if (LORA_FC_DOWNLINK_APP_VerifyCmdLength(&sb_buf_ptr->Msg, sizeof(LORA_FC_DOWNLINK_APP_NoopCmd_t)))
            {
                LORA_FC_DOWNLINK_APP_NoopCmd((const LORA_FC_DOWNLINK_APP_NoopCmd_t *)sb_buf_ptr);
            }
            break;

        case LORA_FC_DOWNLINK_APP_RESET_COUNTERS_CC:
            if (LORA_FC_DOWNLINK_APP_VerifyCmdLength(&sb_buf_ptr->Msg, sizeof(LORA_FC_DOWNLINK_APP_ResetCountersCmd_t)))
            {
                LORA_FC_DOWNLINK_APP_ResetCountersCmd((const LORA_FC_DOWNLINK_APP_ResetCountersCmd_t *)sb_buf_ptr);
            }
            break;

        default:
            CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_CC_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid ground command code: CC = %d", command_code);
            break;
    }
}

void LORA_FC_DOWNLINK_APP_TaskPipe(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    static CFE_SB_MsgId_t cmd_mid        = CFE_SB_MSGID_RESERVED;
    static CFE_SB_MsgId_t send_hk_mid    = CFE_SB_MSGID_RESERVED;
    CFE_SB_MsgId_t        msg_id         = CFE_SB_INVALID_MSG_ID;

    if (!CFE_SB_IsValidMsgId(cmd_mid))
    {
        cmd_mid     = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_CMD_MID);
        send_hk_mid = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SEND_HK_MID);
    }

    CFE_MSG_GetMsgId(&sb_buf_ptr->Msg, &msg_id);

    if (CFE_SB_MsgId_Equal(msg_id, send_hk_mid))
    {
        LORA_FC_DOWNLINK_APP_ReportHousekeeping();
    }
    else if (CFE_SB_MsgId_Equal(msg_id, cmd_mid))
    {
        LORA_FC_DOWNLINK_APP_ProcessGroundCommand(sb_buf_ptr);
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE ||
             CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE ||
             CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE ||
             CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE ||
             CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_MID_VALUE)
    {
        LORA_FC_DOWNLINK_APP_ProcessInputMessage(sb_buf_ptr);
    }
    else
    {
        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LoRa FC Downlink App: invalid command packet, MID = 0x%x",
                          (unsigned int)CFE_SB_MsgIdToValue(msg_id));
    }
}

#include "mavlink_bridge_app_dispatch.h"
#include "mavlink_bridge_app_cmds.h"
#include "mavlink_bridge_app_utils.h"
#include "mavlink_bridge_app_eventids.h"
#include "mavlink_bridge_app_fcncodes.h"
#include "mavlink_bridge_app_msgids.h"
#include "mavlink_bridge_app_msg.h"

void MAVLINK_BRIDGE_APP_TaskPipe(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t     MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t  CommandCode;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE)))
    {
        MAVLINK_BRIDGE_APP_ReportHousekeeping();
        return;
    }

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(ROUTE_UPDATE_MID)))
    {
        MAVLINK_BRIDGE_APP_StartMissionUpload((const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *)SBBufPtr);
        return;
    }

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(CONFIG_CMD_MID)))
    {
        MAVLINK_BRIDGE_APP_ProcessConfigCommand((const MAVLINK_BRIDGE_APP_ConfigCmdTlm_t *)SBBufPtr);
        return;
    }

    if (!CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(MAVLINK_BRIDGE_APP_CMD_MID_VALUE)))
    {
        MAVLINK_BRIDGE_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: Invalid command MID");
        return;
    }

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    switch (CommandCode)
    {
        case MAVLINK_BRIDGE_APP_NOOP_CC:
            MAVLINK_BRIDGE_APP_Noop((const MAVLINK_BRIDGE_APP_NoopCmd_t *)SBBufPtr);
            break;

        case MAVLINK_BRIDGE_APP_RESET_COUNTERS_CC:
            MAVLINK_BRIDGE_APP_ResetCountersCmd((const MAVLINK_BRIDGE_APP_ResetCountersCmd_t *)SBBufPtr);
            break;

        case MAVLINK_BRIDGE_APP_MISSION_QUERY_CC:
            MAVLINK_BRIDGE_APP_MissionQuery((const MAVLINK_BRIDGE_APP_MissionQueryCmd_t *)SBBufPtr);
            break;

        case MAVLINK_BRIDGE_APP_PARSER_RESET_CC:
            MAVLINK_BRIDGE_APP_ProcessParserResetCmd((const MAVLINK_BRIDGE_APP_ParserResetCmd_t *)SBBufPtr);
            break;

        case MAVLINK_BRIDGE_APP_SERIAL_RECONNECT_CC:
            MAVLINK_BRIDGE_APP_ProcessSerialReconnectCmd((const MAVLINK_BRIDGE_APP_SerialReconnectCmd_t *)SBBufPtr);
            break;

        default:
            MAVLINK_BRIDGE_APP_Data.ErrCounter++;
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "MAVLINK_BRIDGE_APP: Invalid command code %u", (unsigned int)CommandCode);
            break;
    }
}

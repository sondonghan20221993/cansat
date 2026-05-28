#include "mavlink_bridge_app_utils.h"
#include "mavlink_bridge_app_eventids.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAVLINK_STX_V1               0xFE
#define MAVLINK_STX_V2               0xFD
#define MAVLINK_MSG_ID_HEARTBEAT               0U
#define MAVLINK_MSG_ID_SYS_TIME                2U
#define MAVLINK_MSG_ID_GPS_RAW_INT            24U
#define MAVLINK_MSG_ID_ATTITUDE               30U
#define MAVLINK_MSG_ID_LOCAL_POSITION_NED     32U
#define MAVLINK_MSG_ID_GLOBAL_POSITION_INT    33U
#define MAVLINK_MSG_ID_COMMAND_LONG           76U
#define MAVLINK_MSG_ID_COMMAND_ACK            77U
#define MAVLINK_MSG_ID_TIMESYNC              111U
#define MAVLINK_MSG_ID_EKF_STATUS_REPORT     193U
#define MAVLINK_MSG_ID_COMMAND_LONG_LEN       33U
#define MAVLINK_MSG_ID_HEARTBEAT_LEN           9U
#define MAVLINK_GPS_RAW_INT_PAYLOAD_LEN       30U
#define MAVLINK_GPS_RAW_INT_CRC_EXTRA         24U
#define MAVLINK_LOCAL_POSITION_NED_PAYLOAD_LEN 28U
#define MAVLINK_LOCAL_POSITION_NED_CRC_EXTRA   185U
#define MAVLINK_GLOBAL_POSITION_INT_PAYLOAD_LEN 28U
#define MAVLINK_GLOBAL_POSITION_INT_CRC_EXTRA   104U
#define MAVLINK_ATTITUDE_PAYLOAD_LEN          28U
#define MAVLINK_ATTITUDE_CRC_EXTRA            39U
#define MAVLINK_EKF_STATUS_MIN_PAYLOAD_LEN    21U
#define MAVLINK_EKF_STATUS_CRC_EXTRA          71U
#define MAVLINK_HEARTBEAT_CRC_EXTRA           50U
#define MAVLINK_COMMAND_LONG_CRC_EXTRA       152U
#define MAVLINK_COMMAND_ACK_CRC_EXTRA        143U
#define MAVLINK_MAX_PAYLOAD_LEN              255U
#define MAVLINK_MAX_FRAME_LEN                64U
#define MAVLINK_BRIDGE_APP_SYSTEM_ID         255U
#define MAVLINK_BRIDGE_APP_COMPONENT_ID      190U
#define MAVLINK_CMD_SET_MESSAGE_INTERVAL     511U
#define MAVLINK_TYPE_ONBOARD_CONTROLLER      18U
#define MAVLINK_AUTOPILOT_INVALID             8U
#define MAVLINK_RESULT_ACCEPTED               0U

#define MAVLINK_MSG_ID_MISSION_COUNT           44U
#define MAVLINK_MSG_ID_MISSION_REQUEST_INT     51U
#define MAVLINK_MSG_ID_MISSION_ITEM_INT        73U
#define MAVLINK_MSG_ID_MISSION_ACK             47U
#define MAVLINK_MISSION_COUNT_CRC_EXTRA       221U
#define MAVLINK_MISSION_REQUEST_INT_CRC_EXTRA 196U
#define MAVLINK_MISSION_ITEM_INT_CRC_EXTRA     38U
#define MAVLINK_MISSION_ACK_CRC_EXTRA         153U
#define MAVLINK_MAV_FRAME_LOCAL_NED             1U
#define MAVLINK_MAV_CMD_NAV_WAYPOINT           16U
#define MAVLINK_MISSION_TYPE_MISSION            0U
#define MAVLINK_MISSION_ACCEPTED                0U
#define MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS   2000U
#define MAVLINK_BRIDGE_APP_MISSION_MAX_RETRIES         3U
#define MAVLINK_MSG_ID_MISSION_REQUEST_LIST            43U
#define MAVLINK_MISSION_REQUEST_LIST_CRC_EXTRA         132U
#define MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_TIMEOUT_MS 3000U
#define MAVLINK_MSG_ID_MISSION_REQUEST                 40U
#define MAVLINK_MSG_ID_MISSION_ITEM                    39U
#define MAVLINK_MSG_ID_MISSION_CLEAR_ALL               45U
#define MAVLINK_MISSION_REQUEST_CRC_EXTRA             230U
#define MAVLINK_MISSION_CLEAR_ALL_CRC_EXTRA           232U
#define MAVLINK_MISSION_ITEM_CRC_EXTRA                254U

typedef enum
{
    MAVLINK_PARSE_WAIT_STX = 0,
    MAVLINK_PARSE_GOT_LENGTH,
    MAVLINK_PARSE_GOT_INCOMPAT,
    MAVLINK_PARSE_GOT_COMPAT,
    MAVLINK_PARSE_GOT_SEQ,
    MAVLINK_PARSE_GOT_SYSID,
    MAVLINK_PARSE_GOT_COMPID,
    MAVLINK_PARSE_GOT_MSGID1,
    MAVLINK_PARSE_GOT_MSGID2,
    MAVLINK_PARSE_GOT_MSGID3,
    MAVLINK_PARSE_READING_PAYLOAD,
    MAVLINK_PARSE_GOT_CRC1,
    MAVLINK_PARSE_GOT_CRC2
} MAVLINK_BRIDGE_APP_ParseState_t;

typedef struct
{
    MAVLINK_BRIDGE_APP_ParseState_t State;
    uint8                           IsV2;
    uint8                           PayloadLen;
    uint8                           PayloadIndex;
    uint8                           Seq;
    uint8                           SysId;
    uint8                           CompId;
    uint32                          MsgId;
    uint8                           CrcLow;
    uint8                           CrcHigh;
    uint8                           Payload[MAVLINK_MAX_PAYLOAD_LEN];
} MAVLINK_BRIDGE_APP_ParserContext_t;

static MAVLINK_BRIDGE_APP_ParserContext_t MAVLINK_BRIDGE_APP_Parser;
static uint8 MAVLINK_BRIDGE_APP_TxSequence;

static bool MAVLINK_BRIDGE_APP_ShouldLogDecoded(uint32 Sequence)
{
    return (Sequence <= 3U) || ((Sequence % 25U) == 0U);
}

static uint32 MAVLINK_BRIDGE_APP_GetTimeMs(void)
{
    CFE_TIME_SysTime_t TimeNow = CFE_TIME_GetTime();
    uint64             TimeMs;

    TimeMs = ((uint64)TimeNow.Seconds * 1000ULL) + ((uint64)TimeNow.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)TimeMs;
}

static void MAVLINK_BRIDGE_APP_ResetParser(void)
{
    memset(&MAVLINK_BRIDGE_APP_Parser, 0, sizeof(MAVLINK_BRIDGE_APP_Parser));
    MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_WAIT_STX;
}

static void MAVLINK_BRIDGE_APP_CrcAccumulate(uint8 Data, uint16 *Crc)
{
    uint8 Tmp;

    Tmp  = Data ^ (uint8)(*Crc & 0xFFU);
    Tmp ^= (uint8)(Tmp << 4);
    *Crc = (*Crc >> 8) ^ ((uint16)Tmp << 8) ^ ((uint16)Tmp << 3) ^ ((uint16)Tmp >> 4);
}

static uint16 MAVLINK_BRIDGE_APP_ComputeFrameCrc(const MAVLINK_BRIDGE_APP_ParserContext_t *Parser, uint8 CrcExtra)
{
    uint16 Crc = 0xFFFFU;
    uint8  Index;

    MAVLINK_BRIDGE_APP_CrcAccumulate(Parser->PayloadLen, &Crc);

    if (Parser->IsV2)
    {
        MAVLINK_BRIDGE_APP_CrcAccumulate(0, &Crc);
        MAVLINK_BRIDGE_APP_CrcAccumulate(0, &Crc);
    }

    MAVLINK_BRIDGE_APP_CrcAccumulate(Parser->Seq, &Crc);
    MAVLINK_BRIDGE_APP_CrcAccumulate(Parser->SysId, &Crc);
    MAVLINK_BRIDGE_APP_CrcAccumulate(Parser->CompId, &Crc);
    MAVLINK_BRIDGE_APP_CrcAccumulate((uint8)(Parser->MsgId & 0xFFU), &Crc);

    if (Parser->IsV2)
    {
        MAVLINK_BRIDGE_APP_CrcAccumulate((uint8)((Parser->MsgId >> 8) & 0xFFU), &Crc);
        MAVLINK_BRIDGE_APP_CrcAccumulate((uint8)((Parser->MsgId >> 16) & 0xFFU), &Crc);
    }

    for (Index = 0; Index < Parser->PayloadLen; ++Index)
    {
        MAVLINK_BRIDGE_APP_CrcAccumulate(Parser->Payload[Index], &Crc);
    }

    MAVLINK_BRIDGE_APP_CrcAccumulate(CrcExtra, &Crc);
    return Crc;
}

static uint32 MAVLINK_BRIDGE_APP_ReadU32LE(const uint8 *Data)
{
    return ((uint32)Data[0]) |
           ((uint32)Data[1] << 8) |
           ((uint32)Data[2] << 16) |
           ((uint32)Data[3] << 24);
}

static uint16 MAVLINK_BRIDGE_APP_ReadU16LE(const uint8 *Data)
{
    return (uint16)(((uint16)Data[0]) | ((uint16)Data[1] << 8));
}

static uint64 MAVLINK_BRIDGE_APP_ReadU64LE(const uint8 *Data)
{
    return ((uint64)Data[0]) |
           ((uint64)Data[1] << 8) |
           ((uint64)Data[2] << 16) |
           ((uint64)Data[3] << 24) |
           ((uint64)Data[4] << 32) |
           ((uint64)Data[5] << 40) |
           ((uint64)Data[6] << 48) |
           ((uint64)Data[7] << 56);
}

static int32 MAVLINK_BRIDGE_APP_ReadI32LE(const uint8 *Data)
{
    return (int32)MAVLINK_BRIDGE_APP_ReadU32LE(Data);
}

static float MAVLINK_BRIDGE_APP_ReadFloatLE(const uint8 *Data)
{
    uint32 RawValue;
    float  Value;

    RawValue = MAVLINK_BRIDGE_APP_ReadU32LE(Data);
    memcpy(&Value, &RawValue, sizeof(Value));
    return Value;
}

static void MAVLINK_BRIDGE_APP_CloseSerial(void)
{
    if (MAVLINK_BRIDGE_APP_Data.SerialFd >= 0)
    {
        close(MAVLINK_BRIDGE_APP_Data.SerialFd);
        MAVLINK_BRIDGE_APP_Data.SerialFd = -1;
    }
}

static void MAVLINK_BRIDGE_APP_CloseLoRa(void)
{
    if (MAVLINK_BRIDGE_APP_Data.LoRaFd >= 0)
    {
        close(MAVLINK_BRIDGE_APP_Data.LoRaFd);
        MAVLINK_BRIDGE_APP_Data.LoRaFd = -1;
    }
}

static void MAVLINK_BRIDGE_APP_WriteU16LE(uint8 *Data, uint16 Value)
{
    Data[0] = (uint8)(Value & 0xFFU);
    Data[1] = (uint8)((Value >> 8) & 0xFFU);
}

static void MAVLINK_BRIDGE_APP_WriteU32LE(uint8 *Data, uint32 Value)
{
    Data[0] = (uint8)(Value & 0xFFU);
    Data[1] = (uint8)((Value >> 8) & 0xFFU);
    Data[2] = (uint8)((Value >> 16) & 0xFFU);
    Data[3] = (uint8)((Value >> 24) & 0xFFU);
}

static void MAVLINK_BRIDGE_APP_WriteFloatLE(uint8 *Data, float Value)
{
    uint32 RawValue;

    memcpy(&RawValue, &Value, sizeof(RawValue));
    MAVLINK_BRIDGE_APP_WriteU32LE(Data, RawValue);
}

static void MAVLINK_BRIDGE_APP_RecordLengthError(uint32 MsgId, uint8 ActualLength, uint8 ExpectedLength)
{
    MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_INVALID_VALUE;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount++;
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: invalid payload msgid=%lu actual=%u expected=%u",
                      (unsigned long)MsgId,
                      (unsigned int)ActualLength,
                      (unsigned int)ExpectedLength);
}

static CFE_Status_t MAVLINK_BRIDGE_APP_SendMavlinkV2(uint32 MsgId, const uint8 *Payload, uint8 PayloadLen, uint8 CrcExtra)
{
    uint8  Frame[MAVLINK_MAX_FRAME_LEN];
    size_t FrameLen;
    uint16 Crc;
    uint8  Index;
    ssize_t WriteRc;

    if (MAVLINK_BRIDGE_APP_Data.SerialFd < 0)
    {
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    if (PayloadLen > MAVLINK_MAX_PAYLOAD_LEN || (10U + PayloadLen + 2U) > sizeof(Frame))
    {
        return CFE_ES_BAD_ARGUMENT;
    }

    Frame[0] = MAVLINK_STX_V2;
    Frame[1] = PayloadLen;
    Frame[2] = 0;
    Frame[3] = 0;
    Frame[4] = MAVLINK_BRIDGE_APP_TxSequence++;
    Frame[5] = MAVLINK_BRIDGE_APP_SYSTEM_ID;
    Frame[6] = MAVLINK_BRIDGE_APP_COMPONENT_ID;
    Frame[7] = (uint8)(MsgId & 0xFFU);
    Frame[8] = (uint8)((MsgId >> 8) & 0xFFU);
    Frame[9] = (uint8)((MsgId >> 16) & 0xFFU);

    if (PayloadLen > 0U && Payload != NULL)
    {
        memcpy(&Frame[10], Payload, PayloadLen);
    }

    Crc = 0xFFFFU;
    for (Index = 1; Index < (uint8)(10U + PayloadLen); ++Index)
    {
        MAVLINK_BRIDGE_APP_CrcAccumulate(Frame[Index], &Crc);
    }
    MAVLINK_BRIDGE_APP_CrcAccumulate(CrcExtra, &Crc);

    Frame[10U + PayloadLen] = (uint8)(Crc & 0xFFU);
    Frame[11U + PayloadLen] = (uint8)((Crc >> 8) & 0xFFU);
    FrameLen = 12U + PayloadLen;

    WriteRc = write(MAVLINK_BRIDGE_APP_Data.SerialFd, Frame, FrameLen);
    if (WriteRc != (ssize_t)FrameLen)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: MAVLink tx failed msgid=%lu errno=%d",
                          (unsigned long)MsgId, errno);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    return CFE_SUCCESS;
}

static void MAVLINK_BRIDGE_APP_SendMissionClearAll(void)
{
    uint8 Payload[3];

    Payload[0] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[1] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[2] = (uint8)MAVLINK_MISSION_TYPE_MISSION;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_CLEAR_ALL, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_CLEAR_ALL_CRC_EXTRA);
}

static void MAVLINK_BRIDGE_APP_SendMissionCount(uint8 WpCount)
{
    uint8 Payload[5];

    memset(Payload, 0, sizeof(Payload));
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[0], (uint16)WpCount);
    Payload[2] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[3] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[4] = (uint8)MAVLINK_MISSION_TYPE_MISSION;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_COUNT, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_COUNT_CRC_EXTRA);
}

static void MAVLINK_BRIDGE_APP_SendMissionItemInt(uint8 Seq)
{
    uint8 Payload[38];
    int32 XE4;
    int32 YE4;

    memset(Payload, 0, sizeof(Payload));

    XE4 = (int32)(MAVLINK_BRIDGE_APP_Data.MissionPendingX[Seq] * 10000.0f);
    YE4 = (int32)(MAVLINK_BRIDGE_APP_Data.MissionPendingY[Seq] * 10000.0f);

    MAVLINK_BRIDGE_APP_WriteU32LE(&Payload[16], (uint32)XE4);
    MAVLINK_BRIDGE_APP_WriteU32LE(&Payload[20], (uint32)YE4);
    /* Route payload uses altitude-positive convention; LOCAL_NED z is down, so negate */
    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[24], -MAVLINK_BRIDGE_APP_Data.MissionPendingZ[Seq]);
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[28], (uint16)Seq);
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[30], (uint16)MAVLINK_MAV_CMD_NAV_WAYPOINT);
    Payload[32] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[33] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[34] = (uint8)MAVLINK_MAV_FRAME_LOCAL_NED;
    Payload[36] = 1U; /* autocontinue */
    Payload[37] = (uint8)MAVLINK_MISSION_TYPE_MISSION;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_ITEM_INT, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_ITEM_INT_CRC_EXTRA);
}

static void MAVLINK_BRIDGE_APP_SendMissionItem(uint8 Seq)
{
    uint8 Payload[37];

    memset(Payload, 0, sizeof(Payload));

    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[16], MAVLINK_BRIDGE_APP_Data.MissionPendingX[Seq]);
    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[20], MAVLINK_BRIDGE_APP_Data.MissionPendingY[Seq]);
    /* Route payload uses altitude-positive convention; LOCAL_NED z is down, so negate */
    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[24], -MAVLINK_BRIDGE_APP_Data.MissionPendingZ[Seq]);
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[28], (uint16)Seq);
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[30], (uint16)MAVLINK_MAV_CMD_NAV_WAYPOINT);
    Payload[32] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[33] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[34] = (uint8)MAVLINK_MAV_FRAME_LOCAL_NED;
    Payload[36] = 1U; /* autocontinue */

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_ITEM, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_ITEM_CRC_EXTRA);
}

void MAVLINK_BRIDGE_APP_StartMissionUpload(const MAVLINK_BRIDGE_APP_RouteUpdateMirror_t *Msg)
{
    uint8 i;

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: StartMissionUpload called wp=%u link=%u",
                      (unsigned int)Msg->WaypointCount,
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.LinkState);

    if (MAVLINK_BRIDGE_APP_Data.LinkState != (uint8)MAVLINK_BRIDGE_LINK_CONNECTED)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: route update ignored - FC link not connected");
        return;
    }

    MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount = Msg->WaypointCount;
    if (MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount > (uint8)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS)
    {
        MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount = (uint8)MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS;
    }

    for (i = 0U; i < MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount; i++)
    {
        MAVLINK_BRIDGE_APP_Data.MissionPendingX[i] = Msg->Waypoints[i].X;
        MAVLINK_BRIDGE_APP_Data.MissionPendingY[i] = Msg->Waypoints[i].Y;
        MAVLINK_BRIDGE_APP_Data.MissionPendingZ[i] = Msg->Waypoints[i].Z;
    }

    MAVLINK_BRIDGE_APP_Data.MissionUploadState     = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE;
    MAVLINK_BRIDGE_APP_Data.MissionUploadRetry     = 0U;
    MAVLINK_BRIDGE_APP_Data.MissionUploadTimeoutMs =
        MAVLINK_BRIDGE_APP_GetTimeMs() + MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS;

    MAVLINK_BRIDGE_APP_SendMissionClearAll();
    MAVLINK_BRIDGE_APP_SendMissionCount(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount);
}

static void MAVLINK_BRIDGE_APP_CheckMissionUploadTimeout(uint32 NowMs)
{
    if (MAVLINK_BRIDGE_APP_Data.MissionUploadState != (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE)
    {
        return;
    }

    if ((int32)(NowMs - MAVLINK_BRIDGE_APP_Data.MissionUploadTimeoutMs) < 0)
    {
        return;
    }

    if (MAVLINK_BRIDGE_APP_Data.MissionUploadRetry < (uint8)MAVLINK_BRIDGE_APP_MISSION_MAX_RETRIES)
    {
        MAVLINK_BRIDGE_APP_Data.MissionUploadRetry++;
        MAVLINK_BRIDGE_APP_Data.MissionUploadTimeoutMs = NowMs + MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS;
        MAVLINK_BRIDGE_APP_SendMissionCount(MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount);
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: mission upload timeout retry=%u",
                          (unsigned int)MAVLINK_BRIDGE_APP_Data.MissionUploadRetry);
    }
    else
    {
        MAVLINK_BRIDGE_APP_Data.MissionUploadState = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
        MAVLINK_BRIDGE_APP_Data.MissionUploadFailCount++;
        MAVLINK_BRIDGE_APP_Data.LastUploadResult   = (uint8)MAVLINK_BRIDGE_UPLOAD_RESULT_TIMEOUT;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: mission upload failed after %u retries",
                          (unsigned int)MAVLINK_BRIDGE_APP_MISSION_MAX_RETRIES);
    }
}

static void MAVLINK_BRIDGE_APP_SendMissionRequestList(void)
{
    uint8 Payload[3];

    memset(Payload, 0, sizeof(Payload));
    Payload[0] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[1] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[2] = (uint8)MAVLINK_MISSION_TYPE_MISSION;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_REQUEST_LIST, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_REQUEST_LIST_CRC_EXTRA);
}

static void MAVLINK_BRIDGE_APP_SendMissionRequestIntDownload(uint8 Seq)
{
    uint8 Payload[5];

    memset(Payload, 0, sizeof(Payload));
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[0], (uint16)Seq);
    Payload[2] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[3] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[4] = (uint8)MAVLINK_MISSION_TYPE_MISSION;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_REQUEST_INT, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_REQUEST_INT_CRC_EXTRA);
}

static void MAVLINK_BRIDGE_APP_SendMissionAckAccepted(void)
{
    uint8 Payload[3];

    Payload[0] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[1] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[2] = (uint8)MAVLINK_MISSION_ACCEPTED;

    MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_MISSION_ACK, Payload, sizeof(Payload),
                                     MAVLINK_MISSION_ACK_CRC_EXTRA);
}

void MAVLINK_BRIDGE_APP_MissionQuery(const MAVLINK_BRIDGE_APP_MissionQueryCmd_t *Cmd)
{
    if (!MAVLINK_BRIDGE_APP_VerifyCmdLength(&Cmd->CommandHeader.Msg, sizeof(*Cmd)))
    {
        return;
    }

    if (MAVLINK_BRIDGE_APP_Data.LinkState != (uint8)MAVLINK_BRIDGE_LINK_CONNECTED)
    {
        MAVLINK_BRIDGE_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: MISSION_QUERY ignored - FC link not connected");
        return;
    }

    MAVLINK_BRIDGE_APP_Data.MissionDownloadState         = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq           = 0U;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadExpectedCount = 0U;
    MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs     =
        MAVLINK_BRIDGE_APP_GetTimeMs() + MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_TIMEOUT_MS;
    MAVLINK_BRIDGE_APP_Data.CmdCounter++;

    MAVLINK_BRIDGE_APP_SendMissionRequestList();

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: MISSION_QUERY started");
}

static void MAVLINK_BRIDGE_APP_CheckMissionDownloadTimeout(uint32 NowMs)
{
    if (MAVLINK_BRIDGE_APP_Data.MissionDownloadState == (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE)
    {
        return;
    }

    if ((int32)(NowMs - MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs) < 0)
    {
        return;
    }

    MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                      "MAVLINK_BRIDGE_APP: MISSION_QUERY timeout (seq=%u expected=%u)",
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq,
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.MissionDownloadExpectedCount);
}

static void MAVLINK_BRIDGE_APP_SendCompanionHeartbeat(uint32 NowMs)
{
    uint8 Payload[MAVLINK_MSG_ID_HEARTBEAT_LEN];

    if ((NowMs - MAVLINK_BRIDGE_APP_Data.LastHeartbeatTxMs) < MAVLINK_BRIDGE_APP_HEARTBEAT_INTERVAL_MS)
    {
        return;
    }

    memset(Payload, 0, sizeof(Payload));
    Payload[4] = MAVLINK_TYPE_ONBOARD_CONTROLLER;
    Payload[5] = MAVLINK_AUTOPILOT_INVALID;
    Payload[8] = 3;

    if (MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_HEARTBEAT, Payload, sizeof(Payload), MAVLINK_HEARTBEAT_CRC_EXTRA) ==
        CFE_SUCCESS)
    {
        MAVLINK_BRIDGE_APP_Data.LastHeartbeatTxMs = NowMs;
    }
}

static CFE_Status_t MAVLINK_BRIDGE_APP_RequestMessageInterval(uint32 MsgId, uint32 IntervalUs)
{
    uint8 Payload[MAVLINK_MSG_ID_COMMAND_LONG_LEN];

    memset(Payload, 0, sizeof(Payload));
    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[0], (float)MsgId);
    MAVLINK_BRIDGE_APP_WriteFloatLE(&Payload[4], (float)IntervalUs);
    MAVLINK_BRIDGE_APP_WriteU16LE(&Payload[28], MAVLINK_CMD_SET_MESSAGE_INTERVAL);
    Payload[30] = MAVLINK_BRIDGE_APP_Data.TargetSystemId;
    Payload[31] = MAVLINK_BRIDGE_APP_Data.TargetComponentId;
    Payload[32] = 0;

    return MAVLINK_BRIDGE_APP_SendMavlinkV2(MAVLINK_MSG_ID_COMMAND_LONG, Payload, sizeof(Payload),
                                            MAVLINK_COMMAND_LONG_CRC_EXTRA);
}

void MAVLINK_BRIDGE_APP_RequestTelemetryStreams(void)
{
    CFE_Status_t Status;

    if (MAVLINK_BRIDGE_APP_Data.TargetSystemId == 0U)
    {
        return;
    }

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: requesting telemetry streams from sys=%u comp=%u",
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.TargetSystemId,
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.TargetComponentId);

    Status = MAVLINK_BRIDGE_APP_RequestMessageInterval(MAVLINK_MSG_ID_ATTITUDE, MAVLINK_BRIDGE_APP_ATTITUDE_INTERVAL_US);
    if (Status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: request failed msgid=%u status=0x%08lX",
                          (unsigned int)MAVLINK_MSG_ID_ATTITUDE,
                          (unsigned long)Status);
        return;
    }

    Status = MAVLINK_BRIDGE_APP_RequestMessageInterval(MAVLINK_MSG_ID_LOCAL_POSITION_NED,
                                                       MAVLINK_BRIDGE_APP_LOCAL_POSITION_INTERVAL_US);
    if (Status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: request failed msgid=%u status=0x%08lX",
                          (unsigned int)MAVLINK_MSG_ID_LOCAL_POSITION_NED,
                          (unsigned long)Status);
        return;
    }

    Status = MAVLINK_BRIDGE_APP_RequestMessageInterval(MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                                                       MAVLINK_BRIDGE_APP_GLOBAL_POSITION_INTERVAL_US);
    if (Status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: request failed msgid=%u status=0x%08lX",
                          (unsigned int)MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                          (unsigned long)Status);
        return;
    }

    Status = MAVLINK_BRIDGE_APP_RequestMessageInterval(MAVLINK_MSG_ID_GPS_RAW_INT, MAVLINK_BRIDGE_APP_GPS_RAW_INTERVAL_US);
    if (Status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: request failed msgid=%u status=0x%08lX",
                          (unsigned int)MAVLINK_MSG_ID_GPS_RAW_INT,
                          (unsigned long)Status);
        return;
    }

    Status = MAVLINK_BRIDGE_APP_RequestMessageInterval(MAVLINK_MSG_ID_EKF_STATUS_REPORT,
                                                       MAVLINK_BRIDGE_APP_EKF_STATUS_INTERVAL_US);
    if (Status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: request failed msgid=%u status=0x%08lX",
                          (unsigned int)MAVLINK_MSG_ID_EKF_STATUS_REPORT,
                          (unsigned long)Status);
        return;
    }

    MAVLINK_BRIDGE_APP_Data.LastStreamRequestMs  = MAVLINK_BRIDGE_APP_GetTimeMs();
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: requested telemetry streams from sys=%u comp=%u",
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.TargetSystemId,
                      (unsigned int)MAVLINK_BRIDGE_APP_Data.TargetComponentId);
}

static bool MAVLINK_BRIDGE_APP_GetBaudConstant(uint32 Baudrate, speed_t *BaudConstant)
{
    switch (Baudrate)
    {
        case 9600:
            *BaudConstant = B9600;
            return true;
        case 19200:
            *BaudConstant = B19200;
            return true;
        case 38400:
            *BaudConstant = B38400;
            return true;
        case 57600:
            *BaudConstant = B57600;
            return true;
        case 115200:
            *BaudConstant = B115200;
            return true;
        case 230400:
            *BaudConstant = B230400;
            return true;
        default:
            return false;
    }
}

static void MAVLINK_BRIDGE_APP_MarkOutputsStale(void)
{
    MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Stale  = 1;
    MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Stale  = 1;
    MAVLINK_BRIDGE_APP_Data.GpsRawTlm.Stale    = 1;
    MAVLINK_BRIDGE_APP_Data.EkfStatusTlm.Stale = 1;
}

static CFE_Status_t MAVLINK_BRIDGE_APP_OpenSerial(void)
{
    int            Fd;
    struct termios Tio;
    speed_t        BaudConstant;

    if (!MAVLINK_BRIDGE_APP_GetBaudConstant(MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE, &BaudConstant))
    {
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_INVALID_VALUE;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: unsupported baud=%lu",
                          (unsigned long)MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    Fd = open(MAVLINK_BRIDGE_APP_SERIAL_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (Fd < 0)
    {
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_SERIAL_OPEN_FAIL;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: open() failed path=%s errno=%d",
                          MAVLINK_BRIDGE_APP_SERIAL_PATH, errno);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    if (tcgetattr(Fd, &Tio) != 0)
    {
        int SavedErrno = errno;
        close(Fd);
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_SERIAL_OPEN_FAIL;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: tcgetattr() failed path=%s errno=%d",
                          MAVLINK_BRIDGE_APP_SERIAL_PATH, SavedErrno);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    Tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    Tio.c_oflag &= ~OPOST;
    Tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    Tio.c_cflag &= ~(CSIZE | PARENB);
    Tio.c_cflag |= CS8;
    cfsetispeed(&Tio, BaudConstant);
    cfsetospeed(&Tio, BaudConstant);
    Tio.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
    Tio.c_cflag &= ~CRTSCTS;
#endif
    Tio.c_cc[VMIN]  = 0;
    Tio.c_cc[VTIME] = 0;

    if (tcsetattr(Fd, TCSANOW, &Tio) != 0)
    {
        int SavedErrno = errno;
        close(Fd);
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_SERIAL_OPEN_FAIL;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: tcsetattr() failed path=%s baud=%lu errno=%d",
                          MAVLINK_BRIDGE_APP_SERIAL_PATH,
                          (unsigned long)MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE,
                          SavedErrno);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    MAVLINK_BRIDGE_APP_Data.SerialFd       = Fd;
    MAVLINK_BRIDGE_APP_Data.LastErrorCode  = MAVLINK_BRIDGE_ERROR_NONE;
    MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = 0;
    MAVLINK_BRIDGE_APP_Data.LastHeartbeatTxMs = 0;
    MAVLINK_BRIDGE_APP_Data.LastStreamRequestMs = 0;
    MAVLINK_BRIDGE_APP_Data.TargetDiscoveryStartMs = MAVLINK_BRIDGE_APP_GetTimeMs();
    MAVLINK_BRIDGE_APP_Data.LastAttitudeRxMs = 0;
    MAVLINK_BRIDGE_APP_Data.LastEkfLocalRxMs = 0;
    MAVLINK_BRIDGE_APP_Data.LastGpsRawRxMs = 0;
    MAVLINK_BRIDGE_APP_Data.LastEkfStatusRxMs = 0;
    MAVLINK_BRIDGE_APP_Data.StreamRequestPending = 1;
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
    MAVLINK_BRIDGE_APP_ResetParser();

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: opened serial path %s at %lu baud",
                      MAVLINK_BRIDGE_APP_SERIAL_PATH, (unsigned long)MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE);
    return CFE_SUCCESS;
}

static void MAVLINK_BRIDGE_APP_ServiceLoRa(void)
{
    int            Fd;
    int            WriteRc;
    struct termios Tio;
    speed_t        BaudConstant;
    char           Line[256];
    int            LineLen;

    if (!MAVLINK_BRIDGE_APP_Data.AttitudeTlm.Valid || !MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Valid)
    {
        return;
    }

    if (!MAVLINK_BRIDGE_APP_GetBaudConstant(MAVLINK_BRIDGE_APP_LORA_BAUDRATE, &BaudConstant))
    {
        return;
    }

    if (MAVLINK_BRIDGE_APP_Data.LoRaFd < 0)
    {
        Fd = open(MAVLINK_BRIDGE_APP_LORA_SERIAL_PATH, O_WRONLY | O_NOCTTY | O_NONBLOCK);
        if (Fd < 0)
        {
            return;
        }

        if (tcgetattr(Fd, &Tio) != 0)
        {
            close(Fd);
            return;
        }

        Tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        Tio.c_oflag &= ~OPOST;
        Tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        Tio.c_cflag &= ~(CSIZE | PARENB);
        Tio.c_cflag |= CS8;
        Tio.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
        Tio.c_cflag &= ~CRTSCTS;
#endif
        Tio.c_cc[VMIN]  = 0;
        Tio.c_cc[VTIME] = 0;
        cfsetispeed(&Tio, BaudConstant);
        cfsetospeed(&Tio, BaudConstant);

        if (tcsetattr(Fd, TCSANOW, &Tio) != 0)
        {
            close(Fd);
            return;
        }

        {
            int LoRaFlags = fcntl(Fd, F_GETFL, 0);
            if (LoRaFlags >= 0)
            {
                fcntl(Fd, F_SETFL, LoRaFlags & ~O_NONBLOCK);
            }
        }
        MAVLINK_BRIDGE_APP_Data.LoRaFd = Fd;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: opened LoRa path %s at %lu baud",
                          MAVLINK_BRIDGE_APP_LORA_SERIAL_PATH, (unsigned long)MAVLINK_BRIDGE_APP_LORA_BAUDRATE);
    }

    LineLen = snprintf(Line, sizeof(Line),
                       "FC,%lu,%lu,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                       (unsigned long)(++MAVLINK_BRIDGE_APP_Data.LoRaTxCount),
                       (unsigned long)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.TimestampMs,
                       (double)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.RollRad,
                       (double)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.PitchRad,
                       (double)MAVLINK_BRIDGE_APP_Data.AttitudeTlm.YawRad,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.X_m,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Y_m,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Z_m,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Vx_mps,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Vy_mps,
                       (double)MAVLINK_BRIDGE_APP_Data.EkfLocalTlm.Vz_mps);

    if (LineLen <= 0)
    {
        return;
    }

    WriteRc = (int)write(MAVLINK_BRIDGE_APP_Data.LoRaFd, Line, (size_t)LineLen);
    if (WriteRc < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            /* TX buffer transiently full - skip this packet, port remains open */
            return;
        }
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: LoRa write failed errno=%d, forcing reopen", errno);
        MAVLINK_BRIDGE_APP_CloseLoRa();
    }
}

static void MAVLINK_BRIDGE_APP_PublishAttitude(uint32 BridgeTimestampMs)
{
    MAVLINK_BRIDGE_APP_AttitudeTlm_t *Tlm;

    if (MAVLINK_BRIDGE_APP_Parser.PayloadLen != MAVLINK_ATTITUDE_PAYLOAD_LEN)
    {
        MAVLINK_BRIDGE_APP_RecordLengthError(MAVLINK_MSG_ID_ATTITUDE,
                                             MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                                             MAVLINK_ATTITUDE_PAYLOAD_LEN);
        return;
    }

    Tlm = &MAVLINK_BRIDGE_APP_Data.AttitudeTlm;

    Tlm->TimestampMs   = MAVLINK_BRIDGE_APP_ReadU32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
    Tlm->Seq           = ++MAVLINK_BRIDGE_APP_Data.SequenceCounter;
    Tlm->Valid         = 1;
    Tlm->Stale         = 0;
    Tlm->ErrorCode     = MAVLINK_BRIDGE_ERROR_NONE;
    Tlm->Reserved      = 0;
    Tlm->RollRad       = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[4]);
    Tlm->PitchRad      = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[8]);
    Tlm->YawRad        = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[12]);
    Tlm->RollspeedRps  = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[16]);
    Tlm->PitchspeedRps = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[20]);
    Tlm->YawspeedRps   = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[24]);
    MAVLINK_BRIDGE_APP_Data.LastAttitudeRxMs = BridgeTimestampMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
    MAVLINK_BRIDGE_APP_ServiceLoRa();

    if (MAVLINK_BRIDGE_APP_ShouldLogDecoded(Tlm->Seq))
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: ATTITUDE decoded seq=%lu boot_ms=%lu rx_ms=%lu",
                          (unsigned long)Tlm->Seq,
                          (unsigned long)Tlm->TimestampMs,
                          (unsigned long)BridgeTimestampMs);
    }
}

static void MAVLINK_BRIDGE_APP_PublishEkfLocal(uint32 BridgeTimestampMs)
{
    MAVLINK_BRIDGE_APP_EkfLocalTlm_t *Tlm;

    if (MAVLINK_BRIDGE_APP_Parser.PayloadLen != MAVLINK_LOCAL_POSITION_NED_PAYLOAD_LEN)
    {
        MAVLINK_BRIDGE_APP_RecordLengthError(MAVLINK_MSG_ID_LOCAL_POSITION_NED,
                                             MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                                             MAVLINK_LOCAL_POSITION_NED_PAYLOAD_LEN);
        return;
    }

    Tlm = &MAVLINK_BRIDGE_APP_Data.EkfLocalTlm;

    Tlm->TimestampMs = MAVLINK_BRIDGE_APP_ReadU32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
    Tlm->Seq         = ++MAVLINK_BRIDGE_APP_Data.SequenceCounter;
    Tlm->Valid       = 1;
    Tlm->Stale       = 0;
    Tlm->ErrorCode   = MAVLINK_BRIDGE_ERROR_NONE;
    Tlm->Reserved    = 0;
    Tlm->X_m         = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[4]);
    Tlm->Y_m         = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[8]);
    Tlm->Z_m         = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[12]);
    Tlm->Vx_mps      = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[16]);
    Tlm->Vy_mps      = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[20]);
    Tlm->Vz_mps      = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[24]);
    MAVLINK_BRIDGE_APP_Data.LastEkfLocalRxMs = BridgeTimestampMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
    MAVLINK_BRIDGE_APP_ServiceLoRa();

    if (MAVLINK_BRIDGE_APP_ShouldLogDecoded(Tlm->Seq))
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: LOCAL_POSITION_NED decoded seq=%lu boot_ms=%lu rx_ms=%lu",
                          (unsigned long)Tlm->Seq,
                          (unsigned long)Tlm->TimestampMs,
                          (unsigned long)BridgeTimestampMs);
    }
}

static void MAVLINK_BRIDGE_APP_PublishGlobalPositionAsLocal(uint32 BridgeTimestampMs)
{
    MAVLINK_BRIDGE_APP_EkfLocalTlm_t *Tlm;
    int32                             RelativeAltMm;
    int16                             VxCms;
    int16                             VyCms;
    int16                             VzCms;

    if (MAVLINK_BRIDGE_APP_Parser.PayloadLen != MAVLINK_GLOBAL_POSITION_INT_PAYLOAD_LEN)
    {
        MAVLINK_BRIDGE_APP_RecordLengthError(MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                                             MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                                             MAVLINK_GLOBAL_POSITION_INT_PAYLOAD_LEN);
        return;
    }

    Tlm = &MAVLINK_BRIDGE_APP_Data.EkfLocalTlm;

    RelativeAltMm = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[16]);
    VxCms         = (int16)MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[20]);
    VyCms         = (int16)MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[22]);
    VzCms         = (int16)MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[24]);

    Tlm->TimestampMs = MAVLINK_BRIDGE_APP_ReadU32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
    Tlm->Seq         = ++MAVLINK_BRIDGE_APP_Data.SequenceCounter;
    Tlm->Valid       = 1;
    Tlm->Stale       = 0;
    Tlm->ErrorCode   = MAVLINK_BRIDGE_ERROR_NONE;
    Tlm->Reserved    = 0;
    Tlm->X_m         = 0.0f;
    Tlm->Y_m         = 0.0f;
    Tlm->Z_m         = -((float)RelativeAltMm / 1000.0f);
    Tlm->Vx_mps      = (float)VxCms / 100.0f;
    Tlm->Vy_mps      = (float)VyCms / 100.0f;
    Tlm->Vz_mps      = (float)VzCms / 100.0f;
    MAVLINK_BRIDGE_APP_Data.LastEkfLocalRxMs = BridgeTimestampMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
    MAVLINK_BRIDGE_APP_ServiceLoRa();

    if (MAVLINK_BRIDGE_APP_ShouldLogDecoded(Tlm->Seq))
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: GLOBAL_POSITION_INT mapped seq=%lu boot_ms=%lu rx_ms=%lu",
                          (unsigned long)Tlm->Seq,
                          (unsigned long)Tlm->TimestampMs,
                          (unsigned long)BridgeTimestampMs);
    }
}

static void MAVLINK_BRIDGE_APP_PublishGpsRaw(uint32 BridgeTimestampMs)
{
    MAVLINK_BRIDGE_APP_GpsRawTlm_t *Tlm;
    uint64                          TimeUsec;

    if (MAVLINK_BRIDGE_APP_Parser.PayloadLen < MAVLINK_GPS_RAW_INT_PAYLOAD_LEN)
    {
        MAVLINK_BRIDGE_APP_RecordLengthError(MAVLINK_MSG_ID_GPS_RAW_INT,
                                             MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                                             MAVLINK_GPS_RAW_INT_PAYLOAD_LEN);
        return;
    }

    Tlm = &MAVLINK_BRIDGE_APP_Data.GpsRawTlm;

    TimeUsec               = MAVLINK_BRIDGE_APP_ReadU64LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
    Tlm->TimestampMs       = (uint32)(TimeUsec / 1000ULL);
    Tlm->Seq               = ++MAVLINK_BRIDGE_APP_Data.SequenceCounter;
    Tlm->FixType           = MAVLINK_BRIDGE_APP_Parser.Payload[28];
    Tlm->SatellitesVisible = MAVLINK_BRIDGE_APP_Parser.Payload[29];
    Tlm->Reserved          = 0;
    Tlm->LatE7             = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[8]);
    Tlm->LonE7             = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[12]);
    Tlm->AltMm             = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[16]);

    if (Tlm->FixType >= 3U)
    {
        Tlm->Valid     = 1;
        Tlm->Stale     = 0;
        Tlm->ErrorCode = MAVLINK_BRIDGE_ERROR_NONE;
        MAVLINK_BRIDGE_APP_Data.LastGpsRawRxMs = BridgeTimestampMs;
    }
    else
    {
        Tlm->Valid     = 0;
        Tlm->Stale     = 0;
        Tlm->ErrorCode = MAVLINK_BRIDGE_ERROR_GPS_NO_FIX;
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_GPS_NO_FIX;
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);

    if (MAVLINK_BRIDGE_APP_ShouldLogDecoded(Tlm->Seq))
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: GPS_RAW_INT decoded seq=%lu fix=%u sats=%u rx_ms=%lu",
                          (unsigned long)Tlm->Seq,
                          (unsigned int)Tlm->FixType,
                          (unsigned int)Tlm->SatellitesVisible,
                          (unsigned long)BridgeTimestampMs);
    }
}

static void MAVLINK_BRIDGE_APP_PublishEkfStatus(uint32 BridgeTimestampMs)
{
    MAVLINK_BRIDGE_APP_EkfStatusTlm_t *Tlm;

    if (MAVLINK_BRIDGE_APP_Parser.PayloadLen < MAVLINK_EKF_STATUS_MIN_PAYLOAD_LEN)
    {
        MAVLINK_BRIDGE_APP_RecordLengthError(MAVLINK_MSG_ID_EKF_STATUS_REPORT,
                                             MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                                             MAVLINK_EKF_STATUS_MIN_PAYLOAD_LEN);
        return;
    }

    Tlm = &MAVLINK_BRIDGE_APP_Data.EkfStatusTlm;

    Tlm->TimestampMs = BridgeTimestampMs;
    Tlm->Seq         = ++MAVLINK_BRIDGE_APP_Data.SequenceCounter;
    Tlm->Flags       = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[20]);
    Tlm->Reserved    = 0;

    if (Tlm->Flags != 0U)
    {
        Tlm->Valid     = 1;
        Tlm->Stale     = 0;
        Tlm->ErrorCode = MAVLINK_BRIDGE_ERROR_NONE;
        MAVLINK_BRIDGE_APP_Data.LastEkfStatusRxMs = BridgeTimestampMs;
    }
    else
    {
        Tlm->Valid     = 0;
        Tlm->Stale     = 0;
        Tlm->ErrorCode = MAVLINK_BRIDGE_ERROR_EKF_UNHEALTHY;
        MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_EKF_UNHEALTHY;
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);

    if (MAVLINK_BRIDGE_APP_ShouldLogDecoded(Tlm->Seq))
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: EKF_STATUS_REPORT decoded seq=%lu flags=0x%04X rx_ms=%lu",
                          (unsigned long)Tlm->Seq,
                          (unsigned int)Tlm->Flags,
                          (unsigned long)BridgeTimestampMs);
    }
}

static void MAVLINK_BRIDGE_APP_HandleFrameComplete(uint32 RxTimestampMs, uint8 CrcHigh)
{
    uint16 ComputedCrc;
    uint16 ReceivedCrc;

    ReceivedCrc = ((uint16)CrcHigh << 8) | MAVLINK_BRIDGE_APP_Parser.CrcLow;

    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: frame msgid=%lu len=%u rx_ms=%lu",
                      (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                      (unsigned int)MAVLINK_BRIDGE_APP_Parser.PayloadLen,
                      (unsigned long)RxTimestampMs);

    if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_HEARTBEAT)
    {
        MAVLINK_BRIDGE_APP_Data.TargetSystemId    = MAVLINK_BRIDGE_APP_Parser.SysId;
        MAVLINK_BRIDGE_APP_Data.TargetComponentId = MAVLINK_BRIDGE_APP_Parser.CompId;
        MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = RxTimestampMs;
        MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_COMMAND_ACK)
    {
        ComputedCrc = MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_COMMAND_ACK_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 10U)
        {
            uint16 Command = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[8]);
            uint8  Result  = MAVLINK_BRIDGE_APP_Parser.Payload[0];

            if (Command == MAVLINK_CMD_SET_MESSAGE_INTERVAL)
            {
                if (Result == MAVLINK_RESULT_ACCEPTED)
                {
                    MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount++;
                }

                CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_INFORMATION,
                                  "MAVLINK_BRIDGE_APP: COMMAND_ACK cmd=%u result=%u ack_count=%u",
                                  (unsigned int)Command,
                                  (unsigned int)Result,
                                  (unsigned int)MAVLINK_BRIDGE_APP_Data.StreamRequestAckCount);
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_ATTITUDE)
    {
        ComputedCrc = MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_ATTITUDE_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_NONE;
            MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = RxTimestampMs;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
            MAVLINK_BRIDGE_APP_PublishAttitude(RxTimestampMs);
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: crc fail msgid=%lu got=0x%04X expected=0x%04X",
                              (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                              (unsigned int)ReceivedCrc,
                              (unsigned int)ComputedCrc);
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_LOCAL_POSITION_NED)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_LOCAL_POSITION_NED_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode      = MAVLINK_BRIDGE_ERROR_NONE;
            MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs  = RxTimestampMs;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
            MAVLINK_BRIDGE_APP_PublishEkfLocal(RxTimestampMs);
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: crc fail msgid=%lu got=0x%04X expected=0x%04X",
                              (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                              (unsigned int)ReceivedCrc,
                              (unsigned int)ComputedCrc);
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_GLOBAL_POSITION_INT)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_GLOBAL_POSITION_INT_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode     = MAVLINK_BRIDGE_ERROR_NONE;
            MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = RxTimestampMs;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
            MAVLINK_BRIDGE_APP_PublishGlobalPositionAsLocal(RxTimestampMs);
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: crc fail msgid=%lu got=0x%04X expected=0x%04X",
                              (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                              (unsigned int)ReceivedCrc,
                              (unsigned int)ComputedCrc);
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_GPS_RAW_INT)
    {
        ComputedCrc = MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_GPS_RAW_INT_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode     = MAVLINK_BRIDGE_ERROR_NONE;
            MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = RxTimestampMs;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
            MAVLINK_BRIDGE_APP_PublishGpsRaw(RxTimestampMs);
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: crc fail msgid=%lu got=0x%04X expected=0x%04X",
                              (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                              (unsigned int)ReceivedCrc,
                              (unsigned int)ComputedCrc);
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_EKF_STATUS_REPORT)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_EKF_STATUS_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode     = MAVLINK_BRIDGE_ERROR_NONE;
            MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = RxTimestampMs;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);
            MAVLINK_BRIDGE_APP_PublishEkfStatus(RxTimestampMs);
        }
        else
        {
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: crc fail msgid=%lu got=0x%04X expected=0x%04X",
                              (unsigned long)MAVLINK_BRIDGE_APP_Parser.MsgId,
                              (unsigned int)ReceivedCrc,
                              (unsigned int)ComputedCrc);
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_MISSION_REQUEST_INT)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_MISSION_REQUEST_INT_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 2U)
        {
            uint16 Seq = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
            if (MAVLINK_BRIDGE_APP_Data.MissionUploadState == (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE &&
                Seq < (uint16)MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount)
            {
                MAVLINK_BRIDGE_APP_Data.MissionUploadTimeoutMs =
                    RxTimestampMs + MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS;
                MAVLINK_BRIDGE_APP_SendMissionItemInt((uint8)Seq);
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_MISSION_REQUEST)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_MISSION_REQUEST_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 2U)
        {
            uint16 Seq = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
            if (MAVLINK_BRIDGE_APP_Data.MissionUploadState == (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE &&
                Seq < (uint16)MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount)
            {
                MAVLINK_BRIDGE_APP_Data.MissionUploadTimeoutMs =
                    RxTimestampMs + MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS;
                MAVLINK_BRIDGE_APP_SendMissionItem((uint8)Seq);
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_MISSION_ACK)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_MISSION_ACK_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 3U)
        {
            uint8 Result = MAVLINK_BRIDGE_APP_Parser.Payload[2];
            if (MAVLINK_BRIDGE_APP_Data.MissionUploadState == (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_ACTIVE)
            {
                MAVLINK_BRIDGE_APP_Data.MissionUploadState = (uint8)MAVLINK_BRIDGE_MISSION_UPLOAD_IDLE;
                if (Result == (uint8)MAVLINK_MISSION_ACCEPTED)
                {
                    MAVLINK_BRIDGE_APP_Data.MissionUploadSuccessCount++;
                    MAVLINK_BRIDGE_APP_Data.LastUploadResult        = (uint8)MAVLINK_BRIDGE_UPLOAD_RESULT_SUCCESS;
                    MAVLINK_BRIDGE_APP_Data.LastUploadTimestampMs   = RxTimestampMs;
                    MAVLINK_BRIDGE_APP_Data.LastUploadWaypointCount = MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount;
                    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                                      "MAVLINK_BRIDGE_APP: mission upload success wp_count=%u",
                                      (unsigned int)MAVLINK_BRIDGE_APP_Data.MissionUploadWpCount);
                }
                else
                {
                    MAVLINK_BRIDGE_APP_Data.MissionUploadFailCount++;
                    MAVLINK_BRIDGE_APP_Data.LastUploadResult = (uint8)MAVLINK_BRIDGE_UPLOAD_RESULT_NAK;
                    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_UPLOAD_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "MAVLINK_BRIDGE_APP: mission upload NAK result=%u",
                                      (unsigned int)Result);
                }
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_MISSION_COUNT &&
             MAVLINK_BRIDGE_APP_Data.MissionDownloadState == (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_COUNT)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_MISSION_COUNT_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 2U)
        {
            uint16 Count = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[0]);
            MAVLINK_BRIDGE_APP_Data.MissionDownloadExpectedCount = (uint8)(Count > 255U ? 255U : Count);
            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: MISSION_COUNT from FC: %u waypoints",
                              (unsigned int)Count);
            if (Count == 0U)
            {
                MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
                MAVLINK_BRIDGE_APP_SendMissionAckAccepted();
                CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "MAVLINK_BRIDGE_APP: MISSION download complete: 0 waypoints (empty)");
            }
            else
            {
                MAVLINK_BRIDGE_APP_Data.MissionDownloadState     = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_ITEM;
                MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq       = 0U;
                MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs =
                    RxTimestampMs + MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_TIMEOUT_MS;
                MAVLINK_BRIDGE_APP_SendMissionRequestIntDownload(0U);
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_MISSION_ITEM_INT &&
             MAVLINK_BRIDGE_APP_Data.MissionDownloadState == (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_WAIT_ITEM)
    {
        ComputedCrc =
            MAVLINK_BRIDGE_APP_ComputeFrameCrc(&MAVLINK_BRIDGE_APP_Parser, MAVLINK_MISSION_ITEM_INT_CRC_EXTRA);
        if (ComputedCrc == ReceivedCrc && MAVLINK_BRIDGE_APP_Parser.PayloadLen >= 32U)
        {
            int32  X_E4 = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[16]);
            int32  Y_E4 = MAVLINK_BRIDGE_APP_ReadI32LE(&MAVLINK_BRIDGE_APP_Parser.Payload[20]);
            float  Z_m  = MAVLINK_BRIDGE_APP_ReadFloatLE(&MAVLINK_BRIDGE_APP_Parser.Payload[24]);
            uint16 Seq  = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[28]);
            uint16 Cmd  = MAVLINK_BRIDGE_APP_ReadU16LE(&MAVLINK_BRIDGE_APP_Parser.Payload[30]);

            CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "MAVLINK_BRIDGE_APP: [wp %u] x=%.2f y=%.2f z=%.2f cmd=%u",
                              (unsigned int)Seq,
                              (double)(X_E4 / 10000.0f),
                              (double)(Y_E4 / 10000.0f),
                              (double)Z_m,
                              (unsigned int)Cmd);

            MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq++;
            if (MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq >=
                MAVLINK_BRIDGE_APP_Data.MissionDownloadExpectedCount)
            {
                MAVLINK_BRIDGE_APP_SendMissionAckAccepted();
                MAVLINK_BRIDGE_APP_Data.MissionDownloadState = (uint8)MAVLINK_BRIDGE_MISSION_DOWNLOAD_IDLE;
                CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "MAVLINK_BRIDGE_APP: MISSION download complete: %u waypoints",
                                  (unsigned int)MAVLINK_BRIDGE_APP_Data.MissionDownloadExpectedCount);
            }
            else
            {
                MAVLINK_BRIDGE_APP_Data.MissionDownloadTimeoutMs =
                    RxTimestampMs + MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_TIMEOUT_MS;
                MAVLINK_BRIDGE_APP_SendMissionRequestIntDownload(MAVLINK_BRIDGE_APP_Data.MissionDownloadSeq);
            }
        }
        else
        {
            MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
        }
    }
    else
    {
    }

    MAVLINK_BRIDGE_APP_ResetParser();
}

static void MAVLINK_BRIDGE_APP_ProcessReceivedByte(uint8 Byte, uint32 RxTimestampMs)
{
    if (Byte == MAVLINK_STX_V1)
    {
        MAVLINK_BRIDGE_APP_ResetParser();
        MAVLINK_BRIDGE_APP_Parser.IsV2  = 0;
        MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_LENGTH;
        return;
    }
    else if (Byte == MAVLINK_STX_V2)
    {
        MAVLINK_BRIDGE_APP_ResetParser();
        MAVLINK_BRIDGE_APP_Parser.IsV2  = 1;
        MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_LENGTH;
        return;
    }

    switch (MAVLINK_BRIDGE_APP_Parser.State)
    {
        case MAVLINK_PARSE_WAIT_STX:
            break;

        case MAVLINK_PARSE_GOT_LENGTH:
            MAVLINK_BRIDGE_APP_Parser.PayloadLen = Byte;
            if (Byte > MAVLINK_MAX_PAYLOAD_LEN)
            {
                MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_ERROR_PARSE_FAIL);
                MAVLINK_BRIDGE_APP_ResetParser();
            }
            else if (MAVLINK_BRIDGE_APP_Parser.IsV2)
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_INCOMPAT;
            }
            else
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_SEQ;
            }
            break;

        case MAVLINK_PARSE_GOT_INCOMPAT:
            MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_COMPAT;
            break;

        case MAVLINK_PARSE_GOT_COMPAT:
            MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_SEQ;
            break;

        case MAVLINK_PARSE_GOT_SEQ:
            MAVLINK_BRIDGE_APP_Parser.Seq   = Byte;
            MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_SYSID;
            break;

        case MAVLINK_PARSE_GOT_SYSID:
            MAVLINK_BRIDGE_APP_Parser.SysId = Byte;
            MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_COMPID;
            break;

        case MAVLINK_PARSE_GOT_COMPID:
            MAVLINK_BRIDGE_APP_Parser.CompId = Byte;
            MAVLINK_BRIDGE_APP_Parser.State  = MAVLINK_PARSE_GOT_MSGID1;
            break;

        case MAVLINK_PARSE_GOT_MSGID1:
            MAVLINK_BRIDGE_APP_Parser.MsgId = Byte;
            if (MAVLINK_BRIDGE_APP_Parser.IsV2)
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_MSGID2;
            }
            else if (MAVLINK_BRIDGE_APP_Parser.PayloadLen == 0U)
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_CRC1;
            }
            else
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_READING_PAYLOAD;
            }
            break;

        case MAVLINK_PARSE_GOT_MSGID2:
            MAVLINK_BRIDGE_APP_Parser.MsgId |= ((uint32)Byte << 8);
            MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_MSGID3;
            break;

        case MAVLINK_PARSE_GOT_MSGID3:
            MAVLINK_BRIDGE_APP_Parser.MsgId |= ((uint32)Byte << 16);
            MAVLINK_BRIDGE_APP_Parser.State = (MAVLINK_BRIDGE_APP_Parser.PayloadLen == 0U) ? MAVLINK_PARSE_GOT_CRC1 : MAVLINK_PARSE_READING_PAYLOAD;
            break;

        case MAVLINK_PARSE_READING_PAYLOAD:
            MAVLINK_BRIDGE_APP_Parser.Payload[MAVLINK_BRIDGE_APP_Parser.PayloadIndex++] = Byte;
            if (MAVLINK_BRIDGE_APP_Parser.PayloadIndex >= MAVLINK_BRIDGE_APP_Parser.PayloadLen)
            {
                MAVLINK_BRIDGE_APP_Parser.State = MAVLINK_PARSE_GOT_CRC1;
            }
            break;

        case MAVLINK_PARSE_GOT_CRC1:
            MAVLINK_BRIDGE_APP_Parser.CrcLow = Byte;
            MAVLINK_BRIDGE_APP_Parser.State  = MAVLINK_PARSE_GOT_CRC2;
            break;

        case MAVLINK_PARSE_GOT_CRC2:
            MAVLINK_BRIDGE_APP_Parser.CrcHigh = Byte;
            MAVLINK_BRIDGE_APP_HandleFrameComplete(RxTimestampMs, MAVLINK_BRIDGE_APP_Parser.CrcHigh);
            break;

        default:
            MAVLINK_BRIDGE_APP_ResetParser();
            break;
    }
}

static void MAVLINK_BRIDGE_APP_HandleReceivedBytes(const uint8 *Buffer, ssize_t Length, uint32 TimestampMs)
{
    ssize_t Index;

    MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs = TimestampMs;
    MAVLINK_BRIDGE_APP_Data.BytesReceived += (uint32)Length;
    MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_NONE;
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_CONNECTED);

    for (Index = 0; Index < Length; ++Index)
    {
        MAVLINK_BRIDGE_APP_ProcessReceivedByte(Buffer[Index], TimestampMs);
    }
}

void MAVLINK_BRIDGE_APP_ReportHousekeeping(void)
{
    MAVLINK_BRIDGE_APP_Data.HkTlm.CommandCounter             = MAVLINK_BRIDGE_APP_Data.CmdCounter;
    MAVLINK_BRIDGE_APP_Data.HkTlm.CommandErrorCounter        = MAVLINK_BRIDGE_APP_Data.ErrCounter;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LinkState                  = MAVLINK_BRIDGE_APP_Data.LinkState;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LastErrorCode              = MAVLINK_BRIDGE_APP_Data.LastErrorCode;
    MAVLINK_BRIDGE_APP_Data.HkTlm.BytesReceived              = MAVLINK_BRIDGE_APP_Data.BytesReceived;
    MAVLINK_BRIDGE_APP_Data.HkTlm.ReconnectAttemptCount      = MAVLINK_BRIDGE_APP_Data.ReconnectAttemptCount;
    MAVLINK_BRIDGE_APP_Data.HkTlm.ParseErrorCount            = MAVLINK_BRIDGE_APP_Data.ParseErrorCount;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LastRxTimestampMs          = MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs;
    MAVLINK_BRIDGE_APP_Data.HkTlm.MissionUploadSuccessCount  = MAVLINK_BRIDGE_APP_Data.MissionUploadSuccessCount;
    MAVLINK_BRIDGE_APP_Data.HkTlm.MissionUploadFailCount     = MAVLINK_BRIDGE_APP_Data.MissionUploadFailCount;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LastUploadTimestampMs      = MAVLINK_BRIDGE_APP_Data.LastUploadTimestampMs;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LastUploadWaypointCount    = MAVLINK_BRIDGE_APP_Data.LastUploadWaypointCount;
    MAVLINK_BRIDGE_APP_Data.HkTlm.LastUploadResult           = MAVLINK_BRIDGE_APP_Data.LastUploadResult;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.HkTlm.TelemetryHeader), true);
}

bool MAVLINK_BRIDGE_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);
    if (ActualLength != ExpectedLength)
    {
        MAVLINK_BRIDGE_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                          "MAVLINK_BRIDGE_APP: Invalid cmd length expected=%lu actual=%lu",
                          (unsigned long)ExpectedLength, (unsigned long)ActualLength);
        return false;
    }

    return true;
}

void MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_APP_LinkState_t NewState)
{
    MAVLINK_BRIDGE_APP_Data.LinkState = (uint8)NewState;
}

void MAVLINK_BRIDGE_APP_RecordParseError(MAVLINK_BRIDGE_APP_ErrorCode_t ErrorCode)
{
    MAVLINK_BRIDGE_APP_Data.LastErrorCode = (uint8)ErrorCode;
    MAVLINK_BRIDGE_APP_Data.ParseErrorCount++;
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_PARSE_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: Parse/data error code=%u", (unsigned int)ErrorCode);
}

void MAVLINK_BRIDGE_APP_ServiceSerial(void)
{
    uint32  NowMs;
    uint8   RxBuffer[MAVLINK_BRIDGE_APP_READ_CHUNK_SIZE];
    ssize_t ReadSize;
    bool    SawData;

    NowMs = MAVLINK_BRIDGE_APP_GetTimeMs();

    if (MAVLINK_BRIDGE_APP_Data.SerialFd < 0)
    {
        if ((NowMs - MAVLINK_BRIDGE_APP_Data.LastReconnectAttemptMs) >= MAVLINK_BRIDGE_APP_Data.ReconnectIntervalMs)
        {
            MAVLINK_BRIDGE_APP_Data.LastReconnectAttemptMs = NowMs;
            MAVLINK_BRIDGE_APP_Data.ReconnectAttemptCount++;
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_DISCONNECTED);

            if (MAVLINK_BRIDGE_APP_OpenSerial() != CFE_SUCCESS)
            {
                CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_INFORMATION,
                                  "MAVLINK_BRIDGE_APP: serial open failed path=%s errno=%d",
                                  MAVLINK_BRIDGE_APP_SERIAL_PATH, errno);
            }
        }

        return;
    }

    MAVLINK_BRIDGE_APP_SendCompanionHeartbeat(NowMs);
    if (MAVLINK_BRIDGE_APP_Data.TargetSystemId != 0U &&
        (NowMs - MAVLINK_BRIDGE_APP_Data.LastStreamRequestMs) >= MAVLINK_BRIDGE_APP_STREAM_REQUEST_RETRY_MS)
    {
        MAVLINK_BRIDGE_APP_Data.StreamRequestPending = 1;
        MAVLINK_BRIDGE_APP_RequestTelemetryStreams();
    }
    else if (MAVLINK_BRIDGE_APP_Data.StreamRequestPending != 0U &&
             MAVLINK_BRIDGE_APP_Data.TargetSystemId == 0U &&
             (NowMs - MAVLINK_BRIDGE_APP_Data.TargetDiscoveryStartMs) >= MAVLINK_BRIDGE_APP_TARGET_DISCOVERY_TIMEOUT_MS)
    {
        CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_STREAM_EID, CFE_EVS_EventType_INFORMATION,
                          "MAVLINK_BRIDGE_APP: waiting for FC heartbeat before stream request");
        MAVLINK_BRIDGE_APP_Data.TargetDiscoveryStartMs = NowMs;
    }

    MAVLINK_BRIDGE_APP_CheckMissionUploadTimeout(NowMs);
    MAVLINK_BRIDGE_APP_CheckMissionDownloadTimeout(NowMs);

    SawData = false;
    while ((ReadSize = read(MAVLINK_BRIDGE_APP_Data.SerialFd, RxBuffer, sizeof(RxBuffer))) > 0)
    {
        SawData = true;
        MAVLINK_BRIDGE_APP_HandleReceivedBytes(RxBuffer, ReadSize, NowMs);
    }

    if (SawData)
    {
        return;
    }

    if (ReadSize == 0 || (ReadSize < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)))
    {
        if ((NowMs - MAVLINK_BRIDGE_APP_Data.LastRxTimestampMs) >= MAVLINK_BRIDGE_APP_STALE_TIMEOUT_MS)
        {
            MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_READ_TIMEOUT;
            MAVLINK_BRIDGE_APP_MarkOutputsStale();
            MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_DEGRADED);
        }
        return;
    }

    MAVLINK_BRIDGE_APP_Data.LastErrorCode = MAVLINK_BRIDGE_ERROR_LINK_DOWN;
    MAVLINK_BRIDGE_APP_MarkOutputsStale();
    MAVLINK_BRIDGE_APP_SetLinkState(MAVLINK_BRIDGE_LINK_DISCONNECTED);
    CFE_EVS_SendEvent(MAVLINK_BRIDGE_APP_LINK_EID, CFE_EVS_EventType_INFORMATION,
                      "MAVLINK_BRIDGE_APP: serial read failed errno=%d, forcing reconnect", errno);
    MAVLINK_BRIDGE_APP_CloseSerial();
}

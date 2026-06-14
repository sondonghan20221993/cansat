#include "lora_fc_downlink_app_utils.h"
#include "lora_fc_downlink_app.h"
#include "lora_fc_downlink_app_eventids.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     RollRad;
    float                     PitchRad;
    float                     YawRad;
    float                     RollspeedRps;
    float                     PitchspeedRps;
    float                     YawspeedRps;
} LORA_FC_DOWNLINK_APP_AttitudeTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    float                     X_m;
    float                     Y_m;
    float                     Z_m;
    float                     Vx_mps;
    float                     Vy_mps;
    float                     Vz_mps;
} LORA_FC_DOWNLINK_APP_EkfLocalTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     FixType;
    uint8                     SatellitesVisible;
    uint8                     Reserved;
    int32                     LatE7;
    int32                     LonE7;
    int32                     AltMm;
} LORA_FC_DOWNLINK_APP_GpsRawTlm_t;

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

static uint16 LORA_FC_DOWNLINK_APP_CRC16(const char *Data, size_t Len)
{
    uint16 Crc = 0xFFFF;
    size_t i;
    int    j;

    for (i = 0; i < Len; i++)
    {
        Crc ^= (uint16)(uint8)Data[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (Crc & 0x8000)
                Crc = (Crc << 1) ^ 0x1021;
            else
                Crc <<= 1;
        }
    }
    return Crc;
}

static bool LORA_FC_DOWNLINK_APP_ParseHb(const char *Line)
{
    /* case-insensitive prefix check for "HB" */
    if ((Line[0] != 'H' && Line[0] != 'h') || (Line[1] != 'B' && Line[1] != 'b'))
    {
        return false;
    }

    /* manual: "HB" or short "HB\n" */
    if (Line[2] == '\0' || Line[2] == '\n' || Line[2] == '\r')
    {
        return true;
    }

    if (Line[2] != ',')
    {
        return false;
    }

    /* canonical: "HB,<node>,<seq>,<tx_ms>,<sensor_ok>,<crc>" — 6 fields */
    {
        int  node, seq, tx_ms, sensor_ok;
        char crc_hex[16];

        if (sscanf(Line + 3, "%d,%d,%d,%d,%15s", &node, &seq, &tx_ms, &sensor_ok, crc_hex) == 5)
        {
            /* validate CRC: canonical string is "HB,node,seq,tx_ms,sensor_ok" */
            char   Canonical[128];
            size_t Len;
            uint16 ExpectedCrc;
            uint16 ActualCrc;

            snprintf(Canonical, sizeof(Canonical), "HB,%d,%d,%d,%d", node, seq, tx_ms, sensor_ok);
            Len         = strlen(Canonical);
            ExpectedCrc = (uint16)strtoul(crc_hex, NULL, 16);
            ActualCrc   = LORA_FC_DOWNLINK_APP_CRC16(Canonical, Len);

            if (ActualCrc != ExpectedCrc)
            {
                return false;
            }
            return sensor_ok != 0;
        }
    }

    /* short form: "HB,<seq>" — accept without CRC */
    return true;
}

static void LORA_FC_DOWNLINK_APP_ServiceLoRaRead(void)
{
    ssize_t rc;
    char    c;

    if (LORA_FC_DOWNLINK_APP_Data.LoRaFd < 0)
    {
        return;
    }

    /* read one byte at a time; accumulate into LoRaReadBuf until newline */
    rc = read(LORA_FC_DOWNLINK_APP_Data.LoRaFd, &c, 1);
    if (rc <= 0)
    {
        return;
    }

    if (LORA_FC_DOWNLINK_APP_Data.LoRaReadLen < sizeof(LORA_FC_DOWNLINK_APP_Data.LoRaReadBuf) - 1)
    {
        LORA_FC_DOWNLINK_APP_Data.LoRaReadBuf[LORA_FC_DOWNLINK_APP_Data.LoRaReadLen++] = c;
    }

    if (c != '\n')
    {
        return;
    }

    LORA_FC_DOWNLINK_APP_Data.LoRaReadBuf[LORA_FC_DOWNLINK_APP_Data.LoRaReadLen] = '\0';
    LORA_FC_DOWNLINK_APP_Data.LoRaReadLen = 0;

    if (LORA_FC_DOWNLINK_APP_ParseHb(LORA_FC_DOWNLINK_APP_Data.LoRaReadBuf))
    {
        CFE_TIME_SysTime_t Now   = CFE_TIME_GetTime();
        uint32             NowMs = (uint32)(Now.Seconds * 1000U);
        LORA_FC_DOWNLINK_APP_Data.HbLastRxMs  = NowMs;
        LORA_FC_DOWNLINK_APP_Data.HbLinkValid = 1;
    }
}

static void LORA_FC_DOWNLINK_APP_ServiceLoRa(void)
{
    int                Fd;
    int                WriteRc;
    struct termios     Tio;
    char               Line[256];
    int                LineLen;
    speed_t            BaudConstant = B57600;
    CFE_TIME_SysTime_t Now;
    uint32             NowMs;

    if (LORA_FC_DOWNLINK_APP_Data.LoRaFd < 0)
    {
        Fd = open(LORA_FC_DOWNLINK_APP_LORA_SERIAL_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
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
        Tio.c_cflag |= CS8 | CLOCAL | CREAD;
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
            int Flags = fcntl(Fd, F_GETFL, 0);
            if (Flags >= 0)
            {
                fcntl(Fd, F_SETFL, Flags & ~O_NONBLOCK);
            }
        }

        LORA_FC_DOWNLINK_APP_Data.LoRaFd = Fd;
        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "LORA_FC_DOWNLINK_APP: opened LoRa %s at %u baud",
                          LORA_FC_DOWNLINK_APP_LORA_SERIAL_PATH, LORA_FC_DOWNLINK_APP_LORA_BAUDRATE);
    }

    /* Rate limiting: minimum 500ms between LoRa writes to avoid overwhelming the link */
    Now   = CFE_TIME_GetTime();
    NowMs = (uint32)((uint64)Now.Seconds * 1000ULL + (uint64)Now.Subseconds * 1000ULL / 0x100000000ULL);
    if ((NowMs - LORA_FC_DOWNLINK_APP_Data.LastLoRaTxMs) < 500U)
    {
        return;
    }

    if (LORA_FC_DOWNLINK_APP_Data.PacketType == LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_PACKET_TYPE)
    {
        LineLen = snprintf(Line, sizeof(Line), "SH,%lu,%lu,%u,%u,0,0\n",
                           (unsigned long)(++LORA_FC_DOWNLINK_APP_Data.LoRaTxCount),
                           (unsigned long)LORA_FC_DOWNLINK_APP_Data.LastSystemHealthTimestampMs,
                           (unsigned int)LORA_FC_DOWNLINK_APP_Data.SystemHealthState,
                           (unsigned int)LORA_FC_DOWNLINK_APP_Data.SystemHealthFaultCode);
    }
    else if (LORA_FC_DOWNLINK_APP_Data.AttitudeValid && LORA_FC_DOWNLINK_APP_Data.LocalValid)
    {
        LineLen = snprintf(Line, sizeof(Line),
                           "FC,%lu,%lu,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%ld,%ld,%ld,%u,0\n",
                           (unsigned long)(++LORA_FC_DOWNLINK_APP_Data.LoRaTxCount),
                           (unsigned long)LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs,
                           (double)LORA_FC_DOWNLINK_APP_Data.AttitudeRollRad,
                           (double)LORA_FC_DOWNLINK_APP_Data.AttitudePitchRad,
                           (double)LORA_FC_DOWNLINK_APP_Data.AttitudeYawRad,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalX_m,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalY_m,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalZ_m,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalVx_mps,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalVy_mps,
                           (double)LORA_FC_DOWNLINK_APP_Data.LocalVz_mps,
                           (long)LORA_FC_DOWNLINK_APP_Data.GpsLatE7,
                           (long)LORA_FC_DOWNLINK_APP_Data.GpsLonE7,
                           (long)LORA_FC_DOWNLINK_APP_Data.GpsAltMm,
                           (unsigned int)LORA_FC_DOWNLINK_APP_Data.GpsFixType);
    }
    else
    {
        return;
    }

    if (LineLen <= 0)
    {
        return;
    }

    WriteRc = (int)write(LORA_FC_DOWNLINK_APP_Data.LoRaFd, Line, (size_t)LineLen);
    if (WriteRc < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_FC_DOWNLINK_APP: LoRa write failed errno=%d, forcing reopen", errno);
        close(LORA_FC_DOWNLINK_APP_Data.LoRaFd);
        LORA_FC_DOWNLINK_APP_Data.LoRaFd = -1;
    }
    else
    {
        LORA_FC_DOWNLINK_APP_Data.LastLoRaTxMs = NowMs;
    }
}

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

    CFE_EVS_SendEvent(LORA_FC_DOWNLINK_APP_HK_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_FC_DOWNLINK_APP HK: downlink_count=%lu att=%u local=%u gps=%u ekf=%u health=%u pkt=%u",
                      (unsigned long)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.DownlinkCount,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.AttitudeValid,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.LocalValid,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.GpsValid,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.EkfValid,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.SystemHealthState,
                      (unsigned int)LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.PacketType);

    CFE_SB_TransmitMsg(CFE_MSG_PTR(LORA_FC_DOWNLINK_APP_Data.HkTlm.TelemetryHeader), true);
}

void LORA_FC_DOWNLINK_APP_ProcessInputMessage(const CFE_SB_Buffer_t *sb_buf_ptr)
{
    CFE_SB_MsgId_t msg_id = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&sb_buf_ptr->Msg, &msg_id);

    if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_AttitudeTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_AttitudeTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.AttitudeValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.AttitudeRollRad         = Msg->RollRad;
        LORA_FC_DOWNLINK_APP_Data.AttitudePitchRad        = Msg->PitchRad;
        LORA_FC_DOWNLINK_APP_Data.AttitudeYawRad          = Msg->YawRad;
        LORA_FC_DOWNLINK_APP_Data.PacketType              = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_EkfLocalTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_EkfLocalTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastLocalTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.LocalValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.LocalX_m             = Msg->X_m;
        LORA_FC_DOWNLINK_APP_Data.LocalY_m             = Msg->Y_m;
        LORA_FC_DOWNLINK_APP_Data.LocalZ_m             = Msg->Z_m;
        LORA_FC_DOWNLINK_APP_Data.LocalVx_mps          = Msg->Vx_mps;
        LORA_FC_DOWNLINK_APP_Data.LocalVy_mps          = Msg->Vy_mps;
        LORA_FC_DOWNLINK_APP_Data.LocalVz_mps          = Msg->Vz_mps;
        LORA_FC_DOWNLINK_APP_Data.PacketType           = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_GpsRawTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_GpsRawTlm_t *)&sb_buf_ptr->Msg;
        LORA_FC_DOWNLINK_APP_Data.LastGpsTimestampMs = Msg->TimestampMs;
        LORA_FC_DOWNLINK_APP_Data.GpsValid           = Msg->Valid;
        LORA_FC_DOWNLINK_APP_Data.GpsLatE7           = Msg->LatE7;
        LORA_FC_DOWNLINK_APP_Data.GpsLonE7           = Msg->LonE7;
        LORA_FC_DOWNLINK_APP_Data.GpsAltMm           = Msg->AltMm;
        LORA_FC_DOWNLINK_APP_Data.GpsFixType         = Msg->FixType;
        LORA_FC_DOWNLINK_APP_Data.PacketType         = LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE;
    }
    else if (CFE_SB_MsgIdToValue(msg_id) == LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE)
    {
        const LORA_FC_DOWNLINK_APP_AttitudeTlm_t *Msg =
            (const LORA_FC_DOWNLINK_APP_AttitudeTlm_t *)&sb_buf_ptr->Msg;
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
        LORA_FC_DOWNLINK_APP_Data.SystemHealthFaultCode       = Msg->FaultCode;
        LORA_FC_DOWNLINK_APP_Data.PacketType                  = LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_PACKET_TYPE;
    }

    LORA_FC_DOWNLINK_APP_Data.DownlinkCount++;
    LORA_FC_DOWNLINK_APP_ServiceLoRaRead();
    LORA_FC_DOWNLINK_APP_ServiceLoRa();
}

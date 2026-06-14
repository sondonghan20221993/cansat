#include "lora_tdm_app.h"
#include "lora_tdm_app_dispatch.h"
#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

LORA_TDM_APP_Data_t LORA_TDM_APP_Data;

/* ---- Serial open ---- */

static int OpenSerial(void)
{
    int            Fd;
    struct termios Tio;
    speed_t        Baud = B57600;
    int            Flags;

    Fd = open(LORA_TDM_APP_LORA_SERIAL_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (Fd < 0)
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_OPEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: open serial failed: %s", LORA_TDM_APP_LORA_SERIAL_PATH);
        return -1;
    }

    if (tcgetattr(Fd, &Tio) != 0)
    {
        close(Fd);
        return -1;
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
    cfsetispeed(&Tio, Baud);
    cfsetospeed(&Tio, Baud);

    if (tcsetattr(Fd, TCSANOW, &Tio) != 0)
    {
        close(Fd);
        return -1;
    }

    Flags = fcntl(Fd, F_GETFL, 0);
    if (Flags >= 0)
    {
        fcntl(Fd, F_SETFL, Flags & ~O_NONBLOCK);
    }

    return Fd;
}

/* ---- Get timestamp in ms ---- */

static uint32 GetTimeMs(void)
{
    CFE_TIME_SysTime_t T = CFE_TIME_GetTime();
    uint64             Ms;

    Ms = ((uint64)T.Seconds * 1000ULL) + ((uint64)T.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)Ms;
}

/* ---- RX window: read until timeout or newline ---- */

static void RunRxWindow(void)
{
    char    Buf[LORA_TDM_APP_LINE_BUF_LEN];
    uint16  BufLen = 0;
    ssize_t Rc;
    char    C;
    uint32  DeadlineMs;
    uint32  NowMs;

    DeadlineMs = GetTimeMs() + LORA_TDM_APP_RX_WINDOW_MS;

    while (true)
    {
        NowMs = GetTimeMs();
        if (NowMs >= DeadlineMs)
        {
            break;
        }

        if (LORA_TDM_APP_Data.LoRaFd < 0)
        {
            break;
        }

        Rc = read(LORA_TDM_APP_Data.LoRaFd, &C, 1);
        if (Rc <= 0)
        {
            break;
        }

        if (BufLen < sizeof(Buf) - 1)
        {
            Buf[BufLen++] = C;
        }

        if (C == '\n')
        {
            Buf[BufLen] = '\0';
            LORA_TDM_APP_ProcessRxLine(Buf, &LORA_TDM_APP_Data);
            BufLen = 0;
        }
    }

    /* No ACK received this cycle */
    if (LORA_TDM_APP_Data.RxAckCount == 0 && LORA_TDM_APP_Data.NoAckCount < 0xFFFF)
    {
        LORA_TDM_APP_Data.NoAckCount++;
    }
}

/* ---- TX downlink ---- */

static void RunTx(void)
{
    char  Line[LORA_TDM_APP_LINE_BUF_LEN];
    int   Len;
    uint8 Type;

    if (LORA_TDM_APP_Data.LoRaFd < 0)
    {
        return;
    }

    /* Alternate FC and SH packets deterministically based on sequence number.
     * This avoids dependence on EKF_STATUS/SYSTEM_HEALTH message arrival order
     * when the SB pipe saturates under high ATTITUDE publish rates. */
    Type = ((LORA_TDM_APP_Data.DownlinkSeq % 2U) == 0U)
               ? LORA_TDM_APP_FC_STATE_PACKET_TYPE
               : LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE;

    if (Type == LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE)
    {
        Len = LORA_TDM_APP_BuildShDownlinkLine(Line, sizeof(Line), &LORA_TDM_APP_Data);
    }
    else
    {
        Len = LORA_TDM_APP_BuildFcDownlinkLine(Line, sizeof(Line), &LORA_TDM_APP_Data);
    }

    if (Len > 0)
    {
        if (write(LORA_TDM_APP_Data.LoRaFd, Line, (size_t)Len) == Len)
        {
            LORA_TDM_APP_Data.PacketType = Type;
            LORA_TDM_APP_Data.TxCount++;
            LORA_TDM_APP_Data.DownlinkSeq++;
            LORA_TDM_APP_Data.PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
        }
        else
        {
            CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_WRITE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: serial write failed");
        }
    }
}

/* ---- Housekeeping publish ---- */

void LORA_TDM_APP_ReportHousekeeping(void)
{
    LORA_TDM_APP_HkPayload_t *P = &LORA_TDM_APP_Data.HkTlm.Payload;

    P->CommandCounter       = LORA_TDM_APP_Data.CmdCounter;
    P->CommandErrorCounter  = LORA_TDM_APP_Data.ErrCounter;
    P->LinkState            = LORA_TDM_APP_Data.LinkState;
    P->PacketType           = LORA_TDM_APP_Data.PacketType;
    P->AttitudeValid        = LORA_TDM_APP_Data.FcState.AttitudeValid;
    P->LocalValid           = LORA_TDM_APP_Data.FcState.LocalValid;
    P->GpsValid             = LORA_TDM_APP_Data.FcState.GpsValid;
    P->EkfValid             = LORA_TDM_APP_Data.FcState.EkfValid;
    P->SystemHealthState    = LORA_TDM_APP_Data.SystemHealth.SystemHealthState;
    P->PendingUplinkFeedback = LORA_TDM_APP_Data.PendingUplinkFeedback;
    P->TxCount              = LORA_TDM_APP_Data.TxCount;
    P->RxAckCount           = LORA_TDM_APP_Data.RxAckCount;
    P->RxCmdCount           = LORA_TDM_APP_Data.RxCmdCount;
    P->RxErrorCount         = LORA_TDM_APP_Data.RxErrorCount;
    P->NoAckCount           = LORA_TDM_APP_Data.NoAckCount;
    P->LastAckTimestampMs   = LORA_TDM_APP_Data.LastAckTimestampMs;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(LORA_TDM_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(LORA_TDM_APP_Data.HkTlm.TelemetryHeader), true);
}

/* ---- Link status telemetry publish ---- */

void LORA_TDM_APP_ReportLinkStatus(void)
{
    LORA_TDM_APP_LinkStatusTlm_t *T = &LORA_TDM_APP_Data.LinkStatusTlm;

    T->Seq                = LORA_TDM_APP_Data.DownlinkSeq;
    T->TimestampMs        = GetTimeMs();
    T->LinkState          = LORA_TDM_APP_Data.LinkState;
    T->LastAckTimestampMs = LORA_TDM_APP_Data.LastAckTimestampMs;
    T->NoAckCount         = LORA_TDM_APP_Data.NoAckCount;
    T->RxErrorCount       = LORA_TDM_APP_Data.RxErrorCount;
    T->TxCount            = LORA_TDM_APP_Data.TxCount;
    T->RxAckCount         = LORA_TDM_APP_Data.RxAckCount;
    T->RxCmdCount         = LORA_TDM_APP_Data.RxCmdCount;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(T->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(T->TelemetryHeader), true);
}

/* ---- One TDM cycle ---- */

void LORA_TDM_APP_RunCycle(void)
{
    CFE_SB_Buffer_t *SBBufPtr;
    CFE_Status_t     Status;
    uint32           NowMs;

    /* Drain SB pipe (non-blocking) */
    while (true)
    {
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, LORA_TDM_APP_Data.CommandPipe, CFE_SB_POLL);
        if (Status != CFE_SUCCESS)
        {
            break;
        }
        LORA_TDM_APP_ProcessCommandPacket(SBBufPtr);
    }

    if (LORA_TDM_APP_Data.LoRaFd < 0)
    {
        LORA_TDM_APP_Data.LoRaFd = OpenSerial();
    }

    RunTx();
    RunRxWindow();

    NowMs = GetTimeMs();
    LORA_TDM_APP_UpdateLinkState(&LORA_TDM_APP_Data, NowMs);
}

/* ---- Init ---- */

CFE_Status_t LORA_TDM_APP_Init(void)
{
    CFE_Status_t Status;

    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));
    LORA_TDM_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    LORA_TDM_APP_Data.LoRaFd   = -1;

    Status = CFE_EVS_Register(NULL, 0, 0);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_CreatePipe(&LORA_TDM_APP_Data.CommandPipe, 200, "LORA_TDM_PIPE");
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_CMD_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_SEND_HK_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    CFE_MSG_Init(CFE_MSG_PTR(LORA_TDM_APP_Data.HkTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_HK_TLM_MID_VALUE),
                 sizeof(LORA_TDM_APP_Data.HkTlm));
    CFE_MSG_Init(CFE_MSG_PTR(LORA_TDM_APP_Data.LinkStatusTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_LINK_STATUS_MID_VALUE),
                 sizeof(LORA_TDM_APP_Data.LinkStatusTlm));

    CFE_EVS_SendEvent(LORA_TDM_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: initialized");
    return CFE_SUCCESS;
}

/* ---- Main entry point ---- */

void LORA_TDM_APP_Main(void)
{
    CFE_Status_t Status;

    CFE_ES_PerfLogEntry(0);

    Status = LORA_TDM_APP_Init();
    if (Status != CFE_SUCCESS)
    {
        LORA_TDM_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    while (CFE_ES_RunLoop(&LORA_TDM_APP_Data.RunStatus))
    {
        CFE_ES_PerfLogExit(0);
        OS_TaskDelay(LORA_TDM_APP_CYCLE_PERIOD_MS);
        CFE_ES_PerfLogEntry(0);

        LORA_TDM_APP_RunCycle();
    }

    if (LORA_TDM_APP_Data.LoRaFd >= 0)
    {
        close(LORA_TDM_APP_Data.LoRaFd);
    }

    CFE_ES_ExitApp(LORA_TDM_APP_Data.RunStatus);
}

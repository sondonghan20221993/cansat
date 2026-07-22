#include "lora_tdm_app.h"
#include "lora_tdm_app_dispatch.h"
#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"
#include "serial_baud.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

LORA_TDM_APP_Data_t LORA_TDM_APP_Data;

/* ---- Serial open ---- */

/* 테스트 환경(PTY mock 등)에서 serial 경로를 주입할 수 있도록 env var를
 * 우선 확인한다. 미설정 시 기존 컴파일타임 경로 사용 (실환경 동작 불변).
 * 상세: tests/TEST_CASES.md "그룹 B 실구현 계획" */
static const char *GetLoRaSerialPath(void)
{
    const char *EnvPath = getenv("LORA_TDM_SERIAL_PATH");

    if (EnvPath != NULL && EnvPath[0] != '\0')
    {
        return EnvPath;
    }
    return LORA_TDM_APP_LORA_SERIAL_PATH;
}

static int OpenSerial(void)
{
    int            Fd;
    struct termios Tio;
    speed_t        Baud;
    int            Flags;
    const char    *SerialPath = GetLoRaSerialPath();

    /* BL-19(2026-07-22): 하드코딩 B57600 대신 설정값(LORA_TDM_APP_LORA_BAUDRATE)을
     * 실제로 사용. 설정값이 지원 목록에 없으면(오타 등) B57600로 대체해 기존
     * 동작을 그대로 유지한다. */
    if (!SERIAL_BAUD_GetConstant(LORA_TDM_APP_LORA_BAUDRATE, &Baud))
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_OPEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: unsupported LORA_BAUDRATE=%u, defaulting to 57600",
                          (unsigned int)LORA_TDM_APP_LORA_BAUDRATE);
        Baud = B57600;
    }

    Fd = open(SerialPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (Fd < 0)
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_OPEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: open serial failed: %s", SerialPath);
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

/* ---- Serial close ----
 * write/read 오류(예: LoRa USB 런타임 분리) 시 fd를 닫고 -1로 되돌려
 * 다음 RunCycle의 재오픈 경로(LoRaFd < 0)가 동작하도록 한다. */
static void CloseSerial(void)
{
    if (LORA_TDM_APP_Data.LoRaFd >= 0)
    {
        close(LORA_TDM_APP_Data.LoRaFd);
        LORA_TDM_APP_Data.LoRaFd = -1;
    }
}

/* ---- Get timestamp in ms ---- */

static uint32 GetTimeMs(void)
{
    CFE_TIME_SysTime_t T = CFE_TIME_GetTime();
    uint64             Ms;

    Ms = ((uint64)T.Seconds * 1000ULL) + ((uint64)T.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)Ms;
}

/* ---- RX frame mode — 첫 바이트로 v1(ASCII 줄)/v2(매직바이트) 판별. §11.2/§8 ---- */
#define LORA_TDM_RXFRAME_NONE     0u /* 첫 바이트 대기 */
#define LORA_TDM_RXFRAME_TEXT     1u /* v1, '\n' 대기 */
#define LORA_TDM_RXFRAME_ACK2     2u /* 고정 5B */
#define LORA_TDM_RXFRAME_UP2_HDR  3u /* plen(2번째 바이트) 확정 전 */
#define LORA_TDM_RXFRAME_UP2_BODY 4u /* plen 확정, 길이만큼 채우는 중 */

/* ---- RX window: read until timeout or newline/frame 완성 ---- */

static void RunRxWindow(void)
{
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
        if (Rc == 0)
        {
            break; /* 이번 폴에 데이터 없음 (정상) */
        }
        if (Rc < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; /* 데이터 없음 */
            }
            /* 실제 오류 (USB 분리 등) → 닫아서 다음 사이클 재오픈 유도 */
            CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_READ_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: serial read failed errno=%d, closing for reopen", errno);
            CloseSerial();
            break;
        }

        /* [[lora_tdm_serial_reopen_gap]] §11.1: 버퍼가 앱 데이터(RunRxWindow 호출 경계를
         * 넘어 유지)라, 이번 창에서 완성이 안 돼도(개행 미도달/바이너리 프레임 중간)
         * 다음 창에서 이어받는다 — v2 UP2가 여러 RX창에 걸쳐 와도 유실 안 됨(§7.1). */
        if (LORA_TDM_APP_Data.RxLineBufLen < sizeof(LORA_TDM_APP_Data.RxLineBuf) - 1)
        {
            LORA_TDM_APP_Data.RxLineBuf[LORA_TDM_APP_Data.RxLineBufLen++] = C;
        }
        else
        {
            /* 오버플로: 재동기화 (그렇지 않으면 잘못 이어붙은 프레임이 영구 폐기됨). */
            LORA_TDM_APP_Data.RxLineBufLen  = 0;
            LORA_TDM_APP_Data.RxFrameMode   = LORA_TDM_RXFRAME_NONE;
            continue;
        }

        /* 첫 바이트로 모드 확정 — magic이면 v2, 아니면(ASCII) v1 텍스트 (§8) */
        if (LORA_TDM_APP_Data.RxLineBufLen == 1)
        {
            if ((uint8)C == (uint8)LORA_TDM_APP_ACK2_MAGIC)
            {
                LORA_TDM_APP_Data.RxFrameMode      = LORA_TDM_RXFRAME_ACK2;
                LORA_TDM_APP_Data.RxFrameTargetLen = 5;
            }
            else if ((uint8)C == (uint8)LORA_TDM_APP_UP2_MAGIC)
            {
                LORA_TDM_APP_Data.RxFrameMode = LORA_TDM_RXFRAME_UP2_HDR;
            }
            else
            {
                LORA_TDM_APP_Data.RxFrameMode = LORA_TDM_RXFRAME_TEXT;
            }
        }

        switch (LORA_TDM_APP_Data.RxFrameMode)
        {
            case LORA_TDM_RXFRAME_TEXT:
                if (C == '\n')
                {
                    LORA_TDM_APP_Data.RxLineBuf[LORA_TDM_APP_Data.RxLineBufLen] = '\0';
                    LORA_TDM_APP_ProcessRxLine(LORA_TDM_APP_Data.RxLineBuf, &LORA_TDM_APP_Data);
                    LORA_TDM_APP_Data.RxLineBufLen = 0;
                    LORA_TDM_APP_Data.RxFrameMode  = LORA_TDM_RXFRAME_NONE;
                }
                break;

            case LORA_TDM_RXFRAME_UP2_HDR:
                /* 2번째 바이트(plen) 도착 시 최종 길이 확정 후 BODY로 전환 */
                if (LORA_TDM_APP_Data.RxLineBufLen == 2)
                {
                    uint8 Plen = (uint8)LORA_TDM_APP_Data.RxLineBuf[1];
                    LORA_TDM_APP_Data.RxFrameTargetLen = (uint16)(7u + (uint16)Plen + 2u);
                    LORA_TDM_APP_Data.RxFrameMode      = LORA_TDM_RXFRAME_UP2_BODY;
                }
                break;

            case LORA_TDM_RXFRAME_ACK2:
            case LORA_TDM_RXFRAME_UP2_BODY:
                if (LORA_TDM_APP_Data.RxLineBufLen >= LORA_TDM_APP_Data.RxFrameTargetLen)
                {
                    LORA_TDM_APP_ProcessRxBinaryFrame((const uint8 *)LORA_TDM_APP_Data.RxLineBuf,
                                                       LORA_TDM_APP_Data.RxLineBufLen, &LORA_TDM_APP_Data);
                    LORA_TDM_APP_Data.RxLineBufLen = 0;
                    LORA_TDM_APP_Data.RxFrameMode  = LORA_TDM_RXFRAME_NONE;
                }
                break;

            default:
                break;
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

    /* v2(DL2)는 FC+SH를 한 프레임에 합쳐 보내므로(§4), v1의 짝/홀 교대 스케줄링이
     * 필요 없다 — 매 사이클 DL2 하나만 전송. */
    if (LORA_TDM_APP_Data.UseV2Downlink)
    {
        uint8 Dl2Buf[LORA_TDM_APP_DL2_MAX_FRAME_LEN];
        int   Dl2Len;

        Dl2Len = LORA_TDM_APP_BuildDl2Frame(Dl2Buf, sizeof(Dl2Buf), &LORA_TDM_APP_Data);
        if (Dl2Len > 0)
        {
            if (write(LORA_TDM_APP_Data.LoRaFd, Dl2Buf, (size_t)Dl2Len) == Dl2Len)
            {
                LORA_TDM_APP_Data.TxCount++;
                LORA_TDM_APP_Data.LastSentSeq = LORA_TDM_APP_Data.DownlinkSeq;
                LORA_TDM_APP_Data.DownlinkSeq++;
                /* PendingUplinkFeedback은 여기서 리셋하지 않는다 (2026-07-21) —
                 * uplink_app 처리 결과(UPLINK_STATUS_MID)가 SB 라운드트립 지연으로
                 * 이 사이클 안에 도착 못 하면, 확정된 판정(SEQ_FAIL/STATE_BLOCKED)이
                 * 다음 TX에서 곧바로 OK로 덮여써져 지상국이 놓치는 레이스가 있었음.
                 * 리셋은 ProcessUpFrame/ForwardUp2ToUplinkApp에서 새 명령을
                 * 포워딩할 때만 수행 — 판정값이 다음 명령 전까지 유지되어야
                 * 지상국이 여러 다운링크 사이클에 걸쳐 안정적으로 관측 가능. */
            }
            else
            {
                CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_WRITE_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "LORA_TDM_APP: serial write failed errno=%d, closing for reopen", errno);
                CloseSerial();
            }
        }
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
            LORA_TDM_APP_Data.LastSentSeq = LORA_TDM_APP_Data.DownlinkSeq;
            LORA_TDM_APP_Data.DownlinkSeq++;
            /* PendingUplinkFeedback 리셋 안 함 — v2 분기와 동일 이유(위 주석 참조) */
        }
        else
        {
            /* write 실패 (USB 분리 등) → 닫아서 다음 사이클 재오픈 유도 */
            CFE_EVS_SendEvent(LORA_TDM_APP_SERIAL_WRITE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: serial write failed errno=%d, closing for reopen", errno);
            CloseSerial();
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

    Status = CFE_SB_CreatePipe(&LORA_TDM_APP_Data.CommandPipe, 50, "LORA_TDM_PIPE");
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

    /* cfs_core_app force-publishes SYSTEM_HEALTH_MID immediately on every FC state update
     * (see cfs_core_app_behavior_spec.md §11.1), not on a 1Hz timer, so this needs the same
     * higher MsgLim as the FC_* MIDs below. */
    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 20);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    /* High-rate FC telemetry (up to 5 Hz) vs. ~1.3s RunCycle drain interval can exceed the
     * default MsgLim(4) within one cycle, so use a larger explicit limit here. */
    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 10);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 10);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 10);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 10);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_SYS_TIME_MID_VALUE),
                                 LORA_TDM_APP_Data.CommandPipe, CFE_SB_DEFAULT_QOS, 10);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_DIAGNOSTIC_CMD_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_STATUS_MID_VALUE),
                               LORA_TDM_APP_Data.CommandPipe);
    if (Status != CFE_SUCCESS)
    {
        return Status;
    }

    /* CONFIG_CMD_MID — cfs_core_app/mavlink_bridge_app와 공유 MID, scope(§lora_tdm_app_mission_cfg.h
     * LORA_TDM_APP_CONFIG_SCOPE)로 자기 것만 골라 처리. openMCT UPLINK_CLASS_CONFIG 경로로 도달 가능한
     * 유일한 실제 지상->기체 커맨드 채널. */
    Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LORA_TDM_APP_CONFIG_CMD_MID_VALUE),
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
    CFE_MSG_Init(CFE_MSG_PTR(LORA_TDM_APP_Data.ExecResultTlm.TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_EXEC_RESULT_MID_VALUE),
                 sizeof(LORA_TDM_APP_Data.ExecResultTlm));

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

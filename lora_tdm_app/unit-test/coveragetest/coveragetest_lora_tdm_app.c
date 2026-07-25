#include "lora_tdm_app_coveragetest_common.h"
#include "lora_tdm_app_internal.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void Test_Init(void)
{
    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.RunStatus, (int)CFE_ES_RunStatus_APP_RUN);
}

void Test_Init_SubscribeError(void)
{
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_Subscribe), 1, CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SB_BAD_ARGUMENT);
}

/* RunTx() write 실패 (closed fd -> EBADF) -> CloseSerial() -> LoRaFd == -1
 * [[lora_tdm_serial_reopen_gap]] P3 — write/read 실패 경로 자체를 검증 (mock 아닌
 * 실제 POSIX fd 조작; OpenSerial/RunTx/RunRxWindow는 raw open/read/write를 쓰므로
 * OSAL stub 대상이 아님). 단, 이 유닛(lora_tdm_app.c)을 단독 커버리지 대상으로 빌드할 때
 * lora_tdm_app_utils.c의 BuildFcDownlinkLine/BuildShDownlinkLine은 stub으로 대체되어
 * 기본 반환값 0(길이)이므로, 명시적으로 양수 길이를 설정해야 RunTx가 실제 write()에
 * 도달한다. */
void Test_RunCycle_TxWriteFailClosesFd(void)
{
    int Fd;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    LORA_TDM_APP_Data.UseV2Downlink = 0; /* BL-45: Init 기본값이 v2라 v1 write 경로 검증엔 명시 필요 */
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildFcDownlinkLine), 10);
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildShDownlinkLine), 10);

    Fd = open("/dev/null", O_WRONLY);
    UtAssert_True(Fd >= 0, "open /dev/null for close-then-use setup");
    close(Fd); /* 닫힌 fd 재사용 -> write()가 EBADF로 실패 보장 */
    LORA_TDM_APP_Data.LoRaFd = Fd;

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LoRaFd, -1);
}

/* UseV2Downlink=1이면 RunTx가 v1(FC/SH 텍스트) 대신 DL2 하나만 보내야 한다 — §4/§8. */
void Test_RunCycle_UseV2Downlink_SendsDl2Only(void)
{
    int Fd;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    LORA_TDM_APP_Data.UseV2Downlink = 1;
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildDl2Frame), (int)LORA_TDM_APP_DL2_FRAME_LEN);

    Fd = open("/dev/null", O_WRONLY);
    UtAssert_True(Fd >= 0, "open /dev/null for write");
    LORA_TDM_APP_Data.LoRaFd = Fd;

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.TxCount, 1);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.DownlinkSeq, 1);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_BuildFcDownlinkLine)) == 0,
                  "v2 모드에서 v1 FC 빌더 미호출");
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_BuildShDownlinkLine)) == 0,
                  "v2 모드에서 v1 SH 빌더 미호출");

    close(Fd);
    LORA_TDM_APP_Data.LoRaFd = -1;
}

/* RunRxWindow() read 실패 (write-only fd -> EBADF) -> CloseSerial() -> LoRaFd == -1
 * write는 /dev/null 대상이라 성공하므로 RunTx가 먼저 fd를 닫지 않음 -> RX 실패
 * 분기를 TX와 분리해서 검증. CFE_TIME_GetTime 기본 stub은 호출마다 값이 자동
 * 증가해서(호출당 +1000ms) RX 창 데드라인(RX_WINDOW_MS<1000ms)이 read() 호출 전에
 * 항상 지나버려 루프 진입 자체가 막히므로, 고정 시각으로 못박아야 read() 분기에
 * 도달한다. */
void Test_RunCycle_RxReadFailClosesFd(void)
{
    int                Fd;
    CFE_TIME_SysTime_t FakeTime;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    LORA_TDM_APP_Data.UseV2Downlink = 0; /* BL-45: Init 기본값이 v2 — v1 TX 후 RX 실패 경로 검증 */
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildFcDownlinkLine), 10);
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildShDownlinkLine), 10);

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 100U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    Fd = open("/dev/null", O_WRONLY);
    UtAssert_True(Fd >= 0, "open /dev/null write-only");
    LORA_TDM_APP_Data.LoRaFd = Fd;

    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LoRaFd, -1);
}

/* BL-60(2026-07-25): LORA_TDM_APP_RunTx()를 RunCycle 경유 없이 직접 호출해
 * write 실패(closed fd -> EBADF) 경로를 검증 — _internal.h로 노출된 함수를
 * UT가 직접 부른다(기존 Test_RunCycle_TxWriteFailClosesFd와 동일 원칙, 간접
 * 호출 대신 직접 호출로 커버리지 갭을 메운다). */
void Test_RunTx_Direct_WriteFailClosesFd(void)
{
    int Fd;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    LORA_TDM_APP_Data.UseV2Downlink = 0;
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildFcDownlinkLine), 10);
    UT_SetDefaultReturnValue(UT_KEY(LORA_TDM_APP_BuildShDownlinkLine), 10);

    Fd = open("/dev/null", O_WRONLY);
    UtAssert_True(Fd >= 0, "open /dev/null for close-then-use setup");
    close(Fd); /* 닫힌 fd 재사용 -> write()가 EBADF로 실패 보장 */
    LORA_TDM_APP_Data.LoRaFd = Fd;

    LORA_TDM_APP_RunTx();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LoRaFd, -1);
}

/* BL-60(2026-07-25): LORA_TDM_APP_RunRxWindow()를 RunCycle 경유 없이 직접
 * 호출해 read 실패(closed fd -> EBADF) 경로를 검증. */
void Test_RunRxWindow_Direct_ReadFailClosesFd(void)
{
    int                Fd;
    CFE_TIME_SysTime_t FakeTime;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 100U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    Fd = open("/dev/null", O_WRONLY); /* write-only -> read()가 EBADF로 실패 보장 */
    UtAssert_True(Fd >= 0, "open /dev/null write-only");
    LORA_TDM_APP_Data.LoRaFd = Fd;

    LORA_TDM_APP_RunRxWindow();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LoRaFd, -1);
}

/* [[lora_tdm_serial_reopen_gap]] §11.1: 줄이 RX창 경계에 걸쳐 도착해도(개행이
 * 이번 창 안에 안 옴) 다음 창에서 이어받아 완성된 줄로 ProcessRxLine에 전달돼야
 * 한다. 이전 구현(지역 변수 Buf)은 창이 끝나면 부분 줄을 그냥 버렸다 — 이 테스트는
 * 그 회귀를 방지한다.
 *
 * RunRxWindow()는 static이라 직접 호출 불가 — LORA_TDM_APP_RunCycle()을 두 번
 * 호출해서 간접 검증한다 (RunTx는 utils.c 스텁 기본값이 길이 0이라 write() 자체를
 * 안 하므로 fd 상태에 영향 없음 — Test_RunCycle_TxWriteFailClosesFd 주석 참조).
 * pipe()로 실제 fd 사용 (mock 아님 — 상단 파일 주석과 동일 원칙). */
void Test_RunRxWindow_LineSpansAcrossWindows(void)
{
    int                PipeFd[2];
    CFE_TIME_SysTime_t FakeTime;

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    UtAssert_True(pipe(PipeFd) == 0, "pipe() 생성");
    /* 논블로킹으로 설정 — RunRxWindow의 EAGAIN 처리 경로와 동일한 조건 재현 */
    UtAssert_True(fcntl(PipeFd[0], F_SETFL, O_NONBLOCK) == 0, "read fd 논블로킹 설정");

    LORA_TDM_APP_Data.LoRaFd = PipeFd[0];

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 100U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    /* 1차 창: 개행 없이 "ACK,1" 앞부분만 도착 */
    UtAssert_True(write(PipeFd[1], "ACK,", 4) == 4, "1차 창 partial write");
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.RxLineBufLen, 4);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxLine)) == 0,
                  "개행 전이라 ProcessRxLine 미호출");

    /* 2차 창: 나머지 "1\n" 도착 -> 이전 창의 4바이트와 이어붙어 완성돼야 함.
     * FakeTime 버퍼는 1차 창의 반복 GetTimeMs() 호출들에서 소진되므로, 2차 창
     * 진입 전에 다시 고정해야 데드라인 비교가 정상 동작한다 (안 하면 기본
     * 자동증가 스텁으로 넘어가 데드라인을 즉시 초과해 루프가 read() 전에 끝남). */
    UtAssert_True(write(PipeFd[1], "1\n", 2) == 2, "2차 창 나머지 write");
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.RxLineBufLen, 0);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxLine)) == 1,
                  "완성된 줄로 ProcessRxLine 1회 호출");

    close(PipeFd[0]);
    close(PipeFd[1]);
    LORA_TDM_APP_Data.LoRaFd = -1;
}

/* §11.2/§8: 첫 바이트가 v2 매직(0xA2=ACK2)이면 개행 대기 없이 길이(5B) 기반으로
 * 완성 판단해서 ProcessRxBinaryFrame으로 넘어가야 한다. */
void Test_RunRxWindow_Ack2MagicDispatchesToBinaryHandler(void)
{
    int                PipeFd[2];
    CFE_TIME_SysTime_t FakeTime;
    uint8              Frame[5] = {0xA2, 0x2A, 0x00, 0x00, 0x00}; /* 내용은 임의, 프레이밍만 검증 */

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    UtAssert_True(pipe(PipeFd) == 0, "pipe() 생성");
    UtAssert_True(fcntl(PipeFd[0], F_SETFL, O_NONBLOCK) == 0, "read fd 논블로킹 설정");
    LORA_TDM_APP_Data.LoRaFd = PipeFd[0];

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 100U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    UtAssert_True(write(PipeFd[1], Frame, sizeof(Frame)) == (ssize_t)sizeof(Frame), "ACK2 프레임 write");
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.RxLineBufLen, 0);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxBinaryFrame)) == 1,
                  "ACK2 매직으로 ProcessRxBinaryFrame 1회 호출");
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxLine)) == 0,
                  "v2 프레임은 v1 텍스트 파서로 안 감");

    close(PipeFd[0]);
    close(PipeFd[1]);
    LORA_TDM_APP_Data.LoRaFd = -1;
}

/* UP2 헤더(magic+plen)가 1차 창에, payload+CRC가 2차 창에 걸쳐 와도(§7.1) 상태(모드+목표
 * 길이)가 유지돼 정확히 완성 판단해야 한다. */
void Test_RunRxWindow_Up2FrameSpansAcrossWindows(void)
{
    int                PipeFd[2];
    CFE_TIME_SysTime_t FakeTime;
    /* plen=2, 나머지 바이트는 프레이밍 검증용이라 값은 임의 */
    uint8 Header[2] = {0xB2, 0x02};
    uint8 Rest[9]   = {0x02, 0x01, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0x00, 0x00}; /* ver,class,seq2,flags,payload2,crc2 */

    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    UtAssert_True(pipe(PipeFd) == 0, "pipe() 생성");
    UtAssert_True(fcntl(PipeFd[0], F_SETFL, O_NONBLOCK) == 0, "read fd 논블로킹 설정");
    LORA_TDM_APP_Data.LoRaFd = PipeFd[0];

    memset(&FakeTime, 0, sizeof(FakeTime));
    FakeTime.Seconds = 100U;
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);

    /* 1차 창: magic+plen만 도착 */
    UtAssert_True(write(PipeFd[1], Header, sizeof(Header)) == (ssize_t)sizeof(Header), "1차 창 헤더 write");
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.RxLineBufLen, 2);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxBinaryFrame)) == 0,
                  "헤더만 와서 아직 미완성");

    /* 2차 창: 나머지(ver..crc) 도착 -> plen=2 기준 목표 길이(11B) 도달, 완성 */
    UtAssert_True(write(PipeFd[1], Rest, sizeof(Rest)) == (ssize_t)sizeof(Rest), "2차 창 나머지 write");
    UT_SetDataBuffer(UT_KEY(CFE_TIME_GetTime), &FakeTime, sizeof(FakeTime), false);
    UT_SetDeferredRetcode(UT_KEY(CFE_SB_ReceiveBuffer), 1, CFE_SB_NO_MESSAGE);
    LORA_TDM_APP_RunCycle();
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.RxLineBufLen, 0);
    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_ProcessRxBinaryFrame)) == 1,
                  "창 경계 넘어 이어받아 완성 시 1회 호출");

    close(PipeFd[0]);
    close(PipeFd[1]);
    LORA_TDM_APP_Data.LoRaFd = -1;
}

/* -----------------------------------------------------------------------
 * ReportHousekeeping — HK payload 필드 복사 확인
 * ----------------------------------------------------------------------- */
void Test_ReportHousekeeping(void)
{
    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));

    LORA_TDM_APP_Data.CmdCounter                     = 3;
    LORA_TDM_APP_Data.ErrCounter                     = 1;
    LORA_TDM_APP_Data.LinkState                      = 2;
    LORA_TDM_APP_Data.PacketType                     = 1;
    LORA_TDM_APP_Data.FcState.AttitudeValid          = 1;
    LORA_TDM_APP_Data.FcState.LocalValid             = 1;
    LORA_TDM_APP_Data.FcState.GpsValid               = 0;
    LORA_TDM_APP_Data.FcState.EkfValid               = 1;
    LORA_TDM_APP_Data.SystemHealth.SystemHealthState = 2;
    LORA_TDM_APP_Data.PendingUplinkFeedback          = 1;
    LORA_TDM_APP_Data.TxCount                        = 10;
    LORA_TDM_APP_Data.RxAckCount                     = 8;
    LORA_TDM_APP_Data.RxCmdCount                     = 4;
    LORA_TDM_APP_Data.RxErrorCount                   = 2;
    LORA_TDM_APP_Data.NoAckCount                     = 1;
    LORA_TDM_APP_Data.LastAckTimestampMs             = 12345;

    LORA_TDM_APP_ReportHousekeeping();

    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.CommandCounter,        3);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.CommandErrorCounter,   1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.LinkState,             2);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.PacketType,            1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.AttitudeValid,         1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.LocalValid,            1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.GpsValid,              0);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.EkfValid,              1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.SystemHealthState,     2);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.PendingUplinkFeedback, 1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.TxCount,              10);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.RxAckCount,            8);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.RxCmdCount,            4);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.RxErrorCount,          2);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.NoAckCount,            1);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.HkTlm.Payload.LastAckTimestampMs, 12345);
}

/* -----------------------------------------------------------------------
 * ReportLinkStatus — LinkStatus TLM 필드 반영 확인
 * ----------------------------------------------------------------------- */
void Test_ReportLinkStatus(void)
{
    memset(&LORA_TDM_APP_Data, 0, sizeof(LORA_TDM_APP_Data));

    LORA_TDM_APP_Data.DownlinkSeq         = 42;
    LORA_TDM_APP_Data.LinkState           = LORA_TDM_APP_LINK_DEGRADED;
    LORA_TDM_APP_Data.LastAckTimestampMs  = 5000;
    LORA_TDM_APP_Data.NoAckCount          = 3;
    LORA_TDM_APP_Data.RxErrorCount        = 2;
    LORA_TDM_APP_Data.TxCount             = 100;
    LORA_TDM_APP_Data.RxAckCount          = 90;
    LORA_TDM_APP_Data.RxCmdCount          = 7;

    LORA_TDM_APP_ReportLinkStatus();

    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.LinkStatusTlm.Seq,          42);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkStatusTlm.LinkState,
                      LORA_TDM_APP_LINK_DEGRADED);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.LinkStatusTlm.LastAckTimestampMs, 5000);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkStatusTlm.NoAckCount,          3);
    UtAssert_INT32_EQ(LORA_TDM_APP_Data.LinkStatusTlm.RxErrorCount,        2);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.LinkStatusTlm.TxCount,      100);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.LinkStatusTlm.RxAckCount,    90);
    UtAssert_INT32_EQ((int)LORA_TDM_APP_Data.LinkStatusTlm.RxCmdCount,     7);
}

/* BL-41(2026-07-23): Init()이 LoadState()를 호출해 저장된 UseV2Downlink를
 * 복원하는지 배선 검증 — 이 앱의 첫 영속 상태(TDD red) */
void Test_Init_RestoresPersistedConfig(void)
{
    /* 이 테스트러너에서 utils는 stub — 값 복원은 utils 테스트(RoundTrip)가
     * 담당하고, 여기서는 Init→LoadState 배선(stub count)만 증명한다. */
    UtAssert_INT32_EQ(LORA_TDM_APP_Init(), CFE_SUCCESS);

    UtAssert_True(UT_GetStubCount(UT_KEY(LORA_TDM_APP_LoadState)) == 1,
                  "Init()이 LoadState()를 정확히 1회 호출");
}

void UtTest_Setup(void)
{
    ADD_TEST(Init);
    ADD_TEST(Init_RestoresPersistedConfig);
    ADD_TEST(Init_SubscribeError);
    ADD_TEST(ReportHousekeeping);
    ADD_TEST(ReportLinkStatus);
    ADD_TEST(RunCycle_TxWriteFailClosesFd);
    ADD_TEST(RunCycle_UseV2Downlink_SendsDl2Only);
    ADD_TEST(RunCycle_RxReadFailClosesFd);
    ADD_TEST(RunTx_Direct_WriteFailClosesFd);
    ADD_TEST(RunRxWindow_Direct_ReadFailClosesFd);
    ADD_TEST(RunRxWindow_LineSpansAcrossWindows);
    ADD_TEST(RunRxWindow_Ack2MagicDispatchesToBinaryHandler);
    ADD_TEST(RunRxWindow_Up2FrameSpansAcrossWindows);
}

#include "lora_tdm_app_coveragetest_common.h"
#include <fcntl.h>
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

void UtTest_Setup(void)
{
    ADD_TEST(Init);
    ADD_TEST(Init_SubscribeError);
    ADD_TEST(RunCycle_TxWriteFailClosesFd);
    ADD_TEST(RunCycle_RxReadFailClosesFd);
    ADD_TEST(RunRxWindow_LineSpansAcrossWindows);
}

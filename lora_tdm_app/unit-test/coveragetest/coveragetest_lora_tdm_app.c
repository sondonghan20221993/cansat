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

void UtTest_Setup(void)
{
    ADD_TEST(Init);
    ADD_TEST(Init_SubscribeError);
    ADD_TEST(RunCycle_TxWriteFailClosesFd);
    ADD_TEST(RunCycle_RxReadFailClosesFd);
}

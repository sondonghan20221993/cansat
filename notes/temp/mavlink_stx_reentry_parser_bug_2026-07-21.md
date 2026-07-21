# mavlink_bridge_app 파서 STX 재진입 버그 — CRC fail 근본 원인 (2026-07-21)

## 배경

FC-Pi UART4 baud를 57600→921600→460800으로 올리는 과정에서, baud와 무관하게
`crc fail msgid=2/24/32/33` 이 지속 발생. 클럭 고정(`init_uart_clock`)도
효과 없어서 전기적/타이밍 문제가 아니라고 판단, 파서 코드 직접 확인.

## 원인

`mavlink_bridge_app_utils.c:MAVLINK_BRIDGE_APP_ProcessReceivedByte()` (라인 1637~):

```c
static void MAVLINK_BRIDGE_APP_ProcessReceivedByte(uint8 Byte, uint32 RxTimestampMs)
{
    if (Byte == MAVLINK_STX_V1)       /* 상태 무관하게 항상 검사 */
    {
        MAVLINK_BRIDGE_APP_ResetParser();
        ...
        return;
    }
    else if (Byte == MAVLINK_STX_V2)  /* 상태 무관하게 항상 검사 */
    {
        MAVLINK_BRIDGE_APP_ResetParser();
        ...
        return;
    }
    switch (MAVLINK_BRIDGE_APP_Parser.State) { ... }
}
```

STX(`0xFE`=v1, `0xFD`=v2) 검사가 파서 상태(`WAIT_STX`/`READING_PAYLOAD`/`GOT_CRC1` 등)와
무관하게 **모든 수신 바이트에 대해 최우선으로** 실행됨. 프레임을 파싱하는 도중
(페이로드나 CRC 바이트 중)에 값이 우연히 `0xFD`/`0xFE`이면, 진행 중이던 프레임을
버리고 그 바이트를 새 프레임 시작으로 오인 → 뒤따르는 바이트들이 다른 오프셋으로
밀려서 읽히고 CRC 불일치 발생.

## 왜 baud 상향 후에 급증했는가

57600에서는 PX4 쪽 대역폭 스로틀링(`tx rate mult=0.137`)으로 해당 메시지들이
거의 전송 안 돼서 증상이 드물게만 보였음. baud를 올려 스로틀링을 해소하자
GPS_RAW_INT(lat/lon int32), GLOBAL_POSITION_INT, SYSTEM_TIME(uint64) 같은
**고엔트로피 페이로드**가 대량으로 흐르게 됐고, 페이로드 바이트 중 0xFD/0xFE가
우연히 등장할 확률이 급증 → 거의 모든 프레임이 파서 재진입으로 깨짐.

crc fail이 찍힌 msgid(2=SYSTEM_TIME, 24=GPS_RAW_INT, 32=LOCAL_POSITION_NED,
33=GLOBAL_POSITION_INT)가 전부 고엔트로피 메시지라는 점도 이 설명과 일치.

## 결론

- baud rate(57600/460800/921600) 자체는 원인 아님 — 셋 다 이 버그의 영향을 받음.
- 라즈베리 파이 UART 클럭 고정(`init_uart_clock=48000000`)도 무관한 조치였음
  (부작용 없고 유지해도 무방하나, 이 버그의 해결책은 아니었음).
- 진짜 원인은 소프트웨어 파서의 STX 재진입 처리 버그.

## 수정 완료

- [x] STX 검사를 `MAVLINK_PARSE_WAIT_STX` 상태일 때만 수행하도록 변경
      (`mavlink_bridge_app_utils.c:MAVLINK_BRIDGE_APP_ProcessReceivedByte`)
- [x] 페이로드에 0xFD/0xFE(STX_V2/STX_V1)가 포함된 ATTITUDE 프레임이 정상
      파싱되는지 회귀 UT 추가 (`Test_ProcessReceivedByte_StxByteInPayload_NoReentry`) —
      구버전 코드에서는 이 테스트가 실패했을 것(파서 재진입으로 Valid=0 유지)
- [x] 로컬 UT: 해당 바이너리 161/161 PASS, 4개 앱 전체 회귀 16/16 PASS
- [ ] Pi 배포 후 `journalctl`로 crc fail 실제 소멸 확인 (미착수)

## 관련
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c`
  (`MAVLINK_BRIDGE_APP_ProcessReceivedByte`, 라인 1637~1747)
- `notes/temp/fc_telemetry_rate_1_2hz_duplicate.md` (baud 상향 작업 배경)

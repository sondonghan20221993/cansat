#!/bin/sh
# msposd 공중부(air unit) 번인 OSD 기동 — RunCam WiFiLink V2 (SSC338Q)
# 참조: https://github.com/OpenIPC/msposd (--osd = air side render, 영상에 직접 번인)
# 설치 위치: 카메라 /usr/bin/msposd_air.sh, 부팅 스크립트에서 호출
#
# TODO(bench): FC UART 디바이스 확인 — WiFiLink 4핀 커넥터가 매핑된 tty
#   후보: /dev/ttyS2 (SSC338Q OpenIPC 관례). `ls /dev/ttyS*` + 배선 후 `cat`으로 확인.
# TODO(bench): RunCam 공장 이미지에 msposd가 이미 서비스로 떠 있는지 확인
#   (`ps | grep msposd`). 떠 있으면 이 스크립트 불필요 — 인자만 대조할 것.

FC_UART=/dev/ttyS2
BAUD=115200

# 기존 인스턴스 정리 후 기동
killall msposd 2>/dev/null

# --master  : FC MSP DisplayPort 수신 UART
# --osd     : air unit에서 영상 스트림에 직접 렌더(번인) — 수신기 무관하게 OSD 보임
# -r 20     : OSD 갱신 20Hz
# --ahi 0   : 인공수평선 비활성 (FC OSD 요소로 대체)
msposd --master "$FC_UART" --baudrate "$BAUD" --osd -r 20 --ahi 0 &

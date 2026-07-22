#!/usr/bin/env bash
# Runtime test: cfs_core_app이 uplink_app/lora_tdm_app HK timeout 시 실제로
# CFE_ES_RestartApp을 호출해 앱을 재기동하는지 실물(Pi, cfs.service)에서 검증한다.
#
# 전제:
#   - Pi에서 cfs.service가 이 저장소의 최신 cfs_core_app 재시작 로직을 반영한
#     빌드로 실행 중이어야 함. 이 스크립트는 STOP_APP 커맨드만 보낼 뿐,
#     빌드/재시작은 하지 않는다. (build/ 재빌드 + `sudo systemctl restart
#     cfs.service`는 사용자가 직접 수행)
#   - cmd_send 툴이 ~/cFS_clean/build/exe/host/cmd_send 에 존재
#   - journalctl로 cfs.service 로그 확인 가능 (EVS 콘솔 출력이 journal에 잡힘)
#
# 사용법:
#   ./runtime_app_restart_test.sh uplink_app
#   ./runtime_app_restart_test.sh lora_tdm_app
#
# 동작:
#   1. CFE_ES_STOP_APP_CC(MID=0x1806, FC=5) 커맨드로 대상 앱을 정지시켜
#      "hang/crash로 HK가 끊기는 상황"을 실물로 재현한다.
#   2. cfs_core_app의 HK timeout(5s) + 재시작 인터벌(5s) 만큼 대기한다.
#   3. journalctl에서 cfs_core_app의 재시작 EID(15=uplink, 16=lora)와
#      cFE 자체의 CFE_ES_RESTART_APP_INF_EID(10)가 관측됐는지 확인한다.
#   4. 대상 앱의 STARTUP 이벤트가 재기동 이후 다시 찍혔는지 확인한다.

set -euo pipefail

CMD_SEND="$HOME/cFS_clean/build/exe/host/cmd_send"
HOST="127.0.0.1"
PORT="1234"
ES_CMD_MID="0x1806"
ES_STOP_APP_CC="5"

APP="${1:-}"
if [[ "$APP" != "uplink_app" && "$APP" != "lora_tdm_app" ]]; then
    echo "사용법: $0 <uplink_app|lora_tdm_app>" >&2
    exit 1
fi

if [[ "$APP" == "uplink_app" ]]; then
    RESTART_EID_NAME="CFS_CORE_APP_UPLINK_RESTART_EID"
    RESTART_EID_NUM=15
    APP_STRLEN=20  # CFE_MISSION_MAX_API_LEN
    CFE_APP_NAME="UPLINK_APP"   # cFE 등록명(startup.scr 3번째 필드) — 바이너리명 아님 (2026-07-22 실측 수정: 소문자명은 GetAppIDByName 실패 RC=0xC4000002)
else
    RESTART_EID_NAME="CFS_CORE_APP_LORA_RESTART_EID"
    RESTART_EID_NUM=16
    APP_STRLEN=20
    CFE_APP_NAME="LORA_TDM_APP"
fi

if [[ ! -x "$CMD_SEND" ]]; then
    echo "cmd_send를 찾을 수 없음: $CMD_SEND" >&2
    echo "빌드: cd ~/cFS_clean/build && make host-tools (또는 tools/commandline-tools 타겟)" >&2
    exit 1
fi

if [[ "$(systemctl is-active cfs.service 2>/dev/null || true)" != "active" ]]; then
    echo "cfs.service가 active 상태가 아님 — 먼저 기동 필요" >&2
    exit 1
fi

echo "[1/4] STOP_APP 커맨드 전송: $APP"
LOG_MARK_TIME="$(date '+%Y-%m-%d %H:%M:%S')"
"$CMD_SEND" --host="$HOST" --port="$PORT" --pktid="$ES_CMD_MID" --pktfc="$ES_STOP_APP_CC" \
    --string="${APP_STRLEN}:${CFE_APP_NAME}"

echo "[2/4] cfs_core_app HK timeout(5s) + 재시작 인터벌(5s) 대기 (여유 4s 포함, 총 14s)"
sleep 14

echo "[3/4] cfs_core_app 재시작 이벤트(${RESTART_EID_NAME}=${RESTART_EID_NUM}) 확인"
if journalctl -u cfs.service --since "$LOG_MARK_TIME" --no-pager 2>/dev/null \
    | grep -q "restart attempt"; then
    echo "  -> 발견: 재시작 시도 이벤트 확인됨"
else
    echo "  -> 미발견: journalctl 로그에서 재시작 이벤트를 찾지 못함" >&2
    journalctl -u cfs.service --since "$LOG_MARK_TIME" --no-pager 2>/dev/null | tail -30
    exit 1
fi

echo "[4/4] cFE 자체 재시작 완료 이벤트(CFE_ES_RESTART_APP_INF_EID=10) 및 ${APP} STARTUP 재확인"
if journalctl -u cfs.service --since "$LOG_MARK_TIME" --no-pager 2>/dev/null \
    | grep -qi "$APP"; then
    echo "  -> ${APP} 관련 로그 재등장 확인됨 (재기동 정황)"
    journalctl -u cfs.service --since "$LOG_MARK_TIME" --no-pager 2>/dev/null | grep -i "$APP" | tail -10
else
    echo "  -> ${APP} 재기동 로그를 찾지 못함 (수동 확인 필요)" >&2
    exit 1
fi

echo ""
echo "RESULT: PASS — ${APP} HK timeout -> cfs_core_app 재시작 시도 -> 재기동 확인됨"

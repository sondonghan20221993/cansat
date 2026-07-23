#include "lora_tdm_app_cmds.h"
#include "lora_tdm_app.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app_utils.h" /* BL-41: SaveState */

CFE_Status_t LORA_TDM_APP_Noop(const LORA_TDM_APP_NoopCmd_t *Msg)
{
    (void)Msg;

    LORA_TDM_APP_Data.CmdCounter++;
    CFE_EVS_SendEvent(LORA_TDM_APP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: NOOP command");
    return CFE_SUCCESS;
}

CFE_Status_t LORA_TDM_APP_ResetCounters(const LORA_TDM_APP_ResetCountersCmd_t *Msg)
{
    (void)Msg;

    LORA_TDM_APP_Data.CmdCounter   = 0;
    LORA_TDM_APP_Data.ErrCounter   = 0;
    LORA_TDM_APP_Data.TxCount      = 0;
    LORA_TDM_APP_Data.RxAckCount   = 0;
    LORA_TDM_APP_Data.RxCmdCount   = 0;
    LORA_TDM_APP_Data.RxErrorCount = 0;
    LORA_TDM_APP_Data.NoAckCount   = 0;

    CFE_EVS_SendEvent(LORA_TDM_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: ResetCounters command");
    return CFE_SUCCESS;
}

/* §8 단계적 전환: 지상에서 검증 후 이 명령으로 v2(DL2)로 전환. 기본값은 여전히 v1
 * (Init의 memset(0)) — 이 명령을 받기 전엔 항상 v1로 동작. */
/* BL-16(2026-07-21): 0/1 외 값은 거부(기체 엄격화) — 지상 PARAM_BOUNDS(0,1)와
 * 대칭 일치. 과거엔 (Value != 0)을 전부 1로 클램프해 값 2도 조용히 v2로
 * 수용했음(향후 프로토콜 버전 확장 여지를 의도치 않게 차단하던 문제). */
CFE_Status_t LORA_TDM_APP_SetDownlinkProtocol(const LORA_TDM_APP_SetDownlinkProtocolCmd_t *Msg)
{
    if (Msg->UseV2 != 0 && Msg->UseV2 != 1)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_ERR_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: rejected downlink protocol value=%u (only 0/1 accepted)",
                          (unsigned int)Msg->UseV2);
        return CFE_SUCCESS;
    }

    LORA_TDM_APP_Data.UseV2Downlink = (Msg->UseV2 != 0) ? 1 : 0;
    LORA_TDM_APP_Data.CmdCounter++;
    LORA_TDM_APP_SaveState(); /* BL-41(2026-07-23): 두 번째 변이 지점 — 성공 즉시 영속화 */

    CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: downlink protocol set to %s",
                      LORA_TDM_APP_Data.UseV2Downlink ? "v2(DL2)" : "v1(text)");
    return CFE_SUCCESS;
}

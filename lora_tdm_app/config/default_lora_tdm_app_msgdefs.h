#ifndef DEFAULT_LORA_TDM_APP_MSGDEFS_H
#define DEFAULT_LORA_TDM_APP_MSGDEFS_H

#include "common_types.h"
#include "lora_tdm_app_fcncodes.h"

typedef enum
{
    LORA_TDM_APP_DIAG_ACTION_LINK_STATUS = 0,
    LORA_TDM_APP_DIAG_ACTION_RX_STATS    = 1,
    LORA_TDM_APP_DIAG_ACTION_TX_STATS    = 2
} LORA_TDM_APP_DiagAction_t;

/* DIAGNOSTIC_CMD_TLM_t.DiagTarget 값 — cfs_core_app이 같은 MID의 구독
 * 대상으로 추가되며 도입(waypoint readback, 2026-07-23). cfs_core_app/config에
 * 동일 값으로 독립 재선언(공유 헤더 없음 관례). */
#define LORA_TDM_APP_DIAG_TARGET_LORA_TDM  0U /* 하위호환 기본값(자기 자신) */
#define LORA_TDM_APP_DIAG_TARGET_CFS_CORE  1U

#endif

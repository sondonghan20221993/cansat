#ifndef EXEC_RESULT_MSG_H
#define EXEC_RESULT_MSG_H

#include "cfe_msg_hdr.h"

/* BL-08(2026-07-22): 대상 앱(cfs_core_app/mavlink_bridge_app/lora_tdm_app)이
 * uplink_app으로부터 전달받은 명령을 처리 완료한 뒤 결과를 회신하는 공용
 * 채널. 3개 앱이 각자 다른 세부 결과 스키마를 갖고 있어(2개는 동일한 7종
 * CONFIG_RESULT enum 중복 보유, 1개는 아예 없음) 스키마를 통일하는 대신
 * "성공/실패" 대분류(GenericResult) + 원시 세부코드(DetailCode, 진단용
 * 참고자료일 뿐 uplink_app은 해석하지 않음)로 단순화. uplink_app은 이
 * MID 하나만 구독하면 됨(앱별 개별 채널 대신 공용 1개, 2026-07-22 결정).
 *
 * 상관(correlation): SourceSequence로 원본 지상 명령의 시퀀스를 그대로
 * 반사(echo) — uplink_app이 전달 시 각 대상 CMD 페이로드에 이미 싣고
 * 있던 값(예: config_msg.h의 SourceSequence)을 그대로 돌려보내면 됨.
 *
 * 타임아웃 없음(2026-07-22 결정) — uplink_app은 SourceSequence가 현재
 * 추적 중인 최신 명령과 일치할 때만 반영하고, 새 명령이 들어오면 이전
 * PENDING/응답은 자연히 무시된다(덮어쓰기). 대상 앱이 죽거나 응답이
 * 누락돼도 uplink_app이 무한정 PENDING에 머무르지 않음 — 다음 명령이
 * 오면 그냥 지나감. */

typedef enum
{
    EXEC_RESULT_SOURCE_CFS_CORE      = 1, /* CONFIG_SCOPE 값과 동일하게 정렬 */
    EXEC_RESULT_SOURCE_MAVLINK_BRIDGE = 2,
    EXEC_RESULT_SOURCE_LORA_TDM      = 3
} EXEC_RESULT_Source_t;

typedef enum
{
    EXEC_RESULT_GENERIC_OK     = 0,
    EXEC_RESULT_GENERIC_FAILED = 1
} EXEC_RESULT_Generic_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint16 SourceSequence; /* 원본 지상 명령 seq 반사 — uplink_app의 상관 키 */
    uint8  SourceApp;      /* EXEC_RESULT_Source_t */
    uint8  CommandClass;   /* UPLINK_APP_CommandClass_t 반사 — 어느 전달경로 결과인지 */
    uint8  GenericResult;  /* EXEC_RESULT_Generic_t — uplink_app이 실제로 쓰는 값 */
    uint8  DetailCode;     /* 대상 앱의 원시 결과코드(예: MAVLINK_BRIDGE_CONFIG_RESULT_BAD_PARAM=3).
                             * 진단/로그 참고용, uplink_app은 해석하지 않음 */
    uint8  Reserved[3];
} EXEC_RESULT_TLM_t;

#endif

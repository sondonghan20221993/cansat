#ifndef ROUTE_SNAPSHOT_MSG_H
#define ROUTE_SNAPSHOT_MSG_H

#include "route_msg.h"

/*
 * waypoint readback(2026-07-23): cfs_core_app 발행(ROUTE_SNAPSHOT_MID
 * 0x1913), lora_tdm_app 구독 — 단일 진실. ROUTE_UPDATE_TLM_t(지상→
 * cfs_core_app 방향)와 와이어 레이아웃이 동일해 별도 구조체 없이 재사용
 * (cfs_core_app→lora_tdm_app 방향, DIAGNOSTIC 요청에 대한 응답).
 */
typedef ROUTE_UPDATE_TLM_t ROUTE_SNAPSHOT_TLM_t;

#endif

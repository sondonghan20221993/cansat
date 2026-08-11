/*
 * Mission override: cFE Time Services mission configuration
 *
 * 목적: cFE 기본 시간 에폭(1980년)을 표시용으로 2026년으로 변경한다.
 *
 * 배경:
 *  - cFE CFE_TIME은 지상 SET_TIME 동기화가 없으면 "에폭 + 부팅후 경과시간(MET)"으로
 *    시각을 표시하므로, 기본 에폭 1980년 때문에 텔레메트리 시각이 1980년대로 나온다.
 *  - health/CONFIG/타임아웃 로직은 전부 MET(boot_ms) 기준이라 에폭 변경은 기능에 영향 없음.
 *    실제 벽시계 동기화가 아니라 "표시 연도만" 2026으로 맞추는 목적.
 *
 * 방식:
 *  - 라이브러리 원본(cfe_time_interface_cfg.h)을 수정하지 않고, mission_defs override로 처리.
 *  - cFS 빌드(generate_configfile_set)는 MISSION_DEFS/config 를 먼저 검색하므로,
 *    이 파일이 라이브러리 default_cfe_time_mission_cfg.h 를 대체한다.
 *  - 기본 정의를 그대로 include한 뒤 CFE_MISSION_TIME_EPOCH_YEAR 만 재정의한다.
 *
 * 주의: 실제 실시각이 필요하면 에폭 변경이 아니라 지상 SET_TIME 동기화가 필요하다.
 */
#ifndef CFE_TIME_MISSION_CFG_H
#define CFE_TIME_MISSION_CFG_H

/* 라이브러리 기본 정의(에폭 1980 등) 로드 */
#include "cfe_time_interface_cfg.h"

/* 표시용 에폭 연도 override: 1980 -> 2026 */
#undef CFE_MISSION_TIME_EPOCH_YEAR
#define CFE_MISSION_TIME_EPOCH_YEAR 2026

#endif /* CFE_TIME_MISSION_CFG_H */

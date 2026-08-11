#ifndef LORA_TDM_APP_INTERNAL_H
#define LORA_TDM_APP_INTERNAL_H

/* BL-60(2026-07-25): RunRxWindow/RunTx는 lora_tdm_app.c 안에서는 static을
 * 유지하되(내부 링크지 노출은 UT 전용), UT가 write/read syscall 실패 경로
 * (close(fd)+fd=-1)를 직접 호출로 검증할 수 있도록 이 헤더로만 노출한다.
 * 파일 스코프 전역 상태(LORA_TDM_APP_Data)를 그대로 사용하는 함수라 wrapper
 * 함수 추가보다 static 제거가 코드 중복이 적음 — 기존 프로젝트 패턴(다른 앱들의
 * 전역-스텁-데이터 UT 방식)과 일관. */

void LORA_TDM_APP_RunRxWindow(void);
void LORA_TDM_APP_RunTx(void);

#endif

# 변경 내역

## 개발 빌드: stable-rc1+dev54
- 'Fix #237, 582 표준을 반영하도록 샘플 앱 업데이트'
- 참조: <https://github.com/nasa/telemetry_app/pull/238>


## 개발 빌드: equuleus-rc1+dev50
- AddressSanitizer 이슈 수정
- <https://github.com/nasa/telemetry_app/pull/235> 참조

## 개발 빌드: stable-rc1+dev46
- 나머지 int32 CFE 상태 변수를 CFE_Status_t로 변환
- 초기화 중 syslog 쓰기를 이벤트로 변환
- <https://github.com/nasa/telemetry_app/pull/218> 및 <https://github.com/nasa/telemetry_app/pull/216>을 참조하세요.

## 개발 빌드: equuleus-rc4+dev40
- EDS 디스패처에 이름 변경 사항 적용
- <https://github.com/nasa/telemetry_app/pull/229> 참조

## 개발 빌드: equuleus-rc1+dev36
- 새로운 버전 관리 시스템을 사용하도록 telemetry_app 업데이트
- <https://github.com/nasa/telemetry_app/pull/226> 참조

## 개발 빌드: v1.3.0-rc4+dev69
- topicids를 통해 msgid 정의
- <https://github.com/nasa/telemetry_app/pull/220> 참조

## 개발 빌드: v1.3.0-rc4+dev65
- telemetry_app을 현재 규약에 맞게 정리
- CommandCode 변수의 이름을 FcnCode로 바꿉니다.
- 초기화 중 CFE_TBL_Load() 성공 여부 확인 추가
- <https://github.com/nasa/telemetry_app/pull/212>, <https://github.com/nasa/telemetry_app/pull/187> 및 <https://github.com/nasa/telemetry_app/pull/190>을 참조하세요.

## 개발 빌드: v1.3.0-rc4+dev56
- 공통 이벤트에 일관된 이벤트 ID 이름 적용
- 구성요소별 cFE 헤더 #include 제거
- TELEMETRY_APP_Init/Process를 리팩토링하여 여러 반환을 제거합니다.
- TELEMETRY_APP_Process()에 누락된 분기에 대한 테스트를 추가합니다.
- 초기화 중 전역 데이터 구조를 0으로 만듭니다.
- cmd와 util을 별도의 파일로 이동
- 현재 패턴에 따라 소스 파일 구성
- <https://github.com/nasa/telemetry_app/pull/189>, <https://github.com/nasa/telemetry_app/pull/195>, <https://github.com/nasa/telemetry_app/pull/198>, <https://github.com/nasa/telemetry_app/pull/200>, <https://github.com/nasa/telemetry_app/pull/201>, <https://github.com/nasa/telemetry_app/pull/205> 및 <https://github.com/nasa/telemetry_app/pull/208>을 참조하세요.

## 개발 빌드: v1.3.0-rc4+dev39
- cmake 레시피 업데이트
- <https://github.com/nasa/telemetry_app/pull/203> 참조

## 개발 빌드: v1.3.0-rc4+dev35
- 중복된 댓글 삭제
- CHANGELOG.md 생성
- <https://github.com/nasa/telemetry_app/pull/185> 및 <https://github.com/nasa/telemetry_app/pull/183>을 참조하세요.

## 개발 빌드: v1.3.0-rc4+dev27
- 잘못 정렬된 댓글
- 반환 값 주위의 불필요한 괄호를 제거합니다.
- '반품'을 제거하세요. void 함수의 마지막 줄에서.
- <https://github.com/nasa/telemetry_app/pull/177>, <https://github.com/nasa/telemetry_app/pull/179> 및 <https://github.com/nasa/telemetry_app/pull/181>을 참조하세요.

## 개발 빌드: v1.3.0-rc4+dev21
- 빈 이벤트 필터 등록 제거
- 재사용 가능한 업데이트를 위한 codeql 워크플로 업데이트
- <https://github.com/nasa/cFS/pull/505> 참조

## 개발 빌드: v1.3.0-rc4+dev16
- 저작권 헤더 업데이트
- 버전 정보 표준화
- <https://github.com/nasa/telemetry_app/pull/171> 및 <https://github.com/nasa/cFS/pull/445>을 참조하세요.

## 개발 빌드: v1.3.0-rc4+dev9
- 선호하는 UT 패턴 사용
- CodeQL, 정적 분석 및 형식 확인 재사용
- <https://github.com/nasa/cFS/pull/414> 참조

## 개발 빌드: v1.3.0-rc4+dev4
- CFE_MSG_PTR 변환 매크로 사용
- cFS-Caelum-rc4의 기준을 v1.3.0-rc4로 업데이트
- <https://github.com/nasa/telemetry_app/pull/163> 및 <https://github.com/nasa/cFS/pull/390>을 참조하세요.

## 개발 빌드: v1.2.0-rc1+dev73
- 필요한 경우 CFE_SB_ValueToMsgId를 적용합니다.
- <https://github.com/nasa/telemetry_app/pull/155> 및 <https://github.com/nasa/cFS/pull/359>을 참조하세요.

## 개발 빌드: v1.2.0-rc1+dev66
-  CodeQL 워크플로에서 코딩 표준 구현
- <https://github.com/nasa/telemetry_app/pull/150> 및 <https://github.com/nasa/cFS/pull/270>을 참조하세요.

## 개발 빌드: v1.2.0-rc1+dev62
- 애플리케이션이 스스로 등록할 필요가 없으므로 앱 등록 호출 `CFE_ES_RegisterApp()`을(를) 제거합니다.
- 선행 밑줄을 제거하여 모든 헤더 파일에 표준 헤더 가드를 적용합니다. 파일 범위 블록 주석을 doxygen 형식으로 변환합니다.
- <https://github.com/nasa/telemetry_app/pull/145> 참조

## 개발 빌드: v1.2.0-rc1+dev56
- 로컬 포함에서 <>를 "로 대체합니다.
- 기본 cFS 기여 가이드에 연결되는 CONTRIBUTING.md를 추가합니다.
- 명령 및 텔레메트리 메시지 ID 요구 사항에 대한 설명을 추가하여 문서 ID에 해당 요구 사항이 있는 이유를 설명합니다.
- <https://github.com/nasa/telemetry_app/pull/137> 참조

## 개발 빌드: v1.2.0-rc1+dev48
- 래퍼 및 인터페이스 라이브러리를 사용하여 빌드 단순화
- 보안 정책에 테스트 도구 추가
- <https://github.com/nasa/telemetry_app/pull/130> 참조

## 개발 빌드: v1.2.0-rc1+dev37
- 문서: 취약점 보고에 대한 지침과 함께 `Security.md`을 추가하세요.
- `CFE_TBL_GetAddress`에 대한 오류로 성공 코드가 보고되는 버그를 해결합니다.
- <https://github.com/nasa/osal/issues/724>의 변경 사항에 따라 `UT_ClearForceFail`의 이름을 `UT_ClearDefaultValue`로 바꿉니다.
- <https://github.com/nasa/telemetry_app/pull/121> 참조

## 개발 빌드: v1.2.0-rc1+dev29
- cFE <https://github.com/nasa/cFE/issues/1009>의 변경 사항에 따라 메시지를 정렬합니다. 정의에 "원시" 메시지 cmd/tlm 유형을 사용합니다.
- <https://github.com/nasa/telemetry_app/pull/114> 참조

## 개발 빌드: v1.2.0-rc1+dev25
- 2개 이상의 값을 유지하는 일부 기능이 반드시 실패하는 것은 아니므로 `UT_SetForceFail`의 이름을 `UT_SetDefaultReturnValue`로 바꿉니다.
- <https://github.com/nasa/telemetry_app/pull/113> 참조

## 개발 빌드: v1.2.0-rc1+dev22
- 더 이상 사용되지 않는 SB API를 MSG로 대체합니다.
- 영향 없음, `OS_PACK`의 바람직하지 않은 패턴 사용 제거
- <https://github.com/nasa/telemetry_app/pull/108> 참조

## 개발 빌드: v1.2.0-rc1+dev18
- 동작 변화가 없습니다. 이제 모든 식별자는 접두사 `TELEMETRY_APP_`을 사용합니다. 기본 함수의 이름을 SAMPLE_AppMain에서 CFE 시작 스크립트에 영향을 주는 TELEMETRY_APP_Main으로 변경합니다.
- 개발 버전 상태를 나타내려면 REVISION을 "99"로 설정하십시오.
- <https://github.com/nasa/telemetry_app/pull/102> 참조

## 개발 빌드: v1.2.0-rc1+dev13
- 단위 테스트 MID 문자열 형식이 이제 32비트입니다.
- 대상 디렉터리에 단위 테스트를 설치합니다.
- UT 이벤트 테스트에서는 형식 문자열만 확인합니다.
- <https://github.com/nasa/telemetry_app/pull/98> 참조

## 개발 빌드: v1.2.0-rc1+dev5
- 표준 코딩 스타일을 적용합니다.
- CFE_SB_InitMsg의 테스트 코드 호출을 제거하고 API/스텁 버퍼를 직접 설정합니다.
- <https://github.com/nasa/telemetry_app/pull/93> 참조

## 개발 빌드: 1.1.0+dev65
- 버전 보고서에 빌드 번호 및 기준 추가
- cmake 레시피의 일부로 단위 테스트를 설치합니다. 이제 샘플 앱 테스트 실행기가 예상 설치 디렉터리에 표시됩니다.
- <https://github.com/nasa/telemetry_app/pull/86> 참조

## 개발 빌드: 1.1.11
- 테이블을 fsw/tables로 이동하고 "sample_table"의 이름을 "telemetry_app_table"로 바꿉니다.
- <https://github.com/nasa/telemetry_app/pull/76> 참조

## 개발 빌드: 1.1.10
- 이제 테스트 사례는 예상되는 이벤트 문자열을 테스트 중인 장치에서 출력된 사양 문자열 및 인수에서 파생된 문자열과 비교합니다.
- `ccsds.h` 유형에 대한 참조를 `cfe_sb.h` 제공 유형으로 바꿉니다.
- <https://github.com/nasa/telemetry_app/pull/71> 참조

## 개발 빌드: 1.1.9
- 정수 MsgId와의 호환성이 필요한 CFE_SB_MsgIdToValue() 및 CFE_SB_ValueToMsgId() 루틴을 적용합니다(syslog 인쇄, 이벤트, 컴파일 타임 MID #define 값).
- RTEMS 빌드에서 더 이상 형식 변환 오류가 발생하지 않습니다.
- <https://github.com/nasa/telemetry_app/pull/63> 참조

## 개발 빌드: 1.1.8
- make lcov의 적용 범위 데이터에는 telemetry_app 코드가 포함됩니다.
- <https://github.com/nasa/telemetry_app/pull/62> 참조

## 개발 빌드: 1.1.7
- 테이블 사용 후 테이블이 해제되지 않는 버그 수정
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/52> 참조)

## 개발 빌드: 1.1.6
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/49> 참조)

## 개발 빌드: 1.1.5
- RASPBIAN OS에서 빌드하도록 수정되었습니다.
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/47> 참조)

## 개발 빌드: 1.1.4
- OMIT_DEPRECATED를 사용한 클린 빌드 수정
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/44> 참조)

## 개발 빌드: 1.1.3
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/34> 참조)

## 개발 빌드: 1.1.2
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/20> 참조)

## 개발 빌드: 1.1.1
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/15> 참조)

## _**공식 릴리스: 1.1.0 - Aquila**_
- 사소한 업데이트(<https://github.com/nasa/telemetry_app/pull/11> 참조)
- OSAL 4.2.1과 역호환되지 않음
- cFE 6.7.0, Apache 2.0의 일부로 출시됨

## _**공식 릴리스: 1.0.0a**_
- cFE 6.6.0a, Apache 2.0의 일부로 출시됨

## 알려진 문제
샘플 애플리케이션으로서 출시 전에 광범위한 테스트가 수행되지 않으며 최소한의 기능만 포함됩니다. 이 애플리케이션과 애플리케이션 개발자 가이드에 자세히 설명된 예제 간에 불일치가 있을 수 있습니다.

## 도움 받기
추가 질문이나 도움이 필요하면 <https://github.com/nasa/cFS>에 이슈를 등록하세요.

공식 cFS 페이지: <http://cfs.gsfc.nasa.gov>

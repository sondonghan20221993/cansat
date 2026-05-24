![Static Analysis](https://github.com/nasa/lora_fc_downlink_app/workflows/Static%20Analysis/badge.svg)
![Format Check](https://github.com/nasa/lora_fc_downlink_app/workflows/Format%20Check/badge.svg)

# Core Flight System : Framework : App : Sample

이 저장소는 Core Flight System 프레임워크 구성 요소인 샘플 애플리케이션 `lora_fc_downlink_app`을 포함한다.

이 샘플 애플리케이션은 cFS 번들을 위한 non-flight 예제 구현이다. 일반적으로 cFS Mission Tree의 `apps/lora_fc_downlink_app` 하위 디렉터리에 위치하도록 설계되었다. Core Flight System 번들은 [nasa/cFS](https://github.com/nasa/cFS)에 있으며, 빌드와 실행 지침을 함께 제공한다.

`lora_fc_downlink_app`은 cFS에서 애플리케이션을 빌드하고 링크하는 방법을 보여주는 예제이다. 비즈니스 로직을 추가할 최소 골격 앱이 필요하다면 [skeleton_app](https://github.com/nasa/skeleton_app)도 참고할 수 있다.

## 알려진 문제

샘플 애플리케이션 특성상 릴리스 전에 광범위한 테스트를 수행하지 않으며, 최소 기능만 포함한다. 따라서 이 애플리케이션과 애플리케이션 개발자 가이드의 예제 사이에 차이가 있을 수 있다.

## 도움 받기

가장 좋은 방법은 [nasa/cFS](https://github.com/nasa/cFS)에 `issues:question` 또는 `issues:help wanted` 유형의 요청을 등록하는 것이다.

공식 cFS 페이지: <http://cfs.gsfc.nasa.gov>

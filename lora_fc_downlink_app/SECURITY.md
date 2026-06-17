# 보안 정책

## 취약점 보고

`lora_fc_downlink_app` 하위 시스템의 취약점을 보고하려면 [이슈를 생성](https://github.com/nasa/lora_fc_downlink_app/issues/new/choose)한다.

일반적인 cFS 취약점은 [cFS framework issue](https://github.com/nasa/cfs/issues/new/choose)를 등록하고, 추가 정보는 [상위 보안 정책](https://github.com/nasa/cFS/security/policy)을 참고한다.

두 경우 모두 `Bug Report` 템플릿을 사용하고 가능한 한 많은 정보를 제공해야 한다. 각 보고서에는 적절한 라벨을 적용하며, 보안 관련 보고서에는 `security` 라벨을 붙인다.

## 테스트

**면책 고지: nasa/lora_fc_downlink_app은 [Apache License 2.0](https://github.com/nasa/lora_fc_downlink_app/blob/main/LICENSE)에 따라 발생하는 책임을 부담하지 않는다.**

테스트는 `lora_fc_downlink_app` 품질 향상을 위해 중요한 활동이다.

cFS 번들에 사용되는 도구는 [상위 보안 정책](https://github.com/nasa/cFS/security/policy)에서 확인할 수 있다.

### CodeQL

[lora_fc_downlink_app CodeQL GitHub Actions 워크플로](https://github.com/nasa/lora_fc_downlink_app/actions/workflows/codeql-build.yml)는 공개되어 있다. 결과를 검토하려면 `lora_fc_downlink_app` 저장소를 포크하고 CodeQL 워크플로를 실행한다.

CodeQL은 GitHub Actions에서 `lora_fc_downlink_app`의 모든 브랜치에 대한 push와 pull request마다 실행된다.

CodeQL GitHub Actions 구성은 <https://github.com/github/codeql-action>에서 확인할 수 있다.

### Cppcheck

[lora_fc_downlink_app Cppcheck GitHub Actions 워크플로와 결과](https://github.com/nasa/lora_fc_downlink_app/actions/workflows/static-analysis.yml)는 공개되어 있다. 결과를 확인하려면 해당 워크플로를 선택하고 artifact를 다운로드한다.

Cppcheck는 GitHub Actions에서 `lora_fc_downlink_app`의 main 브랜치 push와 모든 pull request마다 실행된다.

Cppcheck에 대한 자세한 내용은 <http://cppcheck.sourceforge.net/>에서 확인할 수 있다.

## 추가 지원

추가 지원이 필요하면 GitHub issue를 등록한다. `cfs-community@lists.nasa.gov`로 cFS 커뮤니티에 이메일을 보낼 수도 있다.

NASA core Flight Software(cFS) 제품군의 커뮤니티 구성원이 포함된 메일링 리스트는 [여기](https://lists.nasa.gov/mailman/listinfo/cfs-community)에서 구독할 수 있다. 이 메일링 리스트는 릴리스, 버그 수정, 개선 요청, 커뮤니티 미팅 공지, 회의록 배포 등 cFS 관련 정보를 공유하는 데 사용된다.

사이버보안 사고나 우려 사항을 보고하려면 NASA Security Operations Center에 전화 `1-877-627-2732` 또는 이메일 `soc@nasa.gov`로 연락한다.

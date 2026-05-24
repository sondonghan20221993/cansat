# img_app

cFS 메타데이터 전용 이미지 앱 프로토타입이다.

이 프로토타입은 raw image byte를 직접 전송하지 않는다. 대신 `IMAGE_META_MID` 레코드를 주기적으로 게시하여, 나머지 시스템이 이미지/텔레메트리 상관관계, 타임스탬프 분리, 외부 payload reference 처리를 시험할 수 있도록 한다.

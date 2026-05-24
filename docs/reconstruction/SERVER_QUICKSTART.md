# Reconstruction Server 빠른 시작 가이드

이 문서는 현재 real-image prototype pipeline을 빠르게 실행하기 위한 가이드이다.

현재 실행 가능한 backend:

- `feature_sfm`

향후 확장을 위한 backend 경계는 이미 포함되어 있다.

## 1. 가상환경 생성

```bash
cd /path/to/cansat_2/docs
python3.10 -m venv .venv-reconstruction
source .venv-reconstruction/bin/activate
python -m pip install --upgrade pip
python -m pip install -r reconstruction/requirements-prototype.txt
```

## 2. prototype pipeline 실행

```bash
python -m reconstruction.prototype_cli \
  --backend feature_sfm \
  --image-set-id demo \
  /absolute/path/to/image1.png \
  /absolute/path/to/image2.png
```

## 3. 예상 출력

명령 실행 결과로 다음 항목을 포함한 JSON 결과가 출력된다.

- `status`
- `output_ref`
- `output_format`
- `quality`

성공 시 `output_ref`는 아래 경로에 생성된 GLB 파일을 가리킨다.

```text
artifacts/reconstruction/
```

## 3A. UI에서 고정 좌표계로 시험

재구성된 점들을 브라우저 UI에서 렌더링하면서 좌표계 transform을 고정할 수도 있다.

```bash
python -m reconstruction.prototype_ui_cli \
  --backend feature_sfm \
  --image-set-id demo-ui \
  --frame enu \
  --yaw-deg 0 --pitch-deg 0 --roll-deg 0 \
  --tx 0 --ty 0 --tz 0 \
  --open \
  /absolute/path/to/image1.png \
  /absolute/path/to/image2.png
```

참고:

- `--frame enu`는 고정된 OpenCV-to-ENU 유사 축 변환을 적용한다.
- 추가 고정 transform은 `--yaw-deg`, `--pitch-deg`, `--roll-deg`, `--tx/--ty/--tz`로 적용할 수 있다.
- 생성된 HTML은 `artifacts/reconstruction/ui/` 아래에 저장된다.

## 4. 참고 사항

- 현재 `feature_sfm` backend는 real image와 OpenCV feature matching을 사용한다.
- 현재 미구현 backend 경계는 placeholder만 제공하며 `BACKEND_NOT_IMPLEMENTED`를 반환한다.
- 장면이 겹치는 이미지 두 장 이상을 사용해야 한다.
- 이 pipeline은 prototype이며, 최종 품질의 reconstruction pipeline은 아니다.

## 5. Prototype HTTP polling 흐름

현재 remote-execution prototype은 HTTP polling을 사용한다.

```text
ground client -> POST /jobs -> GET /jobs/{job_id} until complete -> GET /jobs/{job_id}/artifact
```

A6000 장비에서 서버를 실행한다.

```bash
cd /path/to/cansat_2/docs
python -m reconstruction.server.http_server \
  --host 0.0.0.0 \
  --port 8765 \
  --backend feature_sfm
```

지상국 또는 client 장비에서 작업을 제출한다.

```bash
cd /path/to/cansat_2/docs
python -m reconstruction.prototype_remote_cli \
  --endpoint http://SERVER_IP:8765 \
  --image-set-id demo-remote \
  --request-timeout-s 900 \
  --download-dir artifacts/reconstruction/downloads \
  --open-viewer \
  /absolute/path/on/server/image1.png \
  /absolute/path/on/server/image2.png
```

중요한 prototype 제한사항:

- 이미지 경로는 서버 기준으로 해석된다. 이는 현재 이미지가 이미 서버에 존재하는 A6000 workflow와 일치한다.
- 이 prototype에서 서버는 내부적으로 작업을 동기식으로 실행한다. 장시간 실행되는 reconstruction 작업에는 `--request-timeout-s`를 충분히 길게 설정하거나, 이후 비동기 background execution으로 교체해야 한다.
- 다운로드된 artifact는 아래 명령으로 바로 시각화할 수도 있다.

```bash
python -m reconstruction.prototype_ui_cli \
  --artifact artifacts/reconstruction/downloads/JOB_ID.glb \
  --frame enu \
  --open
```

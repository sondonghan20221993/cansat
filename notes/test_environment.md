# 테스트 환경

## 가상환경 경로

```
cfs-telemetry-app/.venv/
```

프로젝트 루트에 `.venv`를 고정 위치로 사용한다. 시스템 Python에는 패키지를 설치하지 않는다.

## 초기 설정 (최초 1회)

```bash
cd ~/Desktop/cfs-telemetry-app   # 또는 실제 clone 경로
python3 -m venv .venv
.venv/bin/pip install pytest pyserial
```

Windows(WSL 외부)에서:

```bat
python -m venv .venv
.venv\Scripts\pip install pytest pyserial
```

## 테스트 실행

```bash
.venv/bin/python -m pytest tests/ -v
```

Windows:

```bat
.venv\Scripts\python -m pytest tests/ -v
```

## 의존 패키지

| 패키지 | 용도 |
| --- | --- |
| `pytest` | 테스트 실행 |
| `pyserial` | `send_lora_serial` import 경로 (테스트에서 mock으로 대체되나 모듈 자체는 필요) |

## 주의

- `.venv/` 는 `.gitignore` 에 포함되어 있어야 한다.
- `__pycache__/` 도 커밋하지 않는다.
- cFS C 앱 빌드(`make native_std.*`)와 Python 테스트 환경은 무관하다. Pi 재빌드 없이 Python 테스트만 독립 실행 가능하다.

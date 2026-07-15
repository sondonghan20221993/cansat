# cFS Telemetry App UT 커버리지 현황 (2026-07-15)

## 형식 정의

각 app별 spec md에 §UT 커버리지 섹션 추가 (예):

```markdown
## §N UT 커버리지

| 기능 | UT수 | 커버리지 | 비고 |
|------|-----|---------|------|
| Proxy CRC 검증 | 2 | 100% | |
| Sequence 검증 | 3 | 100% | replay attack |
| Authorization | 5 | 85% | Level 3 token timeout 미포함 |
| **소계** | **10** | **93%** | |
```

---

## uplink_app (변경 예정)

### 현황 (변경 전)
| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| Proxy CRC 검증 | 2 | 100% |
| Sequence 검증 | 3 | 100% |
| Authorization | 5 | 85% |
| ProxyCommand 검증 | 4 | 90% |
| ROUTE_UPDATE parsing | 8 | 90% |
| ROUTE_UPDATE forward | 2 | 90% |
| VIEWPOINT parsing | 7 | 100% |
| VIEWPOINT forward | 2 | 100% |
| CONFIG forward (검증 없음) | 1 | **50%** |
| MODE forward | 1 | 100% |
| DIAGNOSTIC forward | 1 | 100% |
| **전체** | **36** | **91%** |

### 변경 후 (config checksum UT 5개 추가)
| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| CONFIG forward + checksum | **6** | **100%** |
| **전체** | **41** | **94%** |

---

## lora_tdm_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| LoRa serial parse | 6 | 90% |
| Uplink frame CRC | 3 | 100% |
| Downlink frame build | 4 | 85% |
| TDM slot handling | 5 | 80% |
| DL2 binary frame | 3 | 90% |
| CONFIG command | 4 | 100% |
| **전체** | **25** | **88%** |

---

## cfs_core_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| State validation | 6 | 100% |
| Health publish | 3 | 90% |
| Route cache | 5 | 85% |
| CONFIG command | 4 | 100% |
| Recovery/Mode handling | 4 | 80% |
| **전체** | **22** | **91%** |

---

## mavlink_bridge_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| MAVLink frame parsing | 8 | 85% |
| SysTime DL2 | 4 | 90% |
| MISSION_ACK | 3 | 100% |
| CONFIG command | 4 | 100% |
| FC state aggregation | 6 | 80% |
| **전체** | **25** | **87%** |

---

## 전체 시스템

| 계층 | TC수 | 커버리지 |
|------|-----|---------|
| 단위 테스트 (4 apps) | **109** | **90%** |
| LoRa↔SB 통합 | ~10 | ~70% |
| 시스템 E2E | ~5 | ~60% |
| **합계** | **~124** | **~85%** |

---

## 개선 기회

| 앱 | 미포함 TC | 우선순위 |
|-----|---------|---------|
| uplink_app | Authorization Level 3 timeout | 중 |
| lora_tdm_app | Serial reconnect, timeout | 중 |
| cfs_core_app | Recovery escalation, watchdog | 낮 |
| mavlink_bridge_app | FC connection loss, timeout | 중 |

---

## 다음 단계

- [ ] config checksum UT 5개 추가 → uplink_app 94% 달성
- [ ] 빌드 + 회귀 테스트 (전체 UT)
- [ ] timeout/error path UT 추가 (우선순위 중)

# 07. cFS Integration Requirements

## 1. Purpose

Define requirements for integrating modules into the cFS environment.

## 2. cFS App Structure

- **CFS-APP-01**: Each runtime module SHALL be integrated as a cFS-compatible app or app-managed component with an explicit initialization path.
- **CFS-APP-02**: The integration layer SHALL initialize configuration, Software Bus subscriptions, timers, and event services before entering the main processing loop.
- **CFS-APP-03**: The main loop SHALL process subscribed messages, timer events, and module status updates without blocking unrelated module execution.
- **CFS-APP-04**: Shutdown behavior SHALL release module resources and record final status through cFS event/log mechanisms.

## 3. Software Bus Message Handling

- **CFS-SB-01**: The integration layer SHALL route UWB Position_Result messages through cFS Software Bus for downstream alignment and logging.
- **CFS-SB-02**: The integration layer SHALL route reconstruction request/result metadata through cFS Software Bus or a documented bridge when the payload is too large for direct SB transport.
- **CFS-SB-03**: The integration layer SHALL preserve message timestamp, source module identifier, status, and payload reference fields across module boundaries.
- **CFS-SB-04**: Message IDs, source IDs, and payload schema ownership SHALL follow 03-interface-specification.md.

## 4. Timer Requirements

- **CFS-TMR-01**: The UWB Output_Cycle_Timer period SHALL be 66 ms nominal to support 15 Hz output.
- **CFS-TMR-02**: The UWB missing-distance grace timeout SHALL be 30 ms after Output_Cycle_Timer expiry.
- **CFS-TMR-03**: Timers SHALL use cFS timer services or an equivalent cFS-managed scheduling mechanism.
- **CFS-TMR-04**: Timer callbacks SHALL enqueue work or signal module state; they SHALL NOT perform long-running reconstruction or blocking I/O directly.

## 5. Configuration Management

- **CFS-CFG-01**: Startup configuration SHALL include Anchor coordinates, module enable flags, reconstruction endpoint settings, output format, and alignment transform parameters.
- **CFS-CFG-02**: Configuration validation SHALL occur during initialization before publishing module outputs.
- **CFS-CFG-03**: Invalid required configuration SHALL prevent the affected module from entering nominal operation and SHALL emit a traceable event.
- **CFS-CFG-04**: Runtime parameter updates SHALL be accepted only for fields explicitly marked runtime-changeable in the owning module specification.
- **CFS-CFG-05**: The UWB module SHALL be individually enableable/disableable through configuration. When disabled, missing UWB-specific configuration such as Anchor coordinates SHALL NOT prevent non-UWB modules from entering nominal operation.

## 6. Logging and Event Handling

- **CFS-LOG-01**: The integration layer SHALL define log/event levels: INFO, WARNING, ERROR, and DIAGNOSTIC.
- **CFS-LOG-02**: Residual threshold exceedance in the UWB module SHALL be logged as WARNING with a stable event identifier.
- **CFS-LOG-03**: Invalid_Position, remote reconstruction failure, alignment failure, and configuration validation failure SHALL be logged as ERROR unless the owning module explicitly marks the condition as degraded-but-usable.
- **CFS-LOG-04**: Diagnostic logs SHALL preserve enough context for post-run analysis, including timestamp, source module, status/error code, and relevant payload references.

## 7. Module Integration Method

| Module | Integration Pattern | Trigger | Output Destination |
| --- | --- | --- | --- |
| UWB Module | Timer-driven processing app/component | Output_Cycle_Timer and distance messages | Position_Result SB topic, logs/events |
| Reconstruction Module | Request/response bridge to remote execution path | Image-set ready event or explicit job request | Reconstruction result metadata SB topic, artifact reference |
| Pose / Alignment Module | Message-driven transform processor | New source pose/result or transform config update | Aligned pose/transform metadata SB topic |

## 8. Test Requirements

- **CFS-VER-01**: The verification plan SHALL include tests for app initialization and shutdown behavior.
- **CFS-VER-02**: The verification plan SHALL include tests for Software Bus routing and payload reference preservation.
- **CFS-VER-03**: The verification plan SHALL include tests for timer behavior and non-blocking callback policy.
- **CFS-VER-04**: The verification plan SHALL include tests for configuration validation and runtime update policy.
- **CFS-VER-05**: The verification plan SHALL include failure-injection tests for event and logging behavior.

## 9. Open Items

- OI-CFS-01: Exact cFS message IDs and source IDs need to be assigned.
- OI-CFS-02: Stable event IDs for UWB residual warning and module failure cases need to be assigned.
- OI-CFS-03: Runtime-changeable configuration fields need to be finalized per module.

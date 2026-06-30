#ifndef DEFAULT_CFS_CORE_APP_MSGDEFS_H
#define DEFAULT_CFS_CORE_APP_MSGDEFS_H

typedef enum
{
    CFS_CORE_APP_HEALTH_NOMINAL  = 0,
    CFS_CORE_APP_HEALTH_DEGRADED = 1,
    CFS_CORE_APP_HEALTH_RECOVERY = 2,
    CFS_CORE_APP_HEALTH_FAILED   = 3
} CFS_CORE_APP_HealthState_t;

typedef enum
{
    CFS_CORE_APP_FAULT_NONE             = 0,
    CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT   = 1,
    CFS_CORE_APP_FAULT_GPS_STALE        = 2,
    CFS_CORE_APP_FAULT_EKF_INVALID      = 3,
    CFS_CORE_APP_FAULT_LOCAL_TIMEOUT    = 4,
    CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT = 5,
    CFS_CORE_APP_FAULT_UPLINK_TIMEOUT   = 6,
    CFS_CORE_APP_FAULT_LORA_TIMEOUT     = 7
} CFS_CORE_APP_FaultCode_t;

typedef enum
{
    CFS_CORE_APP_ROUTE_SEGMENT_NONE              = 0,
    CFS_CORE_APP_ROUTE_SEGMENT_MISSION_EXTENSION = 1,
    CFS_CORE_APP_ROUTE_SEGMENT_LANDING           = 2
} CFS_CORE_APP_RouteSegmentType_t;

typedef struct
{
    uint32 Magic;
    uint8  LastHealthState;
    uint8  Reserved[3];
    uint32 Checksum;
} CFS_CORE_APP_PersistentState_t;

/* 런타임 구성 파라미터 ID */
typedef enum
{
    CFS_CORE_APP_PARAM_ATTITUDE_TIMEOUT_MS = 0,
    CFS_CORE_APP_PARAM_LOCAL_TIMEOUT_MS    = 1,
    CFS_CORE_APP_PARAM_GPS_TIMEOUT_MS      = 2,
    CFS_CORE_APP_PARAM_EKF_TIMEOUT_MS      = 3,
    CFS_CORE_APP_PARAM_BRIDGE_TIMEOUT_MS   = 4,
    CFS_CORE_APP_PARAM_PUBLISH_PERIOD_MS   = 5,
    CFS_CORE_APP_PARAM_COUNT               = 6
} CFS_CORE_APP_ParamId_t;

/* 구성 결과 */
typedef enum
{
    CFS_CORE_APP_CONFIG_RESULT_OK            = 0,
    CFS_CORE_APP_CONFIG_RESULT_BAD_SCOPE     = 1,
    CFS_CORE_APP_CONFIG_RESULT_BAD_VERSION   = 2,
    CFS_CORE_APP_CONFIG_RESULT_BAD_PARAM     = 3,
    CFS_CORE_APP_CONFIG_RESULT_BAD_VALUE     = 4,
    CFS_CORE_APP_CONFIG_RESULT_BAD_LENGTH    = 5,
    CFS_CORE_APP_CONFIG_RESULT_BAD_CHECKSUM  = 6
} CFS_CORE_APP_ConfigResult_t;

/* pending/active 버퍼 상태 (spec §13.2) */
typedef enum
{
    CFS_CORE_APP_CONFIG_PENDING_IDLE     = 0,
    CFS_CORE_APP_CONFIG_PENDING_PENDING  = 1,
    CFS_CORE_APP_CONFIG_PENDING_REJECTED = 2
} CFS_CORE_APP_ConfigPendingState_t;

/* 런타임 구성 파라미터 집합 */
typedef struct
{
    uint32 AttitudeTimeoutMs;
    uint32 LocalTimeoutMs;
    uint32 GpsTimeoutMs;
    uint32 EkfTimeoutMs;
    uint32 BridgeTimeoutMs;
    uint32 PublishPeriodMs;
} CFS_CORE_APP_ConfigParams_t;

#endif


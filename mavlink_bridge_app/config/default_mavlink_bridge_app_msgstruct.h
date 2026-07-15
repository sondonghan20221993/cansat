#ifndef DEFAULT_MAVLINK_BRIDGE_APP_MSGSTRUCT_H
#define DEFAULT_MAVLINK_BRIDGE_APP_MSGSTRUCT_H

#include "cfe_msg_hdr.h"
#include "common_types.h"
#include "mavlink_bridge_app_msgdefs.h"
#include "bridge_hk_msg.h"
#include "fc_state_msg.h"
#include "route_msg.h"
#include "config_msg.h"

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_NoopCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_ResetCountersCmd_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} MAVLINK_BRIDGE_APP_MissionQueryCmd_t;

typedef BRIDGE_HK_TLM_t MAVLINK_BRIDGE_APP_HkTlm_t;

typedef FC_EKF_LOCAL_TLM_t  MAVLINK_BRIDGE_APP_EkfLocalTlm_t;
typedef FC_ATTITUDE_TLM_t   MAVLINK_BRIDGE_APP_AttitudeTlm_t;
typedef FC_GPS_RAW_TLM_t    MAVLINK_BRIDGE_APP_GpsRawTlm_t;
typedef FC_EKF_STATUS_TLM_t MAVLINK_BRIDGE_APP_EkfStatusTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    uint64                    TimeUnixUsec;
} MAVLINK_BRIDGE_APP_SysTimeTlm_t;

#define MAVLINK_BRIDGE_APP_ROUTE_MAX_WAYPOINTS 16U

typedef ROUTE_WAYPOINT_t MAVLINK_BRIDGE_APP_WaypointMirror_t;
typedef ROUTE_UPDATE_TLM_t MAVLINK_BRIDGE_APP_RouteUpdateMirror_t;

/* CONFIG_CMD_MID 수신용 (uplink_app의 UPLINK_APP_ConfigCmdTlm_t와 동일 레이아웃) */
typedef CONFIG_CMD_TLM_t MAVLINK_BRIDGE_APP_ConfigCmdTlm_t;

typedef struct
{
    uint8  ConfigScope;
    uint8  ConfigVersion;
    uint16 ParameterId;
    uint8  ValueType;
    uint8  ValueLength;
    uint16 Checksum;   /* uint16 additive sum: scope+version+param_id(2B)+value_type+value_length+value_bytes */
} MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t;

#endif

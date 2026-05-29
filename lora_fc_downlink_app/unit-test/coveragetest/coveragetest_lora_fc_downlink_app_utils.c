#include "lora_fc_downlink_app_coveragetest_common.h"

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
} TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     FixType;
    uint8                     SatellitesVisible;
    uint8                     Reserved;
    int32                     LatE7;
    int32                     LonE7;
    int32                     AltMm;
} TEST_LORA_FC_DOWNLINK_APP_GpsRawTlm_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    Seq;
    uint32                    TimestampMs;
    uint32                    LastValidInputTimestampMs;
    uint8                     HealthState;
    uint8                     FaultCode;
    uint8                     RecoveryRequested;
    uint8                     Reserved;
} TEST_LORA_FC_DOWNLINK_APP_SystemHealthTlm_t;

void Test_LORA_FC_DOWNLINK_APP_ReportHousekeeping(void)
{
    LORA_FC_DOWNLINK_APP_Data.CmdCounter                = 2;
    LORA_FC_DOWNLINK_APP_Data.ErrCounter                = 1;
    LORA_FC_DOWNLINK_APP_Data.DownlinkCount             = 5;
    LORA_FC_DOWNLINK_APP_Data.AttitudeValid             = 1;
    LORA_FC_DOWNLINK_APP_Data.LocalValid                = 1;
    LORA_FC_DOWNLINK_APP_Data.GpsValid                  = 0;
    LORA_FC_DOWNLINK_APP_Data.EkfValid                  = 1;
    LORA_FC_DOWNLINK_APP_Data.SystemHealthState         = 2;
    LORA_FC_DOWNLINK_APP_Data.PacketType                = 1;
    LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs   = 100;
    LORA_FC_DOWNLINK_APP_Data.LastLocalTimestampMs      = 200;
    LORA_FC_DOWNLINK_APP_Data.LastGpsTimestampMs        = 300;
    LORA_FC_DOWNLINK_APP_Data.LastEkfTimestampMs        = 400;
    LORA_FC_DOWNLINK_APP_Data.LastSystemHealthTimestampMs = 500;

    LORA_FC_DOWNLINK_APP_ReportHousekeeping();

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.DownlinkCount, 5);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.HkTlm.Payload.SystemHealthState, 2);
}

void Test_LORA_FC_DOWNLINK_APP_ProcessInputMessage(void)
{
    uint8                                 Storage[sizeof(TEST_LORA_FC_DOWNLINK_APP_SystemHealthTlm_t)];
    CFE_SB_Buffer_t                      *Buffer;
    CFE_SB_MsgId_t                        MsgId;
    TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t *Attitude;
    TEST_LORA_FC_DOWNLINK_APP_SystemHealthTlm_t *Health;

    memset(Storage, 0, sizeof(Storage));
    Buffer   = (CFE_SB_Buffer_t *)Storage;
    Attitude = (TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Attitude->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE), sizeof(*Attitude));
    Attitude->TimestampMs = 1111;
    Attitude->Valid       = 1;
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_ATTITUDE_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_ProcessInputMessage(Buffer);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LastAttitudeTimestampMs, 1111);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.AttitudeValid, 1);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.DownlinkCount, 1);

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Health = (TEST_LORA_FC_DOWNLINK_APP_SystemHealthTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Health->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_MID_VALUE), sizeof(*Health));
    Health->TimestampMs = 2222;
    Health->HealthState = 3;
    Health->FaultCode   = 2;
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_SYSTEM_HEALTH_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_ProcessInputMessage(Buffer);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LastSystemHealthTimestampMs, 2222);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.SystemHealthState, 3);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.SystemHealthFaultCode, 2);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.DownlinkCount, 2);

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Attitude = (TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Attitude->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE), sizeof(*Attitude));
    Attitude->TimestampMs = 3333;
    Attitude->Valid       = 1;
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_LOCAL_STATE_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_ProcessInputMessage(Buffer);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LastLocalTimestampMs, 3333);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LocalValid, 1);

    memset(Storage, 0, sizeof(Storage));
    {
        uint8                                      GpsStorage[sizeof(TEST_LORA_FC_DOWNLINK_APP_GpsRawTlm_t)];
        CFE_SB_Buffer_t                           *GpsBuffer;
        TEST_LORA_FC_DOWNLINK_APP_GpsRawTlm_t    *Gps;

        memset(GpsStorage, 0, sizeof(GpsStorage));
        GpsBuffer = (CFE_SB_Buffer_t *)GpsStorage;
        Gps       = (TEST_LORA_FC_DOWNLINK_APP_GpsRawTlm_t *)GpsStorage;
        CFE_MSG_Init(CFE_MSG_PTR(Gps->TelemetryHeader),
                     CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE), sizeof(*Gps));
        Gps->TimestampMs = 4444;
        Gps->Valid       = 1;
        Gps->FixType     = 3;
        Gps->LatE7       = 374530000;
        Gps->LonE7       = 1269850000;
        Gps->AltMm       = 50000;
        MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_GPS_RAW_STATE_MID_VALUE);
        UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
        LORA_FC_DOWNLINK_APP_ProcessInputMessage(GpsBuffer);

        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LastGpsTimestampMs, 4444);
        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.GpsValid, 1);
        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.GpsLatE7, 374530000);
        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.GpsLonE7, 1269850000);
        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.GpsAltMm, 50000);
        UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.GpsFixType, 3);
    }

    memset(Storage, 0, sizeof(Storage));
    Buffer = (CFE_SB_Buffer_t *)Storage;
    Attitude = (TEST_LORA_FC_DOWNLINK_APP_GenericStateTlm_t *)Storage;
    CFE_MSG_Init(CFE_MSG_PTR(Attitude->TelemetryHeader),
                 CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE), sizeof(*Attitude));
    Attitude->TimestampMs = 5555;
    Attitude->Valid       = 1;
    MsgId = CFE_SB_ValueToMsgId(LORA_FC_DOWNLINK_APP_FC_EKF_STATUS_MID_VALUE);
    UT_SetDataBuffer(UT_KEY(CFE_MSG_GetMsgId), &MsgId, sizeof(MsgId), false);
    LORA_FC_DOWNLINK_APP_ProcessInputMessage(Buffer);

    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.LastEkfTimestampMs, 5555);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.EkfValid, 1);
    UtAssert_INT32_EQ(LORA_FC_DOWNLINK_APP_Data.PacketType, LORA_FC_DOWNLINK_APP_FC_STATE_PACKET_TYPE);
}

void UtTest_Setup(void)
{
    ADD_TEST(LORA_FC_DOWNLINK_APP_ReportHousekeeping);
    ADD_TEST(LORA_FC_DOWNLINK_APP_ProcessInputMessage);
}

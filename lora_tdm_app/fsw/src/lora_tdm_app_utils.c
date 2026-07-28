#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_eventids.h"
#include "system_health_msg.h"
#include "fc_state_msg.h"

#include "cfe_time.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static uint32 UtilsGetTimeMs(void)
{
    CFE_TIME_SysTime_t T = CFE_TIME_GetTime();
    uint64             Ms;

    Ms = ((uint64)T.Seconds * 1000ULL) + ((uint64)T.Subseconds * 1000ULL / 0x100000000ULL);
    return (uint32)Ms;
}

/* ---- CRC-16/CCITT-FALSE ---- */

uint16 LORA_TDM_APP_Crc16(const uint8 *Data, size_t Len)
{
    uint16 Crc = 0xFFFF;
    size_t i;
    uint8  j;

    for (i = 0; i < Len; i++)
    {
        Crc ^= (uint16)Data[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (Crc & 0x8000)
                Crc = (Crc << 1) ^ 0x1021;
            else
                Crc <<= 1;
        }
    }
    return Crc;
}

/* ---- Parse ACK frame: "ACK,<seq>\n" ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseAckFrame(const char *Line, uint32 *SeqEcho)
{
    unsigned long Val;
    char         *End;

    if (Line == NULL || SeqEcho == NULL)
    {
        return LORA_TDM_ACK_INVALID;
    }

    if (strncmp(Line, "ACK,", 4) != 0)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Val = strtoul(Line + 4, &End, 10);
    if (End == Line + 4 || (*End != '\n' && *End != '\r' && *End != '\0'))
    {
        return LORA_TDM_ACK_INVALID;
    }

    *SeqEcho = (uint32)Val;
    return LORA_TDM_ACK_OK;
}

/* ---- Build FC downlink line ---- */
/*
 * Format: FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat>,<lon>,<alt>,<fix>,<ufb>,<sats>\n
 *
 * <sats> appended as field 18 (2026-07-13) — 기존 필드 순서/개수를 바꾸지 않고 끝에 추가해
 * 구 파서(17필드 고정 파싱)와의 호환을 유지한다. 지상 파서는 len(parts)>=18일 때만 읽는다.
 */
int LORA_TDM_APP_BuildFcDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    int Len;

    Len = snprintf(Buf, BufLen,
                   "FC,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%ld,%ld,%ld,%u,%u,%u\n",
                   (unsigned long)AppData->DownlinkSeq,
                   (unsigned long)AppData->FcState.TimestampMs,
                   (double)AppData->FcState.RollRad,
                   (double)AppData->FcState.PitchRad,
                   (double)AppData->FcState.YawRad,
                   (double)AppData->FcState.PosX,
                   (double)AppData->FcState.PosY,
                   (double)AppData->FcState.PosZ,
                   (double)AppData->FcState.VelX,
                   (double)AppData->FcState.VelY,
                   (double)AppData->FcState.VelZ,
                   (long)AppData->FcState.LatE7,
                   (long)AppData->FcState.LonE7,
                   (long)AppData->FcState.AltMm,
                   (unsigned)AppData->FcState.GpsFix,
                   (unsigned)AppData->PendingUplinkFeedback,
                   (unsigned)AppData->FcState.SatellitesVisible);

    if (Len <= 0 || (size_t)Len >= BufLen)
    {
        return -1;
    }
    return Len;
}

/* ---- DL2 binary frame helpers (little-endian packing, portable) ---- */

static uint16 GetU16LE(const uint8 *P)
{
    return (uint16)(P[0] | ((uint16)P[1] << 8));
}

static void PutU16LE(uint8 *P, uint16 V)
{
    P[0] = (uint8)(V & 0xFFu);
    P[1] = (uint8)((V >> 8) & 0xFFu);
}

static void PutI16LE(uint8 *P, int16 V)
{
    PutU16LE(P, (uint16)V);
}

static void PutU32LE(uint8 *P, uint32 V)
{
    P[0] = (uint8)(V & 0xFFu);
    P[1] = (uint8)((V >> 8) & 0xFFu);
    P[2] = (uint8)((V >> 16) & 0xFFu);
    P[3] = (uint8)((V >> 24) & 0xFFu);
}

static void PutI32LE(uint8 *P, int32 V)
{
    PutU32LE(P, (uint32)V);
}

static void PutU64LE(uint8 *P, uint64 V)
{
    PutU32LE(&P[0], (uint32)(V & 0xFFFFFFFFu));
    PutU32LE(&P[4], (uint32)((V >> 32) & 0xFFFFFFFFu));
}

/* waypoint readback(2026-07-23): IEEE754 float를 비트 패턴 그대로 LE 4바이트로 —
 * memcpy로 strict-aliasing 위반 없이 비트 추출 후 기존 PutU32LE로 바이트 순서 고정 */
static void PutFloatLE(uint8 *P, float V)
{
    uint32 Bits;
    memcpy(&Bits, &V, sizeof(Bits));
    PutU32LE(P, Bits);
}

/* rad -> i16(rad*1e4), 범위 방어적 clamp (스펙상 각도 saturation 플래그는 정의 안 함) */
static int16 ScaleRadToI16(float Rad)
{
    double V = (double)Rad * 10000.0;
    if (V > 32767.0)
    {
        V = 32767.0;
    }
    else if (V < -32768.0)
    {
        V = -32768.0;
    }
    return (int16)V;
}

/* m 또는 m/s -> i16(값*100 = cm 또는 cm/s), §4.1 saturation: 초과 시 clamp+플래그 요청 */
static int16 ScaleMeterToI16Cm(float MetersVal, bool *Saturated)
{
    double Cm = (double)MetersVal * 100.0;

    if (Cm > 32767.0)
    {
        Cm = 32767.0;
        *Saturated = true;
    }
    else if (Cm < -32768.0)
    {
        Cm = -32768.0;
        *Saturated = true;
    }
    return (int16)Cm;
}

/* ---- Build DL2 frame (v2 바이너리 다운링크) — lora_protocol_v2_spec.md §4 ----
 * SysTime 확장 블록(§4.2): FcState.TimeValid면 flags bit0 세우고 TimeUnixUsec
 * 8바이트를 삽입. BL-03(2026-07-22)로 uplink_last_seq(2B)+boot_count(1B)가
 * 항상 맨 끝(SysTime 블록 뒤, CRC 앞)에 추가돼 총 길이는 50B(기본) 또는
 * 58B(SysTime 포함) — 기존 47B/55B에서 +3B. */
int LORA_TDM_APP_BuildDl2Frame(uint8 *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    uint8  Flags = 0;
    bool   Saturated = false;
    bool   IncludeSysTime;
    bool   IncludeWaypoints;
    uint8  TailOffset;
    uint8  WaypointOffset;
    uint8  BodyLen;
    uint16 Crc;

    if (Buf == NULL || AppData == NULL || BufLen < LORA_TDM_APP_DL2_FRAME_LEN)
    {
        return -1;
    }

    IncludeSysTime = (AppData->FcState.TimeValid != 0U);
    if (IncludeSysTime && BufLen < LORA_TDM_APP_DL2_MAX_FRAME_LEN)
    {
        IncludeSysTime = false; /* 버퍼 부족 시 SysTime 없이 기본 프레임으로 폴백 */
    }
    /* BL-03: 꼬리 필드(uplink_last_seq/boot_count)는 SysTime 블록(있으면) 뒤,
     * CRC 앞에 항상 붙는다 — "기존 끝에 추가" 결정. */
    TailOffset = (uint8)(LORA_TDM_APP_DL2_BASE_LEN + (IncludeSysTime ? LORA_TDM_APP_DL2_SYSTIME_BLOCK_LEN : 0U));

    /* waypoint readback(2026-07-23, spec §4.3): 꼬리 필드 뒤·CRC 앞에 페이지
     * 블록 첨부 — SysTime과 독립적으로 동시 첨부 가능 (버퍼 부족 시 생략) */
    IncludeWaypoints = (AppData->RouteReadbackPending != 0U);
    WaypointOffset   = (uint8)(TailOffset + LORA_TDM_APP_DL2_TAIL_LEN);
    if (IncludeWaypoints && BufLen < (size_t)(WaypointOffset + LORA_TDM_APP_DL2_WAYPOINT_BLOCK_LEN + 2U))
    {
        IncludeWaypoints = false;
    }
    BodyLen = (uint8)(WaypointOffset + (IncludeWaypoints ? LORA_TDM_APP_DL2_WAYPOINT_BLOCK_LEN : 0U));

    Buf[0] = (uint8)LORA_TDM_APP_DL2_MAGIC;
    Buf[1] = BodyLen;
    PutU16LE(&Buf[2], (uint16)AppData->DownlinkSeq);
    /* Buf[4] flags — 아래서 saturation/systime 확인 후 채움 */
    Buf[5] = (uint8)AppData->PendingUplinkFeedback;
    PutU32LE(&Buf[6], AppData->FcState.TimestampMs);

    PutI16LE(&Buf[10], ScaleRadToI16(AppData->FcState.RollRad));
    PutI16LE(&Buf[12], ScaleRadToI16(AppData->FcState.PitchRad));
    PutI16LE(&Buf[14], ScaleRadToI16(AppData->FcState.YawRad));

    PutI16LE(&Buf[16], ScaleMeterToI16Cm(AppData->FcState.PosX, &Saturated));
    PutI16LE(&Buf[18], ScaleMeterToI16Cm(AppData->FcState.PosY, &Saturated));
    PutI16LE(&Buf[20], ScaleMeterToI16Cm(AppData->FcState.PosZ, &Saturated));

    PutI16LE(&Buf[22], ScaleMeterToI16Cm(AppData->FcState.VelX, &Saturated));
    PutI16LE(&Buf[24], ScaleMeterToI16Cm(AppData->FcState.VelY, &Saturated));
    PutI16LE(&Buf[26], ScaleMeterToI16Cm(AppData->FcState.VelZ, &Saturated));

    PutI32LE(&Buf[28], AppData->FcState.LatE7);
    PutI32LE(&Buf[32], AppData->FcState.LonE7);
    PutI32LE(&Buf[36], AppData->FcState.AltMm);

    Buf[40] = AppData->FcState.GpsFix;
    Buf[41] = AppData->FcState.SatellitesVisible;
    Buf[42] = AppData->SystemHealth.SystemHealthState;
    Buf[43] = AppData->SystemHealth.FaultCode;
    Buf[44] = AppData->LinkState;

    if (IncludeSysTime)
    {
        PutU64LE(&Buf[LORA_TDM_APP_DL2_BASE_LEN], AppData->FcState.TimeUnixUsec);
    }

    PutU16LE(&Buf[TailOffset], AppData->UplinkLastAcceptedSequence);
    Buf[TailOffset + 2] = AppData->UplinkBootCount;

    if (IncludeWaypoints)
    {
        uint8 PageIndex  = AppData->RoutePageIndex;
        uint8 StartIdx   = (uint8)(PageIndex * LORA_TDM_APP_DL2_WAYPOINTS_PER_PAGE);
        uint8 Remaining  = (uint8)(AppData->RouteWaypointCount - StartIdx);
        uint8 InPage     = (Remaining < LORA_TDM_APP_DL2_WAYPOINTS_PER_PAGE) ?
                            Remaining : (uint8)LORA_TDM_APP_DL2_WAYPOINTS_PER_PAGE;

        Buf[WaypointOffset]     = AppData->RouteType;
        Buf[WaypointOffset + 1] = PageIndex;
        Buf[WaypointOffset + 2] = AppData->RouteTotalPages;
        Buf[WaypointOffset + 3] = InPage;

        /* BL-71(2026-07-28): DL2 waypoint 페이지 포맷 — CmdType(u8)+LatE7(int32)+LonE7(int32)+
         * Z(float) 13바이트/waypoint(BL-61의 12바이트 LatE7+LonE7+Z에 CmdType 선두 추가 —
         * 지상이 위치 기반으로 시작/호버/랜딩을 임의 추측하던 문제 해소). Param1~4는
         * 여전히 생략(지상/openMCT가 기본값 0.0으로 복원). */
        Buf[WaypointOffset + 4] = (InPage >= 1U) ? AppData->RouteWaypoints[StartIdx].CmdType : 0U;
        PutI32LE(&Buf[WaypointOffset + 5],  (InPage >= 1U) ? AppData->RouteWaypoints[StartIdx].LatE7 : 0);
        PutI32LE(&Buf[WaypointOffset + 9],  (InPage >= 1U) ? AppData->RouteWaypoints[StartIdx].LonE7 : 0);
        PutFloatLE(&Buf[WaypointOffset + 13], (InPage >= 1U) ? AppData->RouteWaypoints[StartIdx].Z : 0.0f);
        Buf[WaypointOffset + 17] = (InPage >= 2U) ? AppData->RouteWaypoints[StartIdx + 1U].CmdType : 0U;
        PutI32LE(&Buf[WaypointOffset + 18], (InPage >= 2U) ? AppData->RouteWaypoints[StartIdx + 1U].LatE7 : 0);
        PutI32LE(&Buf[WaypointOffset + 22], (InPage >= 2U) ? AppData->RouteWaypoints[StartIdx + 1U].LonE7 : 0);
        PutFloatLE(&Buf[WaypointOffset + 26], (InPage >= 2U) ? AppData->RouteWaypoints[StartIdx + 1U].Z : 0.0f);
    }

    Flags |= Saturated ? 0x02u : 0x00u;
    Flags |= IncludeSysTime  ? (uint8)LORA_TDM_APP_DL2_FLAG_SYSTIME  : 0x00u;
    Flags |= IncludeWaypoints ? (uint8)LORA_TDM_APP_DL2_FLAG_WAYPOINT : 0x00u;
    Buf[4] = Flags;

    Crc = LORA_TDM_APP_Crc16(Buf, BodyLen);
    PutU16LE(&Buf[BodyLen], Crc);

    return (int)BodyLen + 2;
}

/* ---- Parse ACK2 frame (v2 바이너리, 5B) — lora_protocol_v2_spec.md §6 ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseAck2Frame(const uint8 *Buf, size_t Len, uint32 *SeqEcho)
{
    uint16 ExpectedCrc;
    uint16 ActualCrc;

    if (Buf == NULL || SeqEcho == NULL || Len < 5)
    {
        return LORA_TDM_ACK_INVALID;
    }
    if (Buf[0] != (uint8)LORA_TDM_APP_ACK2_MAGIC)
    {
        return LORA_TDM_ACK_INVALID;
    }

    ActualCrc   = LORA_TDM_APP_Crc16(Buf, 3);
    ExpectedCrc = GetU16LE(&Buf[3]);
    if (ActualCrc != ExpectedCrc)
    {
        return LORA_TDM_ACK_INVALID;
    }

    *SeqEcho = GetU16LE(&Buf[1]);
    return LORA_TDM_ACK_OK;
}

/* ---- Parse UP2 frame (v2 바이너리 업링크) — lora_protocol_v2_spec.md §5 ---- */

LORA_TDM_AckResult_t LORA_TDM_APP_ParseUp2Frame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Up2Decoded_t *Out)
{
    uint8  Plen;
    size_t FrameLen;
    uint16 ExpectedCrc;
    uint16 ActualCrc;

    /* 최소 프레임(plen=0): magic+plen+version+class+seq(2)+flags+crc(2) = 8B */
    if (Buf == NULL || Out == NULL || Len < 8)
    {
        return LORA_TDM_ACK_INVALID;
    }
    if (Buf[0] != (uint8)LORA_TDM_APP_UP2_MAGIC)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Plen     = Buf[1];
    FrameLen = 7u + (size_t)Plen + 2u;
    if (Len < FrameLen || Plen > sizeof(Out->Payload))
    {
        return LORA_TDM_ACK_INVALID;
    }

    ActualCrc   = LORA_TDM_APP_Crc16(Buf, 7u + Plen);
    ExpectedCrc = GetU16LE(&Buf[7u + Plen]);
    if (ActualCrc != ExpectedCrc)
    {
        return LORA_TDM_ACK_INVALID;
    }

    Out->Version      = Buf[2];
    Out->CommandClass = Buf[3];
    Out->Seq          = GetU16LE(&Buf[4]);
    Out->Flags        = Buf[6];
    Out->PayloadLen   = Plen;
    if (Plen > 0)
    {
        memcpy(Out->Payload, &Buf[7], Plen);
    }

    return LORA_TDM_ACK_OK;
}

/* ---- Build SH downlink line ---- */
/*
 * Format: SH,<seq>,<ts>,<state>,<fault>,<linkstate>,<ufb>\n
 */
int LORA_TDM_APP_BuildShDownlinkLine(char *Buf, size_t BufLen, const LORA_TDM_APP_Data_t *AppData)
{
    int Len;

    Len = snprintf(Buf, BufLen,
                   "SH,%lu,%lu,%u,%u,%u,%u\n",
                   (unsigned long)AppData->DownlinkSeq,
                   (unsigned long)AppData->SystemHealth.TimestampMs,
                   (unsigned)AppData->SystemHealth.SystemHealthState,
                   (unsigned)AppData->SystemHealth.FaultCode,
                   (unsigned)AppData->LinkState,
                   (unsigned)AppData->PendingUplinkFeedback);

    if (Len <= 0 || (size_t)Len >= BufLen)
    {
        return -1;
    }
    return Len;
}

/* ---- Update link state ---- */

void LORA_TDM_APP_UpdateLinkState(LORA_TDM_APP_Data_t *AppData, uint32 NowMs)
{
    uint32 Elapsed;
    uint8  NewState;
    uint8  OldState = AppData->LinkState;

    if (AppData->LastAckTimestampMs == 0)
    {
        Elapsed = 0;
    }
    else if (NowMs >= AppData->LastAckTimestampMs)
    {
        Elapsed = NowMs - AppData->LastAckTimestampMs;
    }
    else
    {
        Elapsed = 0;
    }

    if (Elapsed > LORA_TDM_APP_LINK_TIMEOUT_MS)
    {
        NewState = LORA_TDM_APP_LINK_DISCONNECTED;
    }
    else if (AppData->NoAckCount >= LORA_TDM_APP_LINK_LOSS_THRESHOLD)
    {
        NewState = LORA_TDM_APP_LINK_DEGRADED;
    }
    else
    {
        NewState = LORA_TDM_APP_LINK_CONNECTED;
    }

    AppData->LinkState = NewState;

    /* BL-04(2026-07-21): 상태가 바뀔 때만 이벤트 발생(엣지 트리거) —
     * 부팅 직후 첫 관측은 "전이"가 아니므로 제외. 플래핑 폭주 방지는
     * 이미 위 임계값 자체가 다중 사이클 디바운스(NoAckCount>=15회 ≈3s,
     * LINK_TIMEOUT=5s)를 제공하므로 이벤트단 추가 히스테리시스는 두지
     * 않음 — 임계값 경계에서만 드물게 발생 가능. RESTORED는 CONNECTED로
     * 들어오는 모든 전이(DEGRADED/DISCONNECTED 어느 쪽에서든)를 포함. */
    if (AppData->LinkStateInitialized == 0U)
    {
        AppData->LinkStateInitialized = 1U;
    }
    else if (NewState != OldState)
    {
        if (NewState == LORA_TDM_APP_LINK_DISCONNECTED)
        {
            CFE_EVS_SendEvent(LORA_TDM_APP_LINK_LOST_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: link lost (state %u -> %u)",
                              (unsigned int)OldState, (unsigned int)NewState);
        }
        else if (NewState == LORA_TDM_APP_LINK_DEGRADED)
        {
            CFE_EVS_SendEvent(LORA_TDM_APP_LINK_DEGRADED_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: link degraded (state %u -> %u)",
                              (unsigned int)OldState, (unsigned int)NewState);
        }
        else /* CONNECTED */
        {
            CFE_EVS_SendEvent(LORA_TDM_APP_LINK_RESTORED_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: link restored (state %u -> %u)",
                              (unsigned int)OldState, (unsigned int)NewState);
        }
    }
}

/* ---- Hex nibble helper ---- */

static int HexNibble(char C)
{
    if (C >= '0' && C <= '9') return C - '0';
    if (C >= 'a' && C <= 'f') return C - 'a' + 10;
    if (C >= 'A' && C <= 'F') return C - 'A' + 10;
    return -1;
}

/* ---- Parse UP frame and forward to uplink_app via SB ---- */

static void ProcessUpFrame(const char *Line, LORA_TDM_APP_Data_t *AppData)
{
    char     VersionStr[8], ClassStr[8], SeqStr[8], FlagsStr[8];
    char     PayloadHex[196 * 2 + 2];  /* max payload hex */
    char     CrcStr[8];
    char     Canonical[640];
    int      CanonLen;
    uint16   ExpectedCrc;
    uint16   ActualCrc;
    uint8    Version;
    uint8    CommandClass;
    uint16   Seq;
    uint8    Flags;
    uint8    Payload[196];
    uint16   PayloadLen;
    uint16   HexLen;
    uint16   i;
    LORA_TDM_APP_UplinkFwdCmd_t FwdCmd;

    if (sscanf(Line + 3, "%7[^,],%7[^,],%7[^,],%7[^,],%[^,],%7s",
               VersionStr, ClassStr, SeqStr, FlagsStr, PayloadHex, CrcStr) != 6)
    {
        /* "%[^,]" requires >=1 char, so a zero-length payload_hex field (",,")
         * falls through here. Retry assuming an empty payload before failing. */
        PayloadHex[0] = '\0';
        if (sscanf(Line + 3, "%7[^,],%7[^,],%7[^,],%7[^,],,%7s",
                   VersionStr, ClassStr, SeqStr, FlagsStr, CrcStr) != 5)
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: UP frame parse error");
            return;
        }
    }

    Version      = (uint8)strtoul(VersionStr, NULL, 0);
    CommandClass = (uint8)strtoul(ClassStr, NULL, 0);
    Seq          = (uint16)strtoul(SeqStr, NULL, 0);
    Flags        = (uint8)strtoul(FlagsStr, NULL, 0);
    ExpectedCrc  = (uint16)strtoul(CrcStr, NULL, 16);

    CanonLen = snprintf(Canonical, sizeof(Canonical), "UP,%s,%s,%s,%s,%s",
                        VersionStr, ClassStr, SeqStr, FlagsStr, PayloadHex);
    if (CanonLen <= 0 || (size_t)CanonLen >= sizeof(Canonical))
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        return;
    }

    ActualCrc = LORA_TDM_APP_Crc16((const uint8 *)Canonical, (size_t)CanonLen);
    if (ActualCrc != ExpectedCrc)
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: UP frame CRC fail expected=%04X actual=%04X",
                          (unsigned)ExpectedCrc, (unsigned)ActualCrc);
        return;
    }

    HexLen = (uint16)strlen(PayloadHex);
    if (HexLen % 2 != 0 || HexLen / 2 > sizeof(Payload))
    {
        AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
        AppData->RxErrorCount++;
        return;
    }

    PayloadLen = HexLen / 2;
    for (i = 0; i < PayloadLen; i++)
    {
        int Hi = HexNibble(PayloadHex[i * 2]);
        int Lo = HexNibble(PayloadHex[i * 2 + 1]);
        if (Hi < 0 || Lo < 0)
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            return;
        }
        Payload[i] = (uint8)((Hi << 4) | Lo);
    }

    memset(&FwdCmd, 0, sizeof(FwdCmd));
    CFE_MSG_Init(CFE_MSG_PTR(FwdCmd.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_APP_CMD_MID_VALUE),
                 sizeof(FwdCmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(FwdCmd.CommandHeader), LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC);
    FwdCmd.Version       = Version;
    FwdCmd.CommandClass  = CommandClass;
    FwdCmd.PayloadLength = (uint8)PayloadLen;
    FwdCmd.Flags         = Flags;
    FwdCmd.Sequence      = Seq;
    if (PayloadLen > 0)
    {
        memcpy(FwdCmd.Payload, Payload, PayloadLen);
    }

    /* CRC-16/CCITT-FALSE over Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload
     * mirrors build_process_uplink_payload() in Python tools */
    {
        uint8 CrcBuf[6 + 196];
        CrcBuf[0] = Version;
        CrcBuf[1] = CommandClass;
        CrcBuf[2] = (uint8)PayloadLen;
        CrcBuf[3] = Flags;
        CrcBuf[4] = (uint8)(Seq & 0xFF);
        CrcBuf[5] = (uint8)(Seq >> 8);
        if (PayloadLen > 0) { memcpy(&CrcBuf[6], Payload, PayloadLen); }
        FwdCmd.Checksum = LORA_TDM_APP_Crc16(CrcBuf, 6 + PayloadLen);
    }

    CFE_SB_TransmitMsg(CFE_MSG_PTR(FwdCmd.CommandHeader), true);

    AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    AppData->RxCmdCount++;
}

/* ---- Process a received line ---- */

void LORA_TDM_APP_ProcessRxLine(const char *Line, LORA_TDM_APP_Data_t *AppData)
{
    uint32               SeqEcho;
    LORA_TDM_AckResult_t AckResult;

    if (Line == NULL || AppData == NULL)
    {
        return;
    }

    if (strncmp(Line, "ACK,", 4) == 0)
    {
        AckResult = LORA_TDM_APP_ParseAckFrame(Line, &SeqEcho);
        if (AckResult == LORA_TDM_ACK_OK)
        {
            if (SeqEcho != AppData->LastSentSeq)
            {
                AppData->SeqFailCount++;
                CFE_EVS_SendEvent(LORA_TDM_APP_SEQ_FAIL_EID, CFE_EVS_EventType_ERROR,
                                  "LORA_TDM_APP: ACK seq mismatch tx=%lu rx=%lu",
                                  (unsigned long)AppData->LastSentSeq, (unsigned long)SeqEcho);
            }
            AppData->RxAckCount++;
            AppData->NoAckCount         = 0;
            AppData->LastAckTimestampMs = UtilsGetTimeMs();
        }
        else
        {
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_ACK_PARSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: bad ACK frame");
        }
        return;
    }

    if (strncmp(Line, "UP,", 3) == 0)
    {
        ProcessUpFrame(Line, AppData);
        return;
    }

    /* Unknown frame type — ignore silently */
}

/* ---- Forward UP2 결과를 uplink_app으로 SB 전달 (ProcessUpFrame의 v2 대응) ---- */

static void ForwardUp2ToUplinkApp(const LORA_TDM_APP_Up2Decoded_t *Decoded, LORA_TDM_APP_Data_t *AppData)
{
    LORA_TDM_APP_UplinkFwdCmd_t FwdCmd;

    memset(&FwdCmd, 0, sizeof(FwdCmd));
    CFE_MSG_Init(CFE_MSG_PTR(FwdCmd.CommandHeader),
                 CFE_SB_ValueToMsgId(LORA_TDM_APP_UPLINK_APP_CMD_MID_VALUE),
                 sizeof(FwdCmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(FwdCmd.CommandHeader), LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC);
    FwdCmd.Version       = Decoded->Version;
    FwdCmd.CommandClass  = Decoded->CommandClass;
    FwdCmd.PayloadLength = Decoded->PayloadLen;
    FwdCmd.Flags         = 0;
    FwdCmd.Sequence      = Decoded->Seq;
    if (Decoded->PayloadLen > 0)
    {
        memcpy(FwdCmd.Payload, Decoded->Payload, Decoded->PayloadLen);
    }

    /* ProcessUpFrame과 동일한 CRC 대상 구성 (Version+CommandClass+PayloadLength+Flags+Seq(LE)+Payload) */
    {
        uint8 CrcBuf[6 + 196];
        CrcBuf[0] = Decoded->Version;
        CrcBuf[1] = Decoded->CommandClass;
        CrcBuf[2] = Decoded->PayloadLen;
        CrcBuf[3] = 0;
        CrcBuf[4] = (uint8)(Decoded->Seq & 0xFF);
        CrcBuf[5] = (uint8)(Decoded->Seq >> 8);
        if (Decoded->PayloadLen > 0)
        {
            memcpy(&CrcBuf[6], Decoded->Payload, Decoded->PayloadLen);
        }
        FwdCmd.Checksum = LORA_TDM_APP_Crc16(CrcBuf, 6u + Decoded->PayloadLen);
    }

    CFE_SB_TransmitMsg(CFE_MSG_PTR(FwdCmd.CommandHeader), true);

    AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK;
    AppData->RxCmdCount++;
}

/* ---- 완성된 v2 바이너리 프레임 1개 처리 (ProcessRxLine의 v2 대응) ---- */

void LORA_TDM_APP_ProcessRxBinaryFrame(const uint8 *Buf, size_t Len, LORA_TDM_APP_Data_t *AppData)
{
    uint32                    SeqEcho;
    LORA_TDM_APP_Up2Decoded_t Decoded;

    if (Buf == NULL || Len == 0 || AppData == NULL)
    {
        return;
    }

    if (Buf[0] == (uint8)LORA_TDM_APP_ACK2_MAGIC)
    {
        if (LORA_TDM_APP_ParseAck2Frame(Buf, Len, &SeqEcho) == LORA_TDM_ACK_OK)
        {
            if (SeqEcho != AppData->LastSentSeq)
            {
                AppData->SeqFailCount++;
                CFE_EVS_SendEvent(LORA_TDM_APP_SEQ_FAIL_EID, CFE_EVS_EventType_ERROR,
                                  "LORA_TDM_APP: ACK2 seq mismatch tx=%lu rx=%lu",
                                  (unsigned long)AppData->LastSentSeq, (unsigned long)SeqEcho);
            }
            AppData->RxAckCount++;
            AppData->NoAckCount         = 0;
            AppData->LastAckTimestampMs = UtilsGetTimeMs();
        }
        else
        {
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_ACK_PARSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: bad ACK2 frame");
        }
        return;
    }

    if (Buf[0] == (uint8)LORA_TDM_APP_UP2_MAGIC)
    {
        if (LORA_TDM_APP_ParseUp2Frame(Buf, Len, &Decoded) == LORA_TDM_ACK_OK)
        {
            ForwardUp2ToUplinkApp(&Decoded, AppData);
        }
        else
        {
            AppData->PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_CRC_FAIL;
            AppData->RxErrorCount++;
            CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: UP2 frame parse error");
        }
        return;
    }

    /* Unknown magic — ignore silently */
}

/* ---- CONFIG_CMD_MID 처리 (§8, openMCT UPLINK_CLASS_CONFIG 경로) ---- */

static uint16 ConfigChecksum(const LORA_TDM_APP_ConfigPayloadHdr_t *Hdr, const uint8 *ValueBytes, uint8 ValueLength)
{
    uint16 Sum = 0;
    uint8  i;

    Sum += (uint16)Hdr->ConfigScope;
    Sum += (uint16)Hdr->ConfigVersion;
    Sum += (uint16)(Hdr->ParameterId & 0xFFU);
    Sum += (uint16)((Hdr->ParameterId >> 8U) & 0xFFU);
    Sum += (uint16)Hdr->ValueType;
    Sum += (uint16)Hdr->ValueLength;
    for (i = 0; i < ValueLength; i++)
    {
        Sum += (uint16)ValueBytes[i];
    }
    return Sum;
}

/* BL-08(2026-07-22): uplink_app에 실행결과 회신 — 공용 EXEC_RESULT_MID.
 * lora_tdm_app은 CONFIG 결과를 저장하는 필드가 원래 없었으므로(이전엔
 * ErrCounter만 증가) DetailCode는 파라미터별 세부코드가 아니라 단순
 * 0=OK/1=FAILED 반복(즉 GenericResult와 동일값)으로 채운다. */
static void LORA_TDM_APP_PublishExecResult(uint16 SourceSequence, uint8 CommandClass, bool Ok)
{
    LORA_TDM_APP_ExecResultTlm_t *Tlm = &LORA_TDM_APP_Data.ExecResultTlm;

    Tlm->SourceSequence = SourceSequence;
    Tlm->SourceApp      = (uint8)EXEC_RESULT_SOURCE_LORA_TDM;
    Tlm->CommandClass   = CommandClass;
    Tlm->GenericResult  = Ok ? (uint8)EXEC_RESULT_GENERIC_OK : (uint8)EXEC_RESULT_GENERIC_FAILED;
    Tlm->DetailCode     = Tlm->GenericResult;

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm->TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm->TelemetryHeader), true);
}

void LORA_TDM_APP_ProcessConfigCommand(const LORA_TDM_APP_ConfigCmdTlm_t *Msg)
{
    const LORA_TDM_APP_ConfigPayloadHdr_t *Hdr;
    const uint8                           *ValueBytes;
    uint32                                  Value;
    uint16                                  Expected;

    if (Msg == NULL || Msg->PayloadLength < (uint8)sizeof(LORA_TDM_APP_ConfigPayloadHdr_t))
    {
        return;
    }

    Hdr = (const LORA_TDM_APP_ConfigPayloadHdr_t *)Msg->Payload;

    /* scope가 다르면 (cfs_core_app/mavlink_bridge_app 대상) 조용히 무시 — 여러 앱이
     * 같은 MID를 구독하는 정상 상황이라 에러가 아니다. */
    if (Hdr->ConfigScope != (uint8)LORA_TDM_APP_CONFIG_SCOPE)
    {
        return;
    }

    if (Hdr->ConfigVersion != (uint8)LORA_TDM_APP_CONFIG_VERSION)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        LORA_TDM_APP_PublishExecResult(Msg->SourceSequence, 1U, false);
        return;
    }

    if (Hdr->ValueLength != sizeof(uint32) ||
        (uint8)(sizeof(*Hdr) + Hdr->ValueLength) > Msg->PayloadLength)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        LORA_TDM_APP_PublishExecResult(Msg->SourceSequence, 1U, false);
        return;
    }

    ValueBytes = Msg->Payload + sizeof(*Hdr);
    Expected   = ConfigChecksum(Hdr, ValueBytes, Hdr->ValueLength);
    if (Hdr->Checksum != Expected)
    {
        LORA_TDM_APP_Data.ErrCounter++;
        CFE_EVS_SendEvent(LORA_TDM_APP_CRC_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: config checksum mismatch got=0x%04X expected=0x%04X",
                          (unsigned int)Hdr->Checksum, (unsigned int)Expected);
        LORA_TDM_APP_PublishExecResult(Msg->SourceSequence, 1U, false);
        return;
    }

    bool ConfigOk = false;

    memcpy(&Value, ValueBytes, sizeof(Value));

    switch (Hdr->ParameterId)
    {
        case LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL:
            /* BL-16(2026-07-21): 0/1 외 값은 거부 — lora_tdm_app_cmds.c의
             * LORA_TDM_APP_SetDownlinkProtocol()와 동일 정책(기체 엄격화). */
            if (Value != 0U && Value != 1U)
            {
                LORA_TDM_APP_Data.ErrCounter++;
                CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "LORA_TDM_APP: rejected downlink protocol value=%u via CONFIG_CMD_MID (only 0/1 accepted)",
                                  (unsigned int)Value);
                break;
            }
            LORA_TDM_APP_Data.UseV2Downlink = (Value != 0U) ? 1U : 0U;
            LORA_TDM_APP_Data.CmdCounter++;
            LORA_TDM_APP_SaveState(); /* BL-41(2026-07-23): CONFIG 적용 성공 즉시 영속화 */
            CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: downlink protocol set to %s (via CONFIG_CMD_MID)",
                              LORA_TDM_APP_Data.UseV2Downlink ? "v2(DL2)" : "v1(text)");
            ConfigOk = true;
            break;

        default:
            LORA_TDM_APP_Data.ErrCounter++;
            break;
    }

    LORA_TDM_APP_PublishExecResult(Msg->SourceSequence, 1U, ConfigOk);
}

/* ---- Update FC state cache from SB message ---- */

void LORA_TDM_APP_UpdateCacheFromMsg(CFE_SB_Buffer_t *SBBufPtr, LORA_TDM_APP_Data_t *AppData)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE)))
    {
        const FC_ATTITUDE_TLM_t *M = (const FC_ATTITUDE_TLM_t *)SBBufPtr;
        AppData->FcState.TimestampMs   = M->TimestampMs;
        AppData->FcState.AttitudeValid = M->Valid;
        AppData->FcState.RollRad       = M->RollRad;
        AppData->FcState.PitchRad      = M->PitchRad;
        AppData->FcState.YawRad        = M->YawRad;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE)))
    {
        const FC_EKF_LOCAL_TLM_t *M = (const FC_EKF_LOCAL_TLM_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.LocalValid  = M->Valid;
        AppData->FcState.PosX        = M->X_m;
        AppData->FcState.PosY        = M->Y_m;
        AppData->FcState.PosZ        = M->Z_m;
        AppData->FcState.VelX        = M->Vx_mps;
        AppData->FcState.VelY        = M->Vy_mps;
        AppData->FcState.VelZ        = M->Vz_mps;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE)))
    {
        const FC_GPS_RAW_TLM_t *M = (const FC_GPS_RAW_TLM_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.GpsValid    = M->Valid;
        AppData->FcState.LatE7       = M->LatE7;
        AppData->FcState.LonE7       = M->LonE7;
        AppData->FcState.AltMm       = M->AltMm;
        AppData->FcState.GpsFix      = M->FixType;
        AppData->FcState.SatellitesVisible = M->SatellitesVisible;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_SYS_TIME_MID_VALUE)))
    {
        /* Layout must match mavlink_bridge_app MAVLINK_BRIDGE_APP_SysTimeTlm_t */
        typedef struct {
            CFE_MSG_TelemetryHeader_t Hdr;
            uint32 TimestampMs; uint32 Seq;
            uint8 Valid; uint8 Stale; uint8 ErrorCode; uint8 Reserved;
            uint64 TimeUnixUsec;
        } SysTimeMsg_t;
        const SysTimeMsg_t *M = (const SysTimeMsg_t *)SBBufPtr;
        AppData->FcState.TimeUnixUsec = M->TimeUnixUsec;
        AppData->FcState.TimeValid    = M->Valid;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE)))
    {
        const SYSTEM_HEALTH_TLM_t *M = (const SYSTEM_HEALTH_TLM_t *)SBBufPtr;
        AppData->SystemHealth.TimestampMs       = M->TimestampMs;
        AppData->SystemHealth.SystemHealthState = M->HealthState;
        AppData->SystemHealth.FaultCode         = M->FaultCode;
        AppData->PacketType = LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE;
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE)))
    {
        const FC_STATE_PREFIX_t *M = (const FC_STATE_PREFIX_t *)SBBufPtr;
        AppData->FcState.TimestampMs = M->TimestampMs;
        AppData->FcState.EkfValid    = M->Valid;
        AppData->PacketType = LORA_TDM_APP_FC_STATE_PACKET_TYPE;
    }
}

void LORA_TDM_APP_ProcessDiagnosticCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    const LORA_TDM_APP_DiagnosticCmdTlm_t *Msg = (const LORA_TDM_APP_DiagnosticCmdTlm_t *)SBBufPtr;

    /* waypoint readback(2026-07-23): DIAGNOSTIC_CMD_MID을 cfs_core_app과
     * 공동구독하게 되며 DiagTarget으로 대상 구분 — 자기 것 아니면 조용히 무시 */
    if (Msg->DiagTarget != LORA_TDM_APP_DIAG_TARGET_LORA_TDM)
    {
        return;
    }

    LORA_TDM_APP_Data.CmdCounter++;

    switch (Msg->DiagAction)
    {
        case LORA_TDM_APP_DIAG_ACTION_LINK_STATUS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag LINK_STATUS seq=%u link=%u noack=%lu dlseq=%lu token=%lu",
                              (unsigned int)Msg->SourceSequence, (unsigned int)LORA_TDM_APP_Data.LinkState,
                              (unsigned long)LORA_TDM_APP_Data.NoAckCount,
                              (unsigned long)LORA_TDM_APP_Data.DownlinkSeq,
                              (unsigned long)Msg->RequestToken);
            break;

        case LORA_TDM_APP_DIAG_ACTION_RX_STATS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag RX_STATS seq=%u rxcmd=%lu rxack=%lu rxerr=%u token=%lu",
                              (unsigned int)Msg->SourceSequence,
                              (unsigned long)LORA_TDM_APP_Data.RxCmdCount,
                              (unsigned long)LORA_TDM_APP_Data.RxAckCount,
                              (unsigned int)LORA_TDM_APP_Data.RxErrorCount,
                              (unsigned long)Msg->RequestToken);
            break;

        case LORA_TDM_APP_DIAG_ACTION_TX_STATS:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "LORA_TDM_APP: diag TX_STATS seq=%u txcount=%lu token=%lu",
                              (unsigned int)Msg->SourceSequence,
                              (unsigned long)LORA_TDM_APP_Data.TxCount,
                              (unsigned long)Msg->RequestToken);
            break;

        default:
            CFE_EVS_SendEvent(LORA_TDM_APP_DIAGNOSTIC_CMD_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: diag UNKNOWN action=%u seq=%u",
                              (unsigned int)Msg->DiagAction, (unsigned int)Msg->SourceSequence);
            break;
    }
}

/* waypoint readback(2026-07-23, spec §4.3): cfs_core_app이 ROUTE_SNAPSHOT_MID로
 * 발행한 mission route를 로컬 캐시에 저장하고 페이징 상태 초기화.
 * 실제 첨부는 BuildDl2Frame()이 매 사이클 RouteReadbackPending을 보고 수행. */
void LORA_TDM_APP_ProcessRouteSnapshot(const LORA_TDM_APP_RouteSnapshotTlm_t *Msg)
{
    uint8 Count = Msg->WaypointCount;

    if (Count > ROUTE_MAX_WAYPOINTS)
    {
        Count = ROUTE_MAX_WAYPOINTS;
    }

    LORA_TDM_APP_Data.RouteType           = Msg->RouteType;
    LORA_TDM_APP_Data.RouteWaypointCount  = Count;
    memcpy(LORA_TDM_APP_Data.RouteWaypoints, Msg->Waypoints, sizeof(ROUTE_WAYPOINT_t) * Count);

    LORA_TDM_APP_Data.RoutePageIndex  = 0;
    LORA_TDM_APP_Data.RouteTotalPages = (Count == 0U) ? 0U :
        (uint8)((Count + LORA_TDM_APP_DL2_WAYPOINTS_PER_PAGE - 1U) / LORA_TDM_APP_DL2_WAYPOINTS_PER_PAGE);
    LORA_TDM_APP_Data.RouteReadbackPending = (LORA_TDM_APP_Data.RouteTotalPages > 0U) ? 1U : 0U;

    CFE_EVS_SendEvent(LORA_TDM_APP_ROUTE_SNAPSHOT_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: route snapshot received wp_count=%u total_pages=%u",
                      (unsigned int)Count, (unsigned int)LORA_TDM_APP_Data.RouteTotalPages);
}

/* -----------------------------------------------------------------------
 * BL-41(2026-07-23): CONFIG 영속화 — UseV2Downlink 단일 필드(축소 구현).
 * cfs_core_app/uplink_app(BL-17/18)과 동일한 매직+체크섬+ConfigVersion+
 * 원자적 tmp-write→fsync→rename 패턴.
 * ----------------------------------------------------------------------- */

static const char *LORA_TDM_APP_GetStateFilePath(void)
{
    const char *EnvPath = getenv("LORA_TDM_APP_STATE_FILE_PATH");

    if (EnvPath != NULL && EnvPath[0] != '\0')
    {
        return EnvPath;
    }
    return LORA_TDM_APP_STATE_FILE_PATH;
}

static uint32 LORA_TDM_APP_ComputeStateChecksum(const LORA_TDM_APP_PersistentState_t *State)
{
    return State->Magic + (uint32)State->ConfigVersion + (uint32)State->UseV2Downlink;
}

void LORA_TDM_APP_LoadState(void)
{
    LORA_TDM_APP_PersistentState_t State;
    int                             Fd;
    ssize_t                         ReadRc;
    const char                     *StatePath = LORA_TDM_APP_GetStateFilePath();

    Fd = open(StatePath, O_RDONLY);
    if (Fd < 0)
    {
        if (errno != ENOENT)
        {
            CFE_EVS_SendEvent(LORA_TDM_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                              "LORA_TDM_APP: state file open failed errno=%d, using defaults", errno);
        }
        return;
    }

    ReadRc = read(Fd, &State, sizeof(State));
    close(Fd);

    if (ReadRc != (ssize_t)sizeof(State))
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state file truncated (rc=%ld), using defaults", (long)ReadRc);
        return;
    }
    if (State.Magic != LORA_TDM_APP_STATE_MAGIC)
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state file bad magic (0x%08lX), using defaults",
                          (unsigned long)State.Magic);
        return;
    }
    if (State.Checksum != LORA_TDM_APP_ComputeStateChecksum(&State))
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state file checksum mismatch, using defaults");
        return;
    }
    if (State.ConfigVersion != LORA_TDM_APP_CONFIG_VERSION)
    {
        /* BL-41: 필드 구조가 바뀐 구버전 파일 — 전체 기본값 폴백 */
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_CORRUPT_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state file config version mismatch (file=%u expected=%u), using defaults",
                          (unsigned int)State.ConfigVersion, (unsigned int)LORA_TDM_APP_CONFIG_VERSION);
        return;
    }

    LORA_TDM_APP_Data.UseV2Downlink = (State.UseV2Downlink != 0U) ? 1U : 0U;

    CFE_EVS_SendEvent(LORA_TDM_APP_SET_DL_PROTO_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "LORA_TDM_APP: restored downlink protocol %s",
                      LORA_TDM_APP_Data.UseV2Downlink ? "v2(DL2)" : "v1(text)");
}

void LORA_TDM_APP_SaveState(void)
{
    LORA_TDM_APP_PersistentState_t State;
    char                            TmpPath[256];
    int                             Fd;
    const char                     *StatePath = LORA_TDM_APP_GetStateFilePath();

    memset(&State, 0, sizeof(State));
    State.Magic         = LORA_TDM_APP_STATE_MAGIC;
    State.ConfigVersion = LORA_TDM_APP_CONFIG_VERSION;
    State.UseV2Downlink = LORA_TDM_APP_Data.UseV2Downlink;
    State.Checksum      = LORA_TDM_APP_ComputeStateChecksum(&State);

    snprintf(TmpPath, sizeof(TmpPath), "%s.tmp", StatePath);

    Fd = open(TmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (Fd < 0)
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state save open failed path=%s errno=%d", TmpPath, errno);
        return;
    }

    if (write(Fd, &State, sizeof(State)) != (ssize_t)sizeof(State))
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state save write failed path=%s errno=%d", TmpPath, errno);
        close(Fd);
        return;
    }

    /* rename()이 디스크에 반영됐다는 보장은 파일 fd만 fsync해선 안 됨 —
     * POSIX상 디렉터리 항목(rename) 자체도 부모 디렉터리 fd를 fsync해야
     * 정전 시 유실되지 않는다 (BL-18). */
    fsync(Fd);
    close(Fd);

    if (rename(TmpPath, StatePath) != 0)
    {
        CFE_EVS_SendEvent(LORA_TDM_APP_STATE_SAVE_FAIL_EID, CFE_EVS_EventType_ERROR,
                          "LORA_TDM_APP: state save rename failed path=%s errno=%d", StatePath, errno);
        return;
    }

    {
        char  DirPath[256];
        char *Slash;
        int   DirFd;

        snprintf(DirPath, sizeof(DirPath), "%s", StatePath);
        Slash = strrchr(DirPath, '/');
        if (Slash != NULL)
        {
            *Slash = '\0';
            DirFd  = open(DirPath, O_RDONLY);
            if (DirFd >= 0)
            {
                fsync(DirFd);
                close(DirFd);
            }
        }
    }
}

/************************************************************************
 * NASA Docket No. GSC-19,200-1, and identified as "cFS Draco"
 *
 * Copyright (c) 2023 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *   TELEMETRY_APP Application Topic IDs
 */
#ifndef TELEMETRY_APP_TOPICIDS_H
#define TELEMETRY_APP_TOPICIDS_H

#include "telemetry_app_topicid_values.h"

#define TELEMETRY_APP_MISSION_CMD_TOPICID              TELEMETRY_APP_MISSION_TIDVAL(CMD)
#define DEFAULT_TELEMETRY_APP_MISSION_CMD_TOPICID      0x82

#define TELEMETRY_APP_MISSION_SEND_HK_TOPICID          TELEMETRY_APP_MISSION_TIDVAL(SEND_HK)
#define DEFAULT_TELEMETRY_APP_MISSION_SEND_HK_TOPICID  0x83

#define TELEMETRY_APP_MISSION_HK_TLM_TOPICID           TELEMETRY_APP_MISSION_TIDVAL(HK_TLM)
#define DEFAULT_TELEMETRY_APP_MISSION_HK_TLM_TOPICID   0x0846

#define TELEMETRY_APP_MISSION_MONITOR_TOPICID          TELEMETRY_APP_MISSION_TIDVAL(MONITOR)
#define DEFAULT_TELEMETRY_APP_MISSION_MONITOR_TOPICID  0x0847

#define TELEMETRY_APP_MISSION_STATUS_TOPICID           TELEMETRY_APP_MISSION_TIDVAL(STATUS)
#define DEFAULT_TELEMETRY_APP_MISSION_STATUS_TOPICID   0x0848

#endif

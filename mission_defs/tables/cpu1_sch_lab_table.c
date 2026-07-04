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

#include "cfe_tbl_filedef.h" /* Required to obtain the CFE_TBL_FILEDEF macro definition */
#include "sch_lab_tbl.h"
#include "cfe_sb_api_typedefs.h" /* Required to use the CFE_SB_MSGID_WRAP_VALUE macro */

/* This is for the standard set of CFE core app MsgID values */
#include "cfe_msgids.h"

/*
** SCH Lab schedule table — mission override (cpu1)
**
** The stock sch_lab_table.c ships as an empty placeholder, so without this
** override none of our apps' SEND_HK is ever requested. cfs_core_app waits
** for mavlink_bridge_app's BRIDGE_HK to know the bridge is alive
** (BridgeState.Received), so a missing SEND_HK schedule entry for it makes
** cfs_core_app permanently declare BRIDGE_TIMEOUT even while the bridge is
** healthy (see notes/spec_code_audit.md, 2026-06-17 finding).
**
** TickRate=100 (ticks/sec) and PacketRate=N means "every N ticks", so a
** PacketRate near 100 is roughly 1 Hz. Values are offset slightly so the
** four requests don't all land on the same tick.
*/

SCH_LAB_ScheduleTable_t Schedule = {
    .TickRate = 100,
    .Config   = {
        {CFE_SB_MSGID_WRAP_VALUE(0x18A1), 91, 0}, /* MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE */
        {CFE_SB_MSGID_WRAP_VALUE(0x18C1), 92, 0}, /* CFS_CORE_APP_SEND_HK_MID_VALUE */
        {CFE_SB_MSGID_WRAP_VALUE(0x18D1), 93, 0}, /* UPLINK_APP_SEND_HK_MID_VALUE */
        {CFE_SB_MSGID_WRAP_VALUE(0x18E1), 94, 0}, /* LORA_TDM_APP_SEND_HK_MID_VALUE */
    }
};

/*
** The macro below identifies:
**    1) the data structure type to use as the table image format
**    2) the name of the table to be placed into the cFE Table File Header
**    3) a brief description of the contents of the file image
**    4) the desired name of the table image binary file that is cFE compatible
*/
CFE_TBL_FILEDEF(Schedule, SCH_LAB.Schedule, Schedule Lab MsgID Table, sch_lab_table.tbl)

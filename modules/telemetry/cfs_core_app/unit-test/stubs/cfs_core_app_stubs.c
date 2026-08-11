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
 *
 * Auto-Generated stub implementations for functions defined in cfs_core_app header
 */

#include "cfs_core_app.h"
#include "utgenstub.h"

/*
 * ----------------------------------------------------
 * Generated stub function for CFS_CORE_APP_Init()
 * ----------------------------------------------------
 */
CFE_Status_t CFS_CORE_APP_Init(void)
{
    UT_GenStub_SetupReturnBuffer(CFS_CORE_APP_Init, CFE_Status_t);

    UT_GenStub_Execute(CFS_CORE_APP_Init, Basic, NULL);

    return UT_GenStub_GetReturnValue(CFS_CORE_APP_Init, CFE_Status_t);
}

/*
 * ----------------------------------------------------
 * Generated stub function for CFS_CORE_APP_Main()
 * ----------------------------------------------------
 */
void CFS_CORE_APP_Main(void)
{

    UT_GenStub_Execute(CFS_CORE_APP_Main, Basic, NULL);
}



#ifndef LORA_TDM_APP_COVERAGETEST_COMMON_H
#define LORA_TDM_APP_COVERAGETEST_COMMON_H

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "setup.h"
#include "eventcheck.h"

#include "cfe.h"
#include "lora_tdm_app_eventids.h"
#include "lora_tdm_app.h"
#include "lora_tdm_app_dispatch.h"
#include "lora_tdm_app_cmds.h"
#include "lora_tdm_app_utils.h"
#include "lora_tdm_app_topicids.h"
#include "lora_tdm_app_msg.h"

#define ADD_TEST(test) \
    UtTest_Add((Test_##test), LORA_TDM_APP_UT_Setup, LORA_TDM_APP_UT_TearDown, #test)

#endif

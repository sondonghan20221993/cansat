#ifndef LORA_FC_DOWNLINK_APP_COVERAGETEST_COMMON_H
#define LORA_FC_DOWNLINK_APP_COVERAGETEST_COMMON_H

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "setup.h"
#include "eventcheck.h"

#include "cfe.h"
#include "lora_fc_downlink_app_eventids.h"
#include "lora_fc_downlink_app.h"
#include "lora_fc_downlink_app_dispatch.h"
#include "lora_fc_downlink_app_cmds.h"
#include "lora_fc_downlink_app_utils.h"
#include "lora_fc_downlink_app_msgids.h"
#include "lora_fc_downlink_app_msg.h"

#define ADD_TEST(test) \
    UtTest_Add((Test_##test), LORA_FC_DOWNLINK_APP_UT_Setup, LORA_FC_DOWNLINK_APP_UT_TearDown, #test)

#endif

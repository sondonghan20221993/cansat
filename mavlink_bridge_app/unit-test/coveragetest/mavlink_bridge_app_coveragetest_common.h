#ifndef MAVLINK_BRIDGE_APP_COVERAGETEST_COMMON_H
#define MAVLINK_BRIDGE_APP_COVERAGETEST_COMMON_H

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "setup.h"
#include "eventcheck.h"

#include "cfe.h"
#include "mavlink_bridge_app_eventids.h"
#include "mavlink_bridge_app.h"
#include "mavlink_bridge_app_dispatch.h"
#include "mavlink_bridge_app_cmds.h"
#include "mavlink_bridge_app_utils.h"
#include "mavlink_bridge_app_fcncodes.h"
#include "mavlink_bridge_app_msgids.h"
#include "mavlink_bridge_app_msg.h"

#define ADD_TEST(test) \
    UtTest_Add((Test_##test), MAVLINK_BRIDGE_APP_UT_Setup, MAVLINK_BRIDGE_APP_UT_TearDown, #test)

/* coverage-mav_bridge_app-* 타겟명에 맞게 공통 prefix 정의 */
#define UT_APP_PREFIX "mav_bridge_app"

#endif

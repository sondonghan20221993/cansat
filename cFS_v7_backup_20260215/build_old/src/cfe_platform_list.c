/* This file is auto-generated from CMake build system.  Do not manually edit! */
#include "cfeconfig_platformdata_tool.h"

#undef CFE_PLATFORM
#define CFE_PLATFORM(x) extern const CFE_ConfigTool_DetailEntry_t CFECONFIG_PLATFORMDATA_ ## x[];
CFE_PLATFORM(aarch64_linux_gnu_default_cpu1)


#undef CFE_PLATFORM
#define CFE_PLATFORM(x) { #x, CFECONFIG_PLATFORMDATA_ ## x },
const CFE_ConfigTool_PlatformMapEntry_t CFECONFIG_PLATFORMDATA_TABLE[] =
{
CFE_PLATFORM(aarch64_linux_gnu_default_cpu1)

    { NULL }
};

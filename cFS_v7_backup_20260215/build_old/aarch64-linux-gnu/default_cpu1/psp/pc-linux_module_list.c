/* This file is generated via CMake - do not edit in place */
#include "cfe_psp_module.h"

extern CFE_PSP_ModuleApi_t CFE_PSP_soft_timebase_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_timebase_posix_clock_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_eeprom_mmap_file_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_ram_notimpl_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_port_notimpl_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_iodriver_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_linux_sysmon_API;
extern CFE_PSP_ModuleApi_t CFE_PSP_endian_api_API;


CFE_StaticModuleLoadEntry_t CFE_PSP_BASE_MODULE_LIST[] =
{
{ .Name = "soft_timebase", .Api = &CFE_PSP_soft_timebase_API },
{ .Name = "timebase_posix_clock", .Api = &CFE_PSP_timebase_posix_clock_API },
{ .Name = "eeprom_mmap_file", .Api = &CFE_PSP_eeprom_mmap_file_API },
{ .Name = "ram_notimpl", .Api = &CFE_PSP_ram_notimpl_API },
{ .Name = "port_notimpl", .Api = &CFE_PSP_port_notimpl_API },
{ .Name = "iodriver", .Api = &CFE_PSP_iodriver_API },
{ .Name = "linux_sysmon", .Api = &CFE_PSP_linux_sysmon_API },
{ .Name = "endian_api", .Api = &CFE_PSP_endian_api_API },

{ NULL }
};

/* END OF FILE */

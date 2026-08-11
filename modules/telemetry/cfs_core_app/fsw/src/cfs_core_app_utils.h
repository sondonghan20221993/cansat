#ifndef CFS_CORE_APP_UTILS_H
#define CFS_CORE_APP_UTILS_H

#include "cfs_core_app.h"
#include "bridge_hk_msg.h"
#include "fc_state_msg.h"

typedef FC_STATE_PREFIX_t CFS_CORE_APP_GenericStateTlm_t;

typedef BRIDGE_HK_TLM_t CFS_CORE_APP_BridgeHkMirror_t;

void CFS_CORE_APP_LoadState(void);
void CFS_CORE_APP_SaveState(void);
void CFS_CORE_APP_ProcessConfigCommand(const CFS_CORE_APP_ConfigCmdTlm_t *Msg);
void CFS_CORE_APP_ProcessViewpointCommand(const CFS_CORE_APP_ViewpointCmdTlm_t *Msg);
void CFS_CORE_APP_ProcessRecoveryCommand(const CFS_CORE_APP_RecoveryCmdTlm_t *Msg);
void CFS_CORE_APP_ProcessModeCommand(const CFS_CORE_APP_ModeCmdTlm_t *Msg);
void CFS_CORE_APP_ProcessDiagnosticCommand(const CFS_CORE_APP_DiagnosticCmdTlm_t *Msg);

#endif


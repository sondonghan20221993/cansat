#ifndef IMG_APP_H
#define IMG_APP_H

#include "cfe.h"
#include "cfe_config.h"

#include "img_app_mission_cfg.h"
#include "img_app_platform_cfg.h"
#include "img_app_internal_cfg.h"

#include "img_app_perfids.h"
#include "img_app_msg.h"
#include "img_app_msgids.h"

typedef struct
{
    uint8                 CmdCounter;
    uint8                 ErrCounter;
    uint16                Reserved;
    uint32                RunStatus;
    uint32                SequenceCounter;
    uint32                PublishCount;
    uint32                LastPublishTimeMs;
    CFE_SB_PipeId_t       CommandPipe;
    IMG_APP_HkTlm_t       HkTlm;
    IMG_APP_ImageMetaTlm_t ImageMetaTlm;
} IMG_APP_Data_t;

extern IMG_APP_Data_t IMG_APP_Data;

void         IMG_APP_Main(void);
CFE_Status_t IMG_APP_Init(void);
void         IMG_APP_ReportHousekeeping(void);
bool         IMG_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);
void         IMG_APP_ServicePrototype(void);

#endif

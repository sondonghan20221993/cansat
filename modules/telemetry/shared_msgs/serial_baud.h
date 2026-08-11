#ifndef SERIAL_BAUD_H
#define SERIAL_BAUD_H

#include <termios.h>
#include "common_types.h"

/* BL-19(2026-07-22): mavlink_bridge_app/lora_tdm_app 둘 다 설정 가능한
 * 정수 baudrate를 termios speed_t로 변환해야 해서 중복 구현 대신 공유.
 * mavlink_bridge_app이 먼저 갖고 있던 lookup을 그대로 이관. */
static inline bool SERIAL_BAUD_GetConstant(uint32 Baudrate, speed_t *BaudConstant)
{
    switch (Baudrate)
    {
        case 9600:
            *BaudConstant = B9600;
            return true;
        case 19200:
            *BaudConstant = B19200;
            return true;
        case 38400:
            *BaudConstant = B38400;
            return true;
        case 57600:
            *BaudConstant = B57600;
            return true;
        case 115200:
            *BaudConstant = B115200;
            return true;
        case 230400:
            *BaudConstant = B230400;
            return true;
        case 460800:
            *BaudConstant = B460800;
            return true;
        case 921600:
            *BaudConstant = B921600;
            return true;
        default:
            return false;
    }
}

#endif

#ifndef __APP_WIFI_H__
#define __APP_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* WiFi 状态只描述 ESP32 是否连上热点，不代表天气数据一定已经获取成功。 */
typedef enum {
    APP_WIFI_STATE_IDLE = 0,
    APP_WIFI_STATE_READY,
    APP_WIFI_STATE_ERROR
} AppWifi_State_t;

void AppWifi_Init(void);
uint8_t AppWifi_Join(void);
/* 仅检查当前连接状态，不在本函数里循环重连。 */
uint8_t AppWifi_CheckConnected(void);
void AppWifi_PrintState(void);
AppWifi_State_t AppWifi_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_WIFI_H__ */

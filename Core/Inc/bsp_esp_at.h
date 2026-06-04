#ifndef __BSP_ESP_AT_H__
#define __BSP_ESP_AT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BSP_ESP_AT_OK             0
#define BSP_ESP_AT_TIMEOUT       -1
#define BSP_ESP_AT_ERROR         -2
#define BSP_ESP_AT_PARAM_ERROR   -3

void BspEspAt_Init(void);
void BspEspAt_Reset(void);
void BspEspAt_WaitHook(void);
int BspEspAt_SendCmd(const char *cmd,
                     const char *expect,
                     char *response,
                     uint16_t response_size,
                     uint32_t timeout_ms);
int BspEspAt_SendRaw(const uint8_t *data, uint16_t len);
int BspEspAt_ReadUntil(const char *expect,
                       char *response,
                       uint16_t response_size,
                       uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ESP_AT_H__ */

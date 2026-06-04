#ifndef APP_DEBUG_UART_H
#define APP_DEBUG_UART_H

#include "main.h"
#include <stdint.h>

/* USART1 调试输出接口，供 WiFi、天气、LCD 等模块打印诊断信息。 */
void AppDebugUart_SendString(const char *text);
void AppDebugUart_SendBytes(const uint8_t *data, uint16_t len);

#endif /* APP_DEBUG_UART_H */

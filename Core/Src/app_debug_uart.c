#include "app_debug_uart.h"

#include "usart.h"
#include <string.h>

/*
 * 调试串口发送兜底层。
 * 部分流程会长时间占用 AT 串口等待响应，这里直接轮询 USART1 TX 标志，
 * 避免 HAL_UART_Transmit 在异常状态下阻塞太久。
 */

#define APP_DEBUG_UART_TIMEOUT_MS 20U

/* 调试输出前重新确认 PA9/USART1 TX 使能，避免被其他初始化覆盖。 */
static void AppDebugUart_EnsureTxReady(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    USART1->CR1 |= USART_CR1_TE;
}

void AppDebugUart_SendBytes(const uint8_t *data, uint16_t len)
{
    uint32_t start;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    AppDebugUart_EnsureTxReady();

    while (len > 0U)
    {
        start = HAL_GetTick();
        while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE) == RESET)
        {
            if ((HAL_GetTick() - start) > APP_DEBUG_UART_TIMEOUT_MS)
            {
                return;
            }
        }

        USART1->DR = (uint16_t)(*data & 0xFFU);
        data++;
        len--;
    }

    start = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - start) > APP_DEBUG_UART_TIMEOUT_MS)
        {
            return;
        }
    }
}

void AppDebugUart_SendString(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    AppDebugUart_SendBytes((const uint8_t *)text, (uint16_t)strlen(text));
}

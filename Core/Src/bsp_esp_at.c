#include "bsp_esp_at.h"
#include "usart.h"
#include <string.h>

#define BSP_ESP_EN_GPIO_PORT GPIOE
#define BSP_ESP_EN_GPIO_PIN  GPIO_PIN_4
#define BSP_ESP_AT_RX_CHUNK  1024U

static void BspEspAt_SetEnable(GPIO_PinState state);
static void BspEspAt_DrainRx(uint32_t timeout_ms);
static uint8_t esp_at_rx_chunk[BSP_ESP_AT_RX_CHUNK];

__weak void BspEspAt_WaitHook(void)
{
}

/**
  * @brief 初始化 ESP32-C3 AT 通信辅助硬件。
  *
  * 当前开发板用 PE4 控制 ESP32-EN。该脚拉高后，ESP32-C3 才会运行。
  * USART2 本身由 MX_USART2_UART_Init() 初始化。
  */
void BspEspAt_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    HAL_GPIO_WritePin(BSP_ESP_EN_GPIO_PORT, BSP_ESP_EN_GPIO_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = BSP_ESP_EN_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BSP_ESP_EN_GPIO_PORT, &GPIO_InitStruct);

    HAL_Delay(500U);
    BspEspAt_DrainRx(50U);
}

/**
  * @brief 通过 ESP32-EN 复位 ESP32-C3。
  */
void BspEspAt_Reset(void)
{
    BspEspAt_SetEnable(GPIO_PIN_RESET);
    HAL_Delay(120U);
    BspEspAt_SetEnable(GPIO_PIN_SET);
    HAL_Delay(1500U);
    BspEspAt_DrainRx(100U);
}

/**
  * @brief 发送 AT 命令并等待指定响应。
  *
  * response 会保存收到的原始响应，便于通过 USART1 打印排查。
  */
int BspEspAt_SendCmd(const char *cmd,
                     const char *expect,
                     char *response,
                     uint16_t response_size,
                     uint32_t timeout_ms)
{
    uint16_t rx_len = 0U;
    uint16_t total_len = 0U;
    uint32_t start_tick;
    HAL_StatusTypeDef status;

    if ((cmd == NULL) || (response == NULL) || (response_size == 0U))
    {
        return BSP_ESP_AT_PARAM_ERROR;
    }

    response[0] = '\0';
    BspEspAt_DrainRx(20U);

    status = HAL_UART_Transmit(&huart2,
                               (uint8_t *)cmd,
                               (uint16_t)strlen(cmd),
                               1000U);
    if (status != HAL_OK)
    {
        return BSP_ESP_AT_ERROR;
    }

    start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        uint32_t elapsed = HAL_GetTick() - start_tick;
        uint32_t remain = (elapsed < timeout_ms) ? (timeout_ms - elapsed) : 0U;
        uint32_t slice = (remain > 200U) ? 200U : remain;

        BspEspAt_WaitHook();

        if (slice < 20U)
        {
            slice = 20U;
        }

        rx_len = 0U;
        status = HAL_UARTEx_ReceiveToIdle(&huart2,
                                          esp_at_rx_chunk,
                                          sizeof(esp_at_rx_chunk),
                                          &rx_len,
                                          slice);

        if (rx_len > 0U)
        {
            uint16_t copy_len = rx_len;

            if (copy_len > (uint16_t)(response_size - 1U - total_len))
            {
                copy_len = (uint16_t)(response_size - 1U - total_len);
            }

            if (copy_len > 0U)
            {
                memcpy(&response[total_len], esp_at_rx_chunk, copy_len);
                total_len = (uint16_t)(total_len + copy_len);
                response[total_len] = '\0';
            }

            if ((expect != NULL) && (strstr(response, expect) != NULL))
            {
                return BSP_ESP_AT_OK;
            }

            if (strstr(response, "ERROR") != NULL)
            {
                return BSP_ESP_AT_ERROR;
            }

            if (total_len >= (uint16_t)(response_size - 1U))
            {
                break;
            }
        }
    }

    if ((expect == NULL) && (total_len > 0U))
    {
        return BSP_ESP_AT_OK;
    }

    return BSP_ESP_AT_TIMEOUT;
}

/**
  * @brief 发送不带 AT 命令格式的原始数据，例如 HTTP GET 请求正文。
  */
int BspEspAt_SendRaw(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((data == NULL) || (len == 0U))
    {
        return BSP_ESP_AT_PARAM_ERROR;
    }

    status = HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 3000U);
    return (status == HAL_OK) ? BSP_ESP_AT_OK : BSP_ESP_AT_ERROR;
}

/**
  * @brief 持续读取 ESP32-C3 返回，直到出现 expect 或超时。
  */
int BspEspAt_ReadUntil(const char *expect,
                       char *response,
                       uint16_t response_size,
                       uint32_t timeout_ms)
{
    uint16_t rx_len = 0U;
    uint16_t total_len = 0U;
    uint32_t start_tick;
    HAL_StatusTypeDef status;

    if ((response == NULL) || (response_size == 0U))
    {
        return BSP_ESP_AT_PARAM_ERROR;
    }

    response[0] = '\0';
    start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        uint32_t elapsed = HAL_GetTick() - start_tick;
        uint32_t remain = (elapsed < timeout_ms) ? (timeout_ms - elapsed) : 0U;
        uint32_t slice = (remain > 300U) ? 300U : remain;

        BspEspAt_WaitHook();

        if (slice < 20U)
        {
            slice = 20U;
        }

        rx_len = 0U;
        status = HAL_UARTEx_ReceiveToIdle(&huart2,
                                          esp_at_rx_chunk,
                                          sizeof(esp_at_rx_chunk),
                                          &rx_len,
                                          slice);
        (void)status;

        if (rx_len > 0U)
        {
            uint16_t copy_len = rx_len;

            if (copy_len > (uint16_t)(response_size - 1U - total_len))
            {
                copy_len = (uint16_t)(response_size - 1U - total_len);
            }

            if (copy_len > 0U)
            {
                memcpy(&response[total_len], esp_at_rx_chunk, copy_len);
                total_len = (uint16_t)(total_len + copy_len);
                response[total_len] = '\0';
            }

            if ((expect != NULL) && (strstr(response, expect) != NULL))
            {
                return BSP_ESP_AT_OK;
            }

            if (total_len >= (uint16_t)(response_size - 1U))
            {
                break;
            }
        }
    }

    if ((expect == NULL) && (total_len > 0U))
    {
        return BSP_ESP_AT_OK;
    }

    return BSP_ESP_AT_TIMEOUT;
}

static void BspEspAt_SetEnable(GPIO_PinState state)
{
    HAL_GPIO_WritePin(BSP_ESP_EN_GPIO_PORT, BSP_ESP_EN_GPIO_PIN, state);
}

static void BspEspAt_DrainRx(uint32_t timeout_ms)
{
    uint8_t chunk[32];
    uint16_t rx_len = 0U;
    uint32_t start_tick = HAL_GetTick();

    do
    {
        BspEspAt_WaitHook();

        rx_len = 0U;
        (void)HAL_UARTEx_ReceiveToIdle(&huart2,
                                       chunk,
                                       sizeof(chunk),
                                       &rx_len,
                                       10U);
    } while (((HAL_GetTick() - start_tick) < timeout_ms) && (rx_len > 0U));
}

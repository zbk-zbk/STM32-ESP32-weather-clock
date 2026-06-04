#include "bsp_lcd_fsmc.h"

/*
 * LCD 硬件初始化。
 *
 * LCD 控制器连接到 FSMC Bank1 NOR/SRAM4：
 * - PG12：NE4 片选。
 * - PG0 ：A10 寄存器选择线。
 * - PD/PE：16 位数据总线以及 NOE/NWE 控制线。
 * - PG15：LCD 复位。
 * - PB0 ：LCD 背光。
 *
 * 按键电路说明：
 * - PF8/PF9 空闲为高电平，按下变低电平，因此使用上拉输入 + 下降沿 EXTI。
 * - PF10/PF11 空闲为低电平，按下变高电平，因此使用下拉输入 + 上升沿 EXTI。
 */

static void BSP_LCD_FSMC_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* FSMC 地址线、数据线和控制线配置为复用推挽输出。 */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
                          GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                          GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* LCD 复位和背光使用普通 GPIO 输出控制。 */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    /* KEY-1/KEY-2：低电平有效。 */
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* KEY-3/KEY-4：高电平有效。 */
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void BSP_LCD_FSMC_Init(void)
{
    BSP_LCD_FSMC_GPIO_Init();

    RCC->AHBENR |= RCC_AHBENR_FSMCEN;
    (void)RCC->AHBENR;

    /*
     * Bank1 第 4 区映射到 0x6C000000。
     * LCD 库使用 A10 区分命令地址和数据地址。
     */
    FSMC_Bank1->BTCR[6] = 0x00000000UL;
    FSMC_Bank1->BTCR[7] = (2UL << FSMC_BTRx_ADDSET_Pos) |
                           (30UL << FSMC_BTRx_DATAST_Pos) |
                           (0UL << FSMC_BTRx_BUSTURN_Pos);
    FSMC_Bank1->BTCR[6] = FSMC_BCRx_MBKEN | FSMC_BCRx_MWID_0 | FSMC_BCRx_WREN;
}

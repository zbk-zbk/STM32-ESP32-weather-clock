#include "bsp_buzzer.h"

/*
 * PB1 连接蜂鸣器，使用 TIM3_CH4 输出约 2 kHz PWM。
 * 报警节奏由本模块内部按 200 ms 开/关切换，应用层只需要 Start/Stop。
 */

#define BSP_BUZZER_ALARM_PERIOD_MS 200U
#define BSP_BUZZER_TIM             TIM3
#define BSP_BUZZER_TIM_PSC         71U
#define BSP_BUZZER_TIM_ARR         499U
#define BSP_BUZZER_TIM_DUTY        250U

static uint8_t buzzer_alarm_active = 0U;
static uint8_t buzzer_output_on = 0U;
static uint32_t buzzer_last_toggle_ms = 0U;

static void BspBuzzer_ConfigPwmPin(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = BSP_BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BSP_BUZZER_GPIO_PORT, &GPIO_InitStruct);
}

static void BspBuzzer_SetDuty(uint16_t duty)
{
    if (duty > BSP_BUZZER_TIM_ARR)
    {
        duty = BSP_BUZZER_TIM_ARR;
    }

    BSP_BUZZER_TIM->CCR4 = duty;
}

void BspBuzzer_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    BspBuzzer_ConfigPwmPin();

    BSP_BUZZER_TIM->CR1 = 0U;
    BSP_BUZZER_TIM->PSC = BSP_BUZZER_TIM_PSC;
    BSP_BUZZER_TIM->ARR = BSP_BUZZER_TIM_ARR;
    BSP_BUZZER_TIM->CCR4 = 0U;

    /* TIM3_CH4 PWM1，CCR4 控制占空比；CCR4=0 时静音。 */
    BSP_BUZZER_TIM->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_OC4PE);
    BSP_BUZZER_TIM->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE);
    BSP_BUZZER_TIM->CCER &= ~TIM_CCER_CC4P;
    BSP_BUZZER_TIM->CCER |= TIM_CCER_CC4E;
    BSP_BUZZER_TIM->EGR = TIM_EGR_UG;
    BSP_BUZZER_TIM->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    buzzer_alarm_active = 0U;
    buzzer_output_on = 0U;
    BspBuzzer_Off();
}

void BspBuzzer_On(void)
{
    BspBuzzer_ConfigPwmPin();
    BspBuzzer_SetDuty(BSP_BUZZER_TIM_DUTY);
    buzzer_output_on = 1U;
}

void BspBuzzer_Off(void)
{
    BspBuzzer_SetDuty(0U);
    buzzer_output_on = 0U;
}

void BspBuzzer_StartAlarm(void)
{
    buzzer_alarm_active = 1U;
    buzzer_last_toggle_ms = HAL_GetTick();
    BspBuzzer_On();
}

void BspBuzzer_StopAlarm(void)
{
    buzzer_alarm_active = 0U;
    BspBuzzer_Off();
}

uint8_t BspBuzzer_IsAlarmActive(void)
{
    return buzzer_alarm_active;
}

void BspBuzzer_Task(void)
{
    uint32_t now;

    if (buzzer_alarm_active == 0U)
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - buzzer_last_toggle_ms) >= BSP_BUZZER_ALARM_PERIOD_MS)
    {
        buzzer_last_toggle_ms = now;
        if (buzzer_output_on != 0U)
        {
            BspBuzzer_Off();
        }
        else
        {
            BspBuzzer_On();
        }
    }
}

/* 串口调试用测试音：连续响三次，确认硬件和 PWM 初始化是否正常。 */
void BspBuzzer_TestPattern(void)
{
    uint8_t i;

    buzzer_alarm_active = 0U;
    for (i = 0U; i < 3U; i++)
    {
        BspBuzzer_On();
        HAL_Delay(200U);
        BspBuzzer_Off();
        HAL_Delay(200U);
    }
}

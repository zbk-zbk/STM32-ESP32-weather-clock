#ifndef __BSP_BUZZER_H__
#define __BSP_BUZZER_H__

#include "stm32f1xx_hal.h"

#define BSP_BUZZER_GPIO_PORT GPIOB
#define BSP_BUZZER_GPIO_PIN  GPIO_PIN_1

/* PB1/TIM3_CH4 蜂鸣器驱动接口。 */
void BspBuzzer_Init(void);
void BspBuzzer_On(void);
void BspBuzzer_Off(void);
void BspBuzzer_StartAlarm(void);
void BspBuzzer_StopAlarm(void);
void BspBuzzer_Task(void);
uint8_t BspBuzzer_IsAlarmActive(void);
void BspBuzzer_TestPattern(void);

#endif

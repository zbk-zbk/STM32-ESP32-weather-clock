#ifndef APP_COUNTDOWN_H
#define APP_COUNTDOWN_H

#include "main.h"

/* 倒计时业务状态，LCD 页面只依赖这个状态决定显示和按键行为。 */
typedef enum
{
    APP_COUNTDOWN_NONE = 0,
    APP_COUNTDOWN_RUNNING,
    APP_COUNTDOWN_PAUSED,
    APP_COUNTDOWN_DONE
} AppCountdown_State_t;

void AppCountdown_Init(void);
void AppCountdown_Task(void);
/* seconds 为 0 时等价于取消计时。 */
void AppCountdown_Start(uint32_t seconds);
void AppCountdown_Pause(void);
void AppCountdown_Resume(void);
void AppCountdown_Cancel(void);
void AppCountdown_CloseDone(void);
AppCountdown_State_t AppCountdown_GetState(void);
uint32_t AppCountdown_GetRemainingSeconds(void);
uint32_t AppCountdown_GetInitialSeconds(void);

#endif

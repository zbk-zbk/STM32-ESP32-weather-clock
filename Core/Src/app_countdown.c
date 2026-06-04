#include "app_countdown.h"
#include "bsp_buzzer.h"

/*
 * 倒计时应用层状态机。
 * 只维护计时状态和剩余秒数；蜂鸣器报警由 bsp_buzzer 模块负责。
 */

static AppCountdown_State_t countdown_state = APP_COUNTDOWN_NONE;
static uint32_t countdown_initial_seconds = 0U;
static uint32_t countdown_remaining_seconds = 0U;
static uint32_t countdown_last_tick_ms = 0U;

void AppCountdown_Init(void)
{
    countdown_state = APP_COUNTDOWN_NONE;
    countdown_initial_seconds = 0U;
    countdown_remaining_seconds = 0U;
    countdown_last_tick_ms = HAL_GetTick();
    BspBuzzer_StopAlarm();
}

void AppCountdown_Start(uint32_t seconds)
{
    if (seconds == 0U)
    {
        AppCountdown_Cancel();
        return;
    }

    countdown_initial_seconds = seconds;
    countdown_remaining_seconds = seconds;
    countdown_last_tick_ms = HAL_GetTick();
    countdown_state = APP_COUNTDOWN_RUNNING;
    BspBuzzer_StopAlarm();
}

void AppCountdown_Pause(void)
{
    if (countdown_state == APP_COUNTDOWN_RUNNING)
    {
        countdown_state = APP_COUNTDOWN_PAUSED;
    }
}

void AppCountdown_Resume(void)
{
    if (countdown_state == APP_COUNTDOWN_PAUSED)
    {
        countdown_last_tick_ms = HAL_GetTick();
        countdown_state = APP_COUNTDOWN_RUNNING;
    }
}

void AppCountdown_Cancel(void)
{
    countdown_state = APP_COUNTDOWN_NONE;
    countdown_initial_seconds = 0U;
    countdown_remaining_seconds = 0U;
    countdown_last_tick_ms = HAL_GetTick();
    BspBuzzer_StopAlarm();
}

void AppCountdown_CloseDone(void)
{
    AppCountdown_Cancel();
}

AppCountdown_State_t AppCountdown_GetState(void)
{
    return countdown_state;
}

uint32_t AppCountdown_GetRemainingSeconds(void)
{
    return countdown_remaining_seconds;
}

uint32_t AppCountdown_GetInitialSeconds(void)
{
    return countdown_initial_seconds;
}

void AppCountdown_Task(void)
{
    uint32_t now;
    uint32_t elapsed_seconds;

    if (countdown_state != APP_COUNTDOWN_RUNNING)
    {
        return;
    }

    now = HAL_GetTick();
    elapsed_seconds = (now - countdown_last_tick_ms) / 1000U;
    if (elapsed_seconds == 0U)
    {
        return;
    }

    /* 按整秒扣减，避免主循环抖动导致显示频繁跳变。 */
    countdown_last_tick_ms += elapsed_seconds * 1000U;

    if (elapsed_seconds >= countdown_remaining_seconds)
    {
        countdown_remaining_seconds = 0U;
        countdown_state = APP_COUNTDOWN_DONE;
        /* 状态置为 DONE 后立即启动蜂鸣器，LCD 任务会跳转到完成页。 */
        BspBuzzer_StartAlarm();
    }
    else
    {
        countdown_remaining_seconds -= elapsed_seconds;
    }
}

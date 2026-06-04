#ifndef APP_LCD_CLOCK_H
#define APP_LCD_CLOCK_H

#include "main.h"

/**
  * @brief 初始化 LCD，绘制首屏，并从 RTC 读取编辑初值。
  */
void AppLcdClock_Init(void);

/**
  * @brief 执行 LCD 刷新和按键消抖处理。
  */
void AppLcdClock_Task(void);

/**
  * @brief 处理 LCD 串口诊断命令。
  * @retval 1 命令已处理，0 命令不属于本模块。
  */
uint8_t AppLcdClock_ProcessCommand(const char *command);

#endif /* APP_LCD_CLOCK_H */

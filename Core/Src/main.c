/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_time_config.h"
#include "app_countdown.h"
#include "app_debug_uart.h"
#include "app_datetime.h"
#include "app_lcd_clock.h"
#include "app_weather.h"
#include "app_wifi.h"
#include "bsp_buzzer.h"
#include "bsp_esp_at.h"
#include "bsp_lcd_fsmc.h"
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_RTC_MAGIC               0xA55AU
#define APP_SECONDS_PER_DAY         86400UL
#define APP_FULL_TIME_COMMAND_LEN   19U
#define APP_RESET_RTC_COMMAND       "RESET_RTC"
#define APP_RESET_RTC_COMMAND_LEN   9U
#define APP_ESP_AT_COMMAND          "ESP_AT"
#define APP_ESP_AT_RESET_COMMAND    "ESP_AT_RESET"
#define APP_WIFI_JOIN_COMMAND       "WIFI_JOIN"
#define APP_WIFI_STATE_COMMAND      "WIFI_STATE"
#define APP_WEATHER_RAW_COMMAND     "WEATHER_RAW"
#define APP_WEATHER_GET_COMMAND     "WEATHER_GET"
#define APP_BUZZER_TEST_COMMAND     "BUZZER_TEST"

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t print_tick_ms = 0;
static uint8_t rx_byte = 0;
static char rx_line[32];
static volatile uint8_t rx_index = 0;
static volatile uint8_t rx_line_ready = 0;
static char esp_at_response[512];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t App_RtcInit(void);
static void App_RtcSetDateTime(const DateTime_t *dt);
static void App_RtcGetDateTime(DateTime_t *dt);
static uint32_t App_RtcReadCounter(void);
static void App_RtcWriteCounter(uint32_t seconds);
static void App_RtcWritePrescaler(uint32_t prescaler);
static uint8_t App_RtcWaitReady(void);
static void App_RtcEnterConfig(void);
static void App_RtcExitConfig(void);
static void App_RtcResetBackupDomainAndReboot(void);
static void App_PrintDateTimeAndCat(void);
static void App_ProcessRxLine(void);
static void App_RunEspAtSmokeTest(uint8_t reset_first);
static void App_SetDefaultTime(DateTime_t *dt);
static uint8_t App_ParseDateTime(const char *text, DateTime_t *dt);
static uint8_t App_IsValidDateTime(const DateTime_t *dt);
static uint32_t App_DateTimeToSeconds(const DateTime_t *dt);
static void App_SecondsToDateTime(uint32_t seconds, DateTime_t *dt);
static uint8_t App_DaysInMonth(uint16_t year, uint8_t month);
static uint8_t App_IsLeapYear(uint16_t year);
static void App_UART_SendString(const char *text);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (!rx_line_ready)
        {
            if (rx_byte == '\r' || rx_byte == '\n')
            {
                if (rx_index > 0U)
                {
                    rx_line[rx_index] = '\0';
                    rx_line_ready = 1U;
                }
            }
            else if (rx_index < (sizeof(rx_line) - 1U))
            {
                rx_line[rx_index++] = (char)rx_byte;
                rx_line[rx_index] = '\0';

                if ((rx_index >= APP_RESET_RTC_COMMAND_LEN) &&
                    (strncmp(rx_line, APP_RESET_RTC_COMMAND, APP_RESET_RTC_COMMAND_LEN) == 0))
                {
                    rx_line_ready = 1U;
                }
                else if (rx_index >= APP_FULL_TIME_COMMAND_LEN)
                {
                    rx_line_ready = 1U;
                }
            }
            else
            {
                rx_index = 0U;
            }
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    /* USER CODE BEGIN 2 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    App_UART_SendString("boot usart ready\r\n");

    BspEspAt_Init();
    BspBuzzer_Init();
    AppCountdown_Init();
    AppWifi_Init();
    BSP_LCD_FSMC_Init();

    if (!App_RtcInit())
    {
        App_UART_SendString("rtc init failed\r\n");
        Error_Handler();
    }

    AppLcdClock_Init();

    print_tick_ms = HAL_GetTick();
    App_UART_SendString("RTC datetime demo ready.\r\n");
    App_UART_SendString("Set RTC: YYYY-MM-DD-HH:MM:SS\r\n");

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        App_ProcessRxLine();
        AppCountdown_Task();
        BspBuzzer_Task();
        AppLcdClock_Task();

        if ((HAL_GetTick() - print_tick_ms) >= 5000U)
        {
            print_tick_ms = HAL_GetTick();
            App_PrintDateTimeAndCat();
        }
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
/**
  * @brief 解析并分发一条完整的串口命令。
  *
  * 支持 RTC 设置、ESP32 AT 自检、WiFi 入网、天气获取、蜂鸣器测试以及 LCD 诊断命令。
  */
static void App_ProcessRxLine(void)
{
    char line[sizeof(rx_line)];
    DateTime_t new_time;

    if (!rx_line_ready)
    {
        return;
    }

    __disable_irq();
    strncpy(line, rx_line, sizeof(line));
    line[sizeof(line) - 1U] = '\0';
    rx_index = 0U;
    rx_line_ready = 0U;
    __enable_irq();

    if (strcmp(line, APP_RESET_RTC_COMMAND) == 0)
    {
        App_UART_SendString("reset rtc backup domain...\r\n");
        HAL_Delay(50U);
        App_RtcResetBackupDomainAndReboot();
    }
    else if (App_ParseDateTime(line, &new_time))
    {
        App_RtcSetDateTime(&new_time);
        print_tick_ms = HAL_GetTick();
        App_UART_SendString("rtc updated: ");
        App_PrintDateTimeAndCat();
    }
    else if (strcmp(line, APP_ESP_AT_COMMAND) == 0)
    {
        App_RunEspAtSmokeTest(0U);
    }
    else if (strcmp(line, APP_ESP_AT_RESET_COMMAND) == 0)
    {
        App_RunEspAtSmokeTest(1U);
    }
    else if (strcmp(line, APP_WIFI_JOIN_COMMAND) == 0)
    {
        (void)AppWifi_Join();
    }
    else if (strcmp(line, APP_WIFI_STATE_COMMAND) == 0)
    {
        AppWifi_PrintState();
    }
    else if (strcmp(line, APP_WEATHER_RAW_COMMAND) == 0)
    {
        (void)AppWeather_FetchRaw();
    }
    else if (strcmp(line, APP_WEATHER_GET_COMMAND) == 0)
    {
        (void)AppWeather_Update();
    }
    else if (strcmp(line, APP_BUZZER_TEST_COMMAND) == 0)
    {
        App_UART_SendString("[BUZZER] test pattern start\r\n");
        BspBuzzer_TestPattern();
        App_UART_SendString("[BUZZER] test pattern done\r\n");
    }
    else if (AppLcdClock_ProcessCommand(line))
    {
        /* Handled by LCD module. */
    }
    else
    {
        App_UART_SendString("invalid format, use YYYY-MM-DD-HH:MM:SS or ESP_AT/ESP_AT_RESET/WIFI_JOIN/WIFI_STATE/WEATHER_RAW/WEATHER_GET/BUZZER_TEST/LCD_ID/LCD_TEST/LCD_HOME/LCD_DATE/LCD_TIME/LCD_WEATHER/LCD_DETAIL/LCD_STATUS/LCD_TIMER_SET/LCD_TIMER_STATUS/TIMER_5S/TIMER_CANCEL/KEY_STATE\r\n");
    }
}

/**
  * @brief 对 ESP32-C3 AT 固件做最小连通性测试。
  *
  * 依次发送 AT、ATE0 和 AT+GMR。若 AT 三次都没有返回 OK，说明 USART2、ESP32 EN、波特率或 AT 固件串口映射至少有一项需要排查。
  */
static void App_RunEspAtSmokeTest(uint8_t reset_first)
{
    int ret;
    uint8_t attempt;

    App_UART_SendString("\r\n[ESP] AT smoke test start\r\n");

    if (reset_first)
    {
        App_UART_SendString("[ESP] reset by PE4...\r\n");
        BspEspAt_Reset();
    }

    for (attempt = 1U; attempt <= 3U; attempt++)
    {
        ret = BspEspAt_SendCmd("AT\r\n", "OK", esp_at_response, sizeof(esp_at_response), 1000U);
        App_UART_SendString("[ESP] AT attempt ");
        App_UART_SendString((attempt == 1U) ? "1" : ((attempt == 2U) ? "2" : "3"));
        App_UART_SendString(": ");

        if (ret == BSP_ESP_AT_OK)
        {
            App_UART_SendString("OK\r\n");
            App_UART_SendString(esp_at_response);
            if (strstr(esp_at_response, "\r\n") == NULL)
            {
                App_UART_SendString("\r\n");
            }
            break;
        }

        App_UART_SendString("failed\r\n");
        if (esp_at_response[0] != '\0')
        {
            App_UART_SendString(esp_at_response);
            App_UART_SendString("\r\n");
        }
        HAL_Delay(200U);
    }

    if (ret != BSP_ESP_AT_OK)
    {
        App_UART_SendString("[ESP] AT failed after 3 attempts. Check GPIO6/GPIO7 AT mapping, baudrate, PE4 EN, wiring and GND.\r\n");
        return;
    }

    ret = BspEspAt_SendCmd("ATE0\r\n", "OK", esp_at_response, sizeof(esp_at_response), 1000U);
    App_UART_SendString("[ESP] ATE0: ");
    App_UART_SendString((ret == BSP_ESP_AT_OK) ? "OK\r\n" : "failed\r\n");
    if (esp_at_response[0] != '\0')
    {
        App_UART_SendString(esp_at_response);
        App_UART_SendString("\r\n");
    }

    ret = BspEspAt_SendCmd("AT+GMR\r\n", "OK", esp_at_response, sizeof(esp_at_response), 2000U);
    App_UART_SendString("[ESP] AT+GMR: ");
    App_UART_SendString((ret == BSP_ESP_AT_OK) ? "OK\r\n" : "failed\r\n");
    if (esp_at_response[0] != '\0')
    {
        App_UART_SendString(esp_at_response);
        App_UART_SendString("\r\n");
    }

    App_UART_SendString("[ESP] AT smoke test done\r\n");
}

/**
  * @brief 初始化 STM32F1 备份域 RTC。
  *
  * RTC 初始化完成后，BKP->DR1 保存 APP_RTC_MAGIC。标记存在且 APP_FORCE_RTC_INIT 为 0 时，复位或重新烧录不会覆盖备用电池维持的 RTC 计数器。
  */
static uint8_t App_RtcInit(void)
{
    DateTime_t default_time;
    uint32_t timeout;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    if ((BKP->DR1 != APP_RTC_MAGIC) || (APP_FORCE_RTC_INIT != 0U))
    {
        RCC->BDCR |= RCC_BDCR_BDRST;
        RCC->BDCR &= ~RCC_BDCR_BDRST;
    }

    RCC->BDCR |= RCC_BDCR_LSEON;
    timeout = HAL_GetTick();
    while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0U)
    {
        if ((HAL_GetTick() - timeout) > LSE_STARTUP_TIMEOUT)
        {
            return 0U;
        }
    }

    if ((RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_BDCR_RTCSEL_LSE)
    {
        RCC->BDCR &= ~RCC_BDCR_RTCSEL;
        RCC->BDCR |= RCC_BDCR_RTCSEL_LSE;
    }
    RCC->BDCR |= RCC_BDCR_RTCEN;

    if (!App_RtcWaitReady())
    {
        return 0U;
    }
    RTC->CRL &= (uint16_t)~RTC_CRL_RSF;
    timeout = HAL_GetTick();
    while ((RTC->CRL & RTC_CRL_RSF) == 0U)
    {
        if ((HAL_GetTick() - timeout) > 1000U)
        {
            return 0U;
        }
    }

    if ((BKP->DR1 != APP_RTC_MAGIC) || (APP_FORCE_RTC_INIT != 0U))
    {
        App_SetDefaultTime(&default_time);
        App_RtcWritePrescaler(32767UL);
        App_RtcWriteCounter(App_DateTimeToSeconds(&default_time));
        BKP->DR1 = APP_RTC_MAGIC;
    }

    return 1U;
}

/**
  * @brief 将日历时间写入 RTC 计数器。
  */
static void App_RtcSetDateTime(const DateTime_t *dt)
{
    App_RtcWriteCounter(App_DateTimeToSeconds(dt));
    BKP->DR1 = APP_RTC_MAGIC;
}

/**
  * @brief 提供给其他模块调用的 RTC 写入接口。
  */
void App_SetRtcDateTime(const DateTime_t *dt)
{
    App_RtcSetDateTime(dt);
}

/**
  * @brief 读取 RTC 计数器并转换为日历字段。
  */
static void App_RtcGetDateTime(DateTime_t *dt)
{
    App_SecondsToDateTime(App_RtcReadCounter(), dt);
}

/**
  * @brief 提供给其他模块调用的 RTC 读取接口。
  */
void App_GetRtcDateTime(DateTime_t *dt)
{
    App_RtcGetDateTime(dt);
}

/**
  * @brief 安全读取 STM32F1 的 32 位 RTC 计数器。
  *
  * CNTH/CNTL 是两个 16 位寄存器。这里采用“高-低-高”的读取方式；如果读取过程中高 16 位发生变化，则重新读取。
  */
static uint32_t App_RtcReadCounter(void)
{
    uint16_t high1;
    uint16_t high2;
    uint16_t low;

    do
    {
        high1 = (uint16_t)(RTC->CNTH & RTC_CNTH_RTC_CNT);
        low = (uint16_t)(RTC->CNTL & RTC_CNTL_RTC_CNT);
        high2 = (uint16_t)(RTC->CNTH & RTC_CNTH_RTC_CNT);
    } while (high1 != high2);

    return (((uint32_t)high1) << 16U) | low;
}

/**
  * @brief 将从 1970-01-01 起算的秒数写入 RTC 计数器。
  */
static void App_RtcWriteCounter(uint32_t seconds)
{
    if (!App_RtcWaitReady())
    {
        return;
    }
    App_RtcEnterConfig();
    RTC->CNTH = (uint16_t)(seconds >> 16U);
    RTC->CNTL = (uint16_t)(seconds & 0xFFFFU);
    App_RtcExitConfig();
}

/**
  * @brief 配置 RTC 预分频，使 32.768 kHz LSE 产生 1 Hz 计数节拍。
  */
static void App_RtcWritePrescaler(uint32_t prescaler)
{
    if (!App_RtcWaitReady())
    {
        return;
    }
    App_RtcEnterConfig();
    RTC->PRLH = (uint16_t)(prescaler >> 16U);
    RTC->PRLL = (uint16_t)(prescaler & 0xFFFFU);
    App_RtcExitConfig();
}

/**
  * @brief 等待 RTC 寄存器可访问。
  */
static uint8_t App_RtcWaitReady(void)
{
    uint32_t timeout = HAL_GetTick();

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U)
    {
        if ((HAL_GetTick() - timeout) > 1000U)
        {
            return 0U;
        }
    }
    return 1U;
}

static void App_RtcEnterConfig(void)
{
    RTC->CRL |= RTC_CRL_CNF;
}

static void App_RtcExitConfig(void)
{
    RTC->CRL &= (uint16_t)~RTC_CRL_CNF;
    (void)App_RtcWaitReady();
}

/**
  * @brief 清除备份域并复位系统，使 RTC 在下次启动时重新初始化。
  */
static void App_RtcResetBackupDomainAndReboot(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;

    HAL_Delay(20U);
    NVIC_SystemReset();
}

/**
  * @brief 从编译时间生成默认 RTC 时间。
  */
static void App_SetDefaultTime(DateTime_t *dt)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char build_month[4] = {0};
    uint8_t month = 1U;
    int day = 1;
    int year = 2026;
    int hour = 0;
    int minute = 0;
    int second = 0;

#if (APP_USE_BUILD_TIME_WHEN_RTC_EMPTY != 0U)
    (void)sscanf(__DATE__, "%3s %d %d", build_month, &day, &year);
    (void)sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    for (uint8_t i = 0U; i < 12U; i++)
    {
        if (strncmp(build_month, &months[i * 3U], 3U) == 0)
        {
            month = (uint8_t)(i + 1U);
            break;
        }
    }
#endif

    dt->year = (uint16_t)year;
    dt->month = month;
    dt->day = (uint8_t)day;
    dt->hour = (uint8_t)hour;
    dt->minute = (uint8_t)minute;
    dt->second = (uint8_t)second;
}

/**
  * @brief 解析串口支持的日期时间格式，并校验结果是否合法。
  */
static uint8_t App_ParseDateTime(const char *text, DateTime_t *dt)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    if ((sscanf(text, "%d-%d-%d-%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) &&
        (sscanf(text, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6))
    {
        return 0U;
    }

    dt->year = (uint16_t)year;
    dt->month = (uint8_t)month;
    dt->day = (uint8_t)day;
    dt->hour = (uint8_t)hour;
    dt->minute = (uint8_t)minute;
    dt->second = (uint8_t)second;

    return App_IsValidDateTime(dt);
}

/**
  * @brief 校验本项目允许的日期时间范围。
  */
static uint8_t App_IsValidDateTime(const DateTime_t *dt)
{
    if (dt->year < 2000U || dt->year > 2099U)
    {
        return 0U;
    }
    if (dt->month < 1U || dt->month > 12U)
    {
        return 0U;
    }
    if (dt->day < 1U || dt->day > App_DaysInMonth(dt->year, dt->month))
    {
        return 0U;
    }
    if (dt->hour > 23U || dt->minute > 59U || dt->second > 59U)
    {
        return 0U;
    }
    return 1U;
}

/**
  * @brief 将日历字段转换为从 1970-01-01 起算的秒数。
  */
static uint32_t App_DateTimeToSeconds(const DateTime_t *dt)
{
    uint32_t days = 0U;

    for (uint16_t year = 1970U; year < dt->year; year++)
    {
        days += App_IsLeapYear(year) ? 366UL : 365UL;
    }

    for (uint8_t month = 1U; month < dt->month; month++)
    {
        days += App_DaysInMonth(dt->year, month);
    }

    days += (uint32_t)(dt->day - 1U);

    return (days * APP_SECONDS_PER_DAY) +
           ((uint32_t)dt->hour * 3600UL) +
           ((uint32_t)dt->minute * 60UL) +
           dt->second;
}

/**
  * @brief 将从 1970-01-01 起算的秒数转换为日历字段。
  */
static void App_SecondsToDateTime(uint32_t seconds, DateTime_t *dt)
{
    uint32_t days = seconds / APP_SECONDS_PER_DAY;
    uint32_t day_seconds = seconds % APP_SECONDS_PER_DAY;
    uint16_t year = 1970U;
    uint8_t month = 1U;
    uint16_t year_days;
    uint8_t month_days;

    while (1)
    {
        year_days = App_IsLeapYear(year) ? 366U : 365U;
        if (days < year_days)
        {
            break;
        }
        days -= year_days;
        year++;
    }

    while (1)
    {
        month_days = App_DaysInMonth(year, month);
        if (days < month_days)
        {
            break;
        }
        days -= month_days;
        month++;
    }

    dt->year = year;
    dt->month = month;
    dt->day = (uint8_t)(days + 1U);
    dt->hour = (uint8_t)(day_seconds / 3600UL);
    dt->minute = (uint8_t)((day_seconds % 3600UL) / 60UL);
    dt->second = (uint8_t)(day_seconds % 60UL);
}

static uint8_t App_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if (month == 2U && App_IsLeapYear(year))
    {
        return 29U;
    }
    return days[month - 1U];
}

static uint8_t App_IsLeapYear(uint16_t year)
{
    return ((year % 4U) == 0U && (((year % 100U) != 0U) || ((year % 400U) == 0U)));
}

/**
  * @brief 通过串口打印当前 RTC 日期时间和 ASCII 图案。
  */
static void App_PrintDateTimeAndCat(void)
{
    char text[128];
    DateTime_t current_time;

    App_RtcGetDateTime(&current_time);

    int len = snprintf(text,
                       sizeof(text),
                       "%04u-%02u-%02u %02u:%02u:%02u\r\n"
                       " /\\_/\\\\\r\n"
                       "( o.o )\r\n"
                       " > ^ <\r\n",
                       current_time.year,
                       current_time.month,
                       current_time.day,
                       current_time.hour,
                       current_time.minute,
                       current_time.second);

    if (len > 0)
    {
        AppDebugUart_SendString(text);
    }
}

/**
  * @brief 通过 USART1 发送一个以 '\0' 结尾的字符串。
  */
static void App_UART_SendString(const char *text)
{
    AppDebugUart_SendString(text);
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    AppDebugUart_SendString("error");
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


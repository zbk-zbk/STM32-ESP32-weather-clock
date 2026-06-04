#include "app_lcd_clock.h"

#include "Inf_LCD.h"
#include "app_debug_uart.h"
#include "app_countdown.h"
#include "app_countdown_font.h"
#include "app_countdown_font16.h"
#include "app_datetime.h"
#include "app_weather.h"
#include "app_weather_wind_font.h"
#include "app_wifi.h"
#include "bsp_esp_at.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/*
 * LCD 菜单应用层。
 * 本模块负责：
 * - LCD 多页面显示；
 * - 日期和时间编辑状态管理；
 * - 天气页面显示与手动刷新；
 * - 倒计时页面、状态和按键交互；
 * - EXTI 按键事件缓存和消抖处理。
 *
 * EXTI 回调里只记录事件，不直接刷屏、联网或写 RTC。
 * 真正动作都在 AppLcdClock_Task() 中执行，保证中断足够短。
 */
typedef enum
{
    LCD_PAGE_HOME = 0,
    LCD_PAGE_DATE_EDIT,
    LCD_PAGE_TIME_EDIT,
    LCD_PAGE_WEATHER,
    LCD_PAGE_WEATHER_DETAIL,
    LCD_PAGE_STATUS,
    LCD_PAGE_TIMER_STATUS,
    LCD_PAGE_TIMER_SET,
    LCD_PAGE_TIMER_CONFIRM,
    LCD_PAGE_COUNT
} LcdPage_t;

typedef enum
{
    LCD_EDIT_YEAR = 0,
    LCD_EDIT_MONTH,
    LCD_EDIT_DAY,
    LCD_EDIT_HOUR,
    LCD_EDIT_MINUTE,
    LCD_EDIT_SECOND
} LcdEditField_t;

#define LCD_KEY_SELECT_PIN   GPIO_PIN_8
#define LCD_KEY_INC_PIN      GPIO_PIN_9
#define LCD_KEY_DEC_PIN      GPIO_PIN_10
#define LCD_KEY_SAVE_PIN     GPIO_PIN_11
#define LCD_KEY_DEBOUNCE_MS  25U
#define LCD_KEY_GUARD_MS     180U
#define LCD_HOME_WEATHER_REFRESH_MS 900000U
#define LCD_HOME_WEATHER_FIRST_REFRESH_MS 5000U
#define LCD_HOME_WIFI_CHECK_MS 10000U
#define LCD_CN_LEN(text) ((uint8_t)(sizeof(text) / sizeof((text)[0])))

typedef enum
{
    CN_ZHU = 0,
    CN_YE = 1,
    CN_RI = 2,
    CN_QI = 3,
    CN_SHE = 4,
    CN_ZHI = 5,
    CN_SHI = 6,
    CN_JIAN = 7,
    CN_XUAN = 10,
    CN_XIANG = 11,
    CN_NIAN = 12,
    CN_YUE = 13,
    CN_FEN = 16,
    CN_MIAO = 17,
    CN_BAO = 18,
    CN_CUN = 19,
    CN_SHUA = 20,
    CN_XIN = 21,
    CN_FAN = 22,
    CN_HUI = 23,
    CN_YI = 24,
    CN_LIAN = 25,
    CN_JIE = 26,
    CN_WEI = 27,
    CN_TIAN = 30,
    CN_TIAN_QI = 31,
    CN_XIANG_QING = 32,
    CN_QING = 33,
    CN_ZHUANG = 34,
    CN_TAI = 35,
    CN_WU = 36,
    CN_SHU = 37,
    CN_JU = 38,
    CN_XU = 39,
    CN_YAO = 40,
    CN_WANG = 41,
    CN_LUO = 42,
    CN_WEN = 43,
    CN_DU = 44,
    CN_TI = 45,
    CN_GAN = 46,
    CN_SHI_WET = 47,
    CN_JIANG = 49,
    CN_SHUI = 50,
    CN_FENG = 51,
    CN_SU = 52,
    CN_XIANG_DIR = 54,
    CN_JIU = 56,
    CN_XU_READY = 57,
    CN_CUO = 58,
    CN_WU_ERR = 59,
    CN_KONG = 60,
    CN_XIAN = 61,
    CN_YOU = 62,
    CN_XIAO = 63,
    CN_WEI_UNKNOWN = 64,
    CN_ZHI_KNOW = 65,
    CN_QING_CLEAR = 66,
    CN_DUO = 67,
    CN_YUN = 68,
    CN_WU_FOG = 69,
    CN_MAO = 70,
    CN_YU = 72,
    CN_XUE = 73,
    CN_LEI = 74,
    CN_XIA = 76,
    CN_YI_ONE = 77,
    CN_YE_PAGE = 78,
    CN_QING_DETAIL = 79,
    CN16_XIU = 128,
    CN16_GAI = 129,
    CN16_RI = 130,
    CN16_QI_DATE = 131,
    CN16_SHI_TIME = 132,
    CN16_JIAN_TIME = 133,
    CN16_SHUA = 134,
    CN16_XIN = 135,
    CN16_TIAN = 136,
    CN16_QI_WEATHER = 137,
    CN16_XIAN_SHOW = 138,
    CN16_SHI_SHOW = 139,
    CN16_DEG_C = 140
} LcdCnIndex_t;

typedef enum
{
    CD32_JI = 0,
    CD32_SHI = 1,
    CD32_QI_TIMER = 2,
    CD32_WU = 3,
    CD32_ZHONG = 4,
    CD32_WAN = 5,
    CD32_CHENG = 6,
    CD32_ZHUANG = 7,
    CD32_TAI = 8,
    CD32_SHENG = 9,
    CD32_YU = 10,
    CD32_SHE = 11,
    CD32_ZHI = 12,
    CD32_QUE_RECOGNIZE = 13,
    CD32_REN = 14,
    CD32_TING_STOP = 15,
    CD32_ZHI_STOP = 16,
    CD32_JI_CONTINUE = 17,
    CD32_XU = 18,
    CD32_QU = 19,
    CD32_XIAO = 20,
    CD32_KAI = 21,
    CD32_SHI_START = 22,
    CD32_FAN = 23,
    CD32_HUI = 24,
    CD32_FENG = 25,
    CD32_MING = 26,
    CD32_BAO_ALARM = 27,
    CD32_JING = 28,
    CD32_GUAN = 29,
    CD32_BI = 30,
    CD32_YI = 31,
    CD32_ZAN = 32,
    CD32_TING_PAUSE = 33,
    CD32_QUE_CONFIRM = 34,
    CD32_DING = 35,
    CD32_YAO = 36,
    CD32_XUAN = 37,
    CD32_XIANG = 38,
    CD32_FEN = 39,
    CD32_MIAO = 40,
    CD32_ZHU = 41,
    CD32_YE = 42,
    CD32_RI = 43,
    CD32_QI_DATE = 44,
    CD32_WANG = 45,
    CD32_LUO = 46,
    CD32_LIAN = 47,
    CD32_JIE = 48,
    CD32_TIAN = 49,
    CD32_QI_WEATHER = 50,
    CD32_XIAN = 51,
    CD32_SHI_SHOW = 52,
    CD32_XIU = 53,
    CD32_GAI = 54,
    CD32_BAO_SAVE = 55,
    CD32_CUN = 56,
    CD32_SHUA = 57,
    CD32_XIN = 58
} CountdownCn32Index_t;
static DateTime_t edit_time;
static LcdPage_t current_page = LCD_PAGE_HOME;
static LcdEditField_t edit_field = LCD_EDIT_YEAR;
static uint32_t last_display_ms = 0;
static uint32_t last_key_ms = 0;
static uint32_t last_home_weather_refresh_ms = 0;
static uint32_t last_home_wifi_check_ms = 0;
static uint8_t home_wifi_connected = 0U;
static uint8_t lcd_need_full_redraw = 1U;
static uint8_t edit_dirty = 0U;
static uint8_t home_static_area_cleared = 0U;
static uint8_t lcd_initialized = 0U;
static uint8_t lcd_wait_hook_active = 0U;
static AppCountdown_State_t last_countdown_state = APP_COUNTDOWN_NONE;

static uint8_t timer_set_hour = 0U;
static uint8_t timer_set_minute = 0U;
static uint8_t timer_set_second = 0U;
static LcdEditField_t timer_edit_field = LCD_EDIT_HOUR;

/* 在 EXTI 中断上下文中置位，在主循环中读取并清除。 */
static volatile uint16_t key_pending_pins = 0;
static volatile uint32_t key_last_edge_ms = 0;
static volatile uint32_t key_irq_counts[4] = {0};

static uint8_t LcdClock_IsLeapYear(uint16_t year)
{
    return ((year % 4U) == 0U && (((year % 100U) != 0U) || ((year % 400U) == 0U)));
}

static uint8_t LcdClock_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if (month == 2U && LcdClock_IsLeapYear(year))
    {
        return 29U;
    }
    return days[month - 1U];
}

static int8_t LcdClock_KeyIndex(uint16_t pin)
{
    switch (pin)
    {
    case LCD_KEY_SELECT_PIN:
        return 0;
    case LCD_KEY_INC_PIN:
        return 1;
    case LCD_KEY_DEC_PIN:
        return 2;
    case LCD_KEY_SAVE_PIN:
        return 3;
    default:
        return -1;
    }
}

static const char *LcdClock_KeyName(uint16_t pin)
{
    switch (pin)
    {
    case LCD_KEY_SELECT_PIN:
        return "SW3(PF8)";
    case LCD_KEY_INC_PIN:
        return "SW4(PF9)";
    case LCD_KEY_DEC_PIN:
        return "SW5(PF10)";
    case LCD_KEY_SAVE_PIN:
        return "SW6(PF11)";
    default:
        return "unknown";
    }
}

static const char *LcdClock_PageName(void)
{
    switch (current_page)
    {
    case LCD_PAGE_HOME:
        return "HOME";
    case LCD_PAGE_DATE_EDIT:
        return "DATE EDIT";
    case LCD_PAGE_TIME_EDIT:
        return "TIME EDIT";
    case LCD_PAGE_WEATHER:
        return "WEATHER";
    case LCD_PAGE_WEATHER_DETAIL:
        return "WEATHER DETAIL";
    case LCD_PAGE_STATUS:
        return "STATUS";
    case LCD_PAGE_TIMER_STATUS:
        return "TIMER STATUS";
    case LCD_PAGE_TIMER_SET:
        return "TIMER SET";
    case LCD_PAGE_TIMER_CONFIRM:
        return "TIMER CONFIRM";
    default:
        return "UNKNOWN";
    }
}

static const char *LcdClock_EditFieldName(void)
{
    switch (edit_field)
    {
    case LCD_EDIT_YEAR:
        return "YEAR";
    case LCD_EDIT_MONTH:
        return "MONTH";
    case LCD_EDIT_DAY:
        return "DAY";
    case LCD_EDIT_HOUR:
        return "HOUR";
    case LCD_EDIT_MINUTE:
        return "MIN";
    case LCD_EDIT_SECOND:
        return "SEC";
    default:
        return "---";
    }
}

static void LcdClock_SendDiag(const char *text)
{
    AppDebugUart_SendString(text);
}

static void LcdClock_LimitDate(DateTime_t *dt)
{
    uint8_t max_day;

    if (dt->year < 2000U)
    {
        dt->year = 2000U;
    }
    if (dt->year > 2099U)
    {
        dt->year = 2099U;
    }
    if (dt->month < 1U)
    {
        dt->month = 1U;
    }
    if (dt->month > 12U)
    {
        dt->month = 12U;
    }

    max_day = LcdClock_DaysInMonth(dt->year, dt->month);
    if (dt->day < 1U)
    {
        dt->day = 1U;
    }
    if (dt->day > max_day)
    {
        dt->day = max_day;
    }
}

static uint8_t LcdClock_AdjustWrapU8(uint8_t value, uint8_t min, uint8_t max, int8_t step)
{
    int16_t next = (int16_t)value + step;

    if (next < min)
    {
        next = max;
    }
    if (next > max)
    {
        next = min;
    }
    return (uint8_t)next;
}

static uint16_t LcdClock_AdjustWrapU16(uint16_t value, uint16_t min, uint16_t max, int8_t step)
{
    int32_t next = (int32_t)value + step;

    if (next < min)
    {
        next = max;
    }
    if (next > max)
    {
        next = min;
    }
    return (uint16_t)next;
}

static void LcdClock_AdjustSelected(int8_t step)
{
    switch (edit_field)
    {
    case LCD_EDIT_YEAR:
        edit_time.year = LcdClock_AdjustWrapU16(edit_time.year, 2000U, 2099U, step);
        break;
    case LCD_EDIT_MONTH:
        edit_time.month = LcdClock_AdjustWrapU8(edit_time.month, 1U, 12U, step);
        break;
    case LCD_EDIT_DAY:
        edit_time.day = LcdClock_AdjustWrapU8(edit_time.day, 1U, LcdClock_DaysInMonth(edit_time.year, edit_time.month), step);
        break;
    case LCD_EDIT_HOUR:
        edit_time.hour = LcdClock_AdjustWrapU8(edit_time.hour, 0U, 23U, step);
        break;
    case LCD_EDIT_MINUTE:
        edit_time.minute = LcdClock_AdjustWrapU8(edit_time.minute, 0U, 59U, step);
        break;
    case LCD_EDIT_SECOND:
        edit_time.second = LcdClock_AdjustWrapU8(edit_time.second, 0U, 59U, step);
        break;
    default:
        break;
    }

    LcdClock_LimitDate(&edit_time);
    edit_dirty = 1U;
}

static void LcdClock_DrawCat(void)
{
    const uint16_t c = BLUE;
    const uint16_t fill = WHITE;

    Inf_LCD_DrawLine(95, 330, 125, 290, 3, c);
    Inf_LCD_DrawLine(125, 290, 150, 328, 3, c);
    Inf_LCD_DrawLine(170, 328, 195, 290, 3, c);
    Inf_LCD_DrawLine(195, 290, 225, 330, 3, c);
    Inf_LCD_DrawCircle_1(160, 365, 70, 3, c);
    Inf_LCD_DrawCircleFill_1(135, 355, 6, 2, c, c);
    Inf_LCD_DrawCircleFill_1(185, 355, 6, 2, c, c);
    Inf_LCD_DrawLine(155, 375, 165, 375, 2, c);
    Inf_LCD_DrawLine(160, 375, 150, 388, 2, c);
    Inf_LCD_DrawLine(160, 375, 170, 388, 2, c);
    Inf_LCD_DrawLine(60, 365, 115, 380, 2, c);
    Inf_LCD_DrawLine(60, 395, 115, 395, 2, c);
    Inf_LCD_DrawLine(205, 380, 260, 365, 2, c);
    Inf_LCD_DrawLine(205, 395, 260, 395, 2, c);
    Inf_LCD_DrawCircleFill_1(160, 435, 10, 2, fill, fill);
}

static void LcdClock_PrintId(void)
{
    uint32_t id = Inf_LCD_ReadId();
    char diag[32];

    snprintf(diag, sizeof(diag), "lcd id: 0x%06lX\r\n", (unsigned long)id);
    LcdClock_SendDiag(diag);
}

static void LcdClock_ShowColorProbe(void)
{
    Inf_LCD_ClearAll(RED);
    HAL_Delay(120);
    Inf_LCD_ClearAll(GREEN);
    HAL_Delay(120);
    Inf_LCD_ClearAll(BLUE);
    HAL_Delay(120);
    Inf_LCD_ClearAll(WHITE);
}

static void LcdClock_PrintKeyState(void)
{
    uint32_t counts[4];
    char diag[96];

    __disable_irq();
    counts[0] = key_irq_counts[0];
    counts[1] = key_irq_counts[1];
    counts[2] = key_irq_counts[2];
    counts[3] = key_irq_counts[3];
    __enable_irq();

    snprintf(diag, sizeof(diag), "key raw PF8=%u PF9=%u PF10=%u PF11=%u\r\n",
             (unsigned int)HAL_GPIO_ReadPin(GPIOF, LCD_KEY_SELECT_PIN),
             (unsigned int)HAL_GPIO_ReadPin(GPIOF, LCD_KEY_INC_PIN),
             (unsigned int)HAL_GPIO_ReadPin(GPIOF, LCD_KEY_DEC_PIN),
             (unsigned int)HAL_GPIO_ReadPin(GPIOF, LCD_KEY_SAVE_PIN));
    LcdClock_SendDiag(diag);

    snprintf(diag, sizeof(diag), "key irq PF8=%lu PF9=%lu PF10=%lu PF11=%lu\r\n",
             (unsigned long)counts[0],
             (unsigned long)counts[1],
             (unsigned long)counts[2],
             (unsigned long)counts[3]);
    LcdClock_SendDiag(diag);
}

static void LcdClock_WriteLine(uint16_t y, const char *text, uint16_t color)
{
    Inf_LCD_WriteAsciiString(16, y, 24, (uint8_t *)text, color, WHITE);
}

static void LcdClock_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t count = (uint32_t)w * (uint32_t)h;

    Inf_LCD_SetArea((int16_t)x, y, w, h);
    while (count > 0U)
    {
        Inf_LCD_WriteData(color);
        count--;
    }
}

static uint16_t LcdClock_WriteCnText(uint16_t x, uint16_t y, const uint8_t *text, uint8_t len, uint16_t color, uint16_t bg)
{
    uint8_t i;

    for (i = 0U; i < len; i++)
    {
        Inf_LCD_WriteChineseChar(x, y, text[i], color, bg);
        x = (uint16_t)(x + 32U);
    }

    return x;
}

static void LcdClock_WriteCnLine(uint16_t y, const uint8_t *text, uint8_t len, uint16_t color)
{
    (void)LcdClock_WriteCnText(16, y, text, len, color, WHITE);
}

static uint16_t LcdClock_WriteCn16Text(uint16_t x, uint16_t y, const uint8_t *text, uint8_t len, uint16_t color, uint16_t bg)
{
    uint8_t i;

    for (i = 0U; i < len; i++)
    {
        Inf_LCD_WriteChineseChar16(x, y, text[i], color, bg);
        x = (uint16_t)(x + 16U);
    }

    return x;
}

static uint16_t LcdClock_WriteCn16Label(uint16_t y, const uint8_t *text, uint8_t len, uint16_t color)
{
    return LcdClock_WriteCn16Text(16, y, text, len, color, WHITE);
}

static void LcdClock_WriteKeyHint16(uint16_t x, uint16_t y, const char *key, const uint8_t *text, uint8_t len, uint16_t color)
{
    char label[8];

    snprintf(label, sizeof(label), "%s:", key);
    Inf_LCD_WriteAsciiString(x, y, 16, (uint8_t *)label, color, WHITE);
    (void)LcdClock_WriteCn16Text((uint16_t)(x + 40U), y, text, len, color, WHITE);
}

static uint16_t LcdClock_WriteCnLabel(uint16_t y, const uint8_t *text, uint8_t len, uint16_t color)
{
    return LcdClock_WriteCnText(16, y, text, len, color, WHITE);
}
static void LcdClock_WriteCd16Char(uint16_t x, uint16_t y, uint8_t index, uint16_t color, uint16_t bg)
{
    uint8_t row;
    uint8_t col;

    if (index >= APP_COUNTDOWN_FONT16_COUNT)
    {
        return;
    }

    Inf_LCD_SetArea(x, y, 16, 16);
    *LCD_ADDR_CMD = 0x2C;

    for (row = 0U; row < 16U; row++)
    {
        for (col = 0U; col < 16U; col++)
        {
            uint8_t data = app_countdown_font16[index][(uint16_t)row * 2U + (col / 8U)];
            uint8_t pixel = (uint8_t)((data >> (col % 8U)) & 0x01U);
            Inf_LCD_WriteData(pixel ? color : bg);
        }
    }
}

static uint16_t LcdClock_WriteCd16Text(uint16_t x, uint16_t y, const uint8_t *text, uint8_t len, uint16_t color, uint16_t bg)
{
    uint8_t i;

    for (i = 0U; i < len; i++)
    {
        LcdClock_WriteCd16Char(x, y, text[i], color, bg);
        x = (uint16_t)(x + 16U);
    }

    return x;
}

static void LcdClock_WriteCdKeyHint16(uint16_t x, uint16_t y, const char *key, const uint8_t *text, uint8_t len, uint16_t color)
{
    char label[8];

    snprintf(label, sizeof(label), "%s:", key);
    Inf_LCD_WriteAsciiString(x, y, 16, (uint8_t *)label, color, WHITE);
    (void)LcdClock_WriteCd16Text((uint16_t)(x + 40U), y, text, len, color, WHITE);
}
static void LcdClock_WriteCd32Char(uint16_t x, uint16_t y, uint8_t index, uint16_t color, uint16_t bg)
{
    uint8_t row;
    uint8_t col;

    if (index >= APP_COUNTDOWN_FONT32_COUNT)
    {
        return;
    }

    Inf_LCD_SetArea(x, y, 32, 32);
    *LCD_ADDR_CMD = 0x2C;

    for (row = 0U; row < 32U; row++)
    {
        for (col = 0U; col < 32U; col++)
        {
            uint8_t data = app_countdown_font32[index][(uint16_t)row * 4U + (col / 8U)];
            uint8_t pixel = (uint8_t)((data >> (col % 8U)) & 0x01U);
            Inf_LCD_WriteData(pixel ? color : bg);
        }
    }
}

static uint16_t LcdClock_WriteCd32Text(uint16_t x, uint16_t y, const uint8_t *text, uint8_t len, uint16_t color, uint16_t bg)
{
    uint8_t i;

    for (i = 0U; i < len; i++)
    {
        LcdClock_WriteCd32Char(x, y, text[i], color, bg);
        x = (uint16_t)(x + 32U);
    }

    return x;
}

static void LcdClock_WriteWeatherWind32Char(uint16_t x, uint16_t y, uint8_t index, uint16_t color, uint16_t bg)
{
    uint8_t row;
    uint8_t col;

    if (index >= APP_WEATHER_WIND_FONT32_COUNT)
    {
        return;
    }

    Inf_LCD_SetArea(x, y, 32, 32);
    *LCD_ADDR_CMD = 0x2C;

    for (row = 0U; row < 32U; row++)
    {
        for (col = 0U; col < 32U; col++)
        {
            uint8_t data = app_weather_wind_font32[index][(uint16_t)row * 4U + (col / 8U)];
            uint8_t pixel = (uint8_t)((data >> (col % 8U)) & 0x01U);
            Inf_LCD_WriteData(pixel ? color : bg);
        }
    }
}

static uint16_t LcdClock_WriteWeatherWind32Text(uint16_t x, uint16_t y, const uint8_t *text, uint8_t len, uint16_t color, uint16_t bg)
{
    uint8_t i;

    for (i = 0U; i < len; i++)
    {
        LcdClock_WriteWeatherWind32Char(x, y, text[i], color, bg);
        x = (uint16_t)(x + 32U);
    }

    return x;
}

static uint8_t LcdClock_TextHasUtf8(const char *text, const char *needle)
{
    if ((text == NULL) || (needle == NULL))
    {
        return 0U;
    }

    return (strstr(text, needle) != NULL) ? 1U : 0U;
}

static uint16_t LcdClock_WriteWeatherWindDirection32(uint16_t x, uint16_t y, const char *direction, uint16_t color)
{
    static const char utf8_xuan_zhuan_bu_ding[] = "\xE6\x97\x8B" "\xE8\xBD\xAC" "\xE4\xB8\x8D" "\xE5\xAE\x9A";
    static const char utf8_dong_bei[] = "\xE4\xB8\x9C" "\xE5\x8C\x97";
    static const char utf8_dong_nan[] = "\xE4\xB8\x9C" "\xE5\x8D\x97";
    static const char utf8_xi_bei[] = "\xE8\xA5\xBF" "\xE5\x8C\x97";
    static const char utf8_xi_nan[] = "\xE8\xA5\xBF" "\xE5\x8D\x97";
    static const char utf8_dong[] = "\xE4\xB8\x9C";
    static const char utf8_nan[] = "\xE5\x8D\x97";
    static const char utf8_xi[] = "\xE8\xA5\xBF";
    static const char utf8_bei[] = "\xE5\x8C\x97";
    static const char utf8_wu[] = "\xE6\x97\xA0";
    static const uint8_t xuan_zhuan_bu_ding[] = {WEATHER_WIND32_XUAN, WEATHER_WIND32_ZHUAN, WEATHER_WIND32_BU, WEATHER_WIND32_DING};
    static const uint8_t xiang_dong_bei[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_DONG, WEATHER_WIND32_BEI};
    static const uint8_t xiang_dong_nan[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_DONG, WEATHER_WIND32_NAN};
    static const uint8_t xiang_xi_bei[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_XI, WEATHER_WIND32_BEI};
    static const uint8_t xiang_xi_nan[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_XI, WEATHER_WIND32_NAN};
    static const uint8_t xiang_dong[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_DONG};
    static const uint8_t xiang_nan[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_NAN};
    static const uint8_t xiang_xi[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_XI};
    static const uint8_t xiang_bei[] = {WEATHER_WIND32_XIANG, WEATHER_WIND32_BEI};
    static const uint8_t wu_one[] = {CD32_WU};
    static const uint8_t feng_one[] = {WEATHER_WIND32_FENG};

    if ((direction == NULL) || (direction[0] == '\0'))
    {
        Inf_LCD_WriteAsciiString(x, (uint16_t)(y + 4U), 24, (uint8_t *)"--", color, WHITE);
        return (uint16_t)(x + 28U);
    }

    if (LcdClock_TextHasUtf8(direction, utf8_xuan_zhuan_bu_ding))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xuan_zhuan_bu_ding, LCD_CN_LEN(xuan_zhuan_bu_ding), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_wu))
    {
        x = LcdClock_WriteCd32Text(x, y, wu_one, LCD_CN_LEN(wu_one), color, WHITE);
        return LcdClock_WriteWeatherWind32Text(x, y, feng_one, LCD_CN_LEN(feng_one), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_dong_bei))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_dong_bei, LCD_CN_LEN(xiang_dong_bei), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_dong_nan))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_dong_nan, LCD_CN_LEN(xiang_dong_nan), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_xi_bei))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_xi_bei, LCD_CN_LEN(xiang_xi_bei), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_xi_nan))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_xi_nan, LCD_CN_LEN(xiang_xi_nan), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_dong))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_dong, LCD_CN_LEN(xiang_dong), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_nan))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_nan, LCD_CN_LEN(xiang_nan), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_xi))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_xi, LCD_CN_LEN(xiang_xi), color, WHITE);
    }
    if (LcdClock_TextHasUtf8(direction, utf8_bei))
    {
        return LcdClock_WriteWeatherWind32Text(x, y, xiang_bei, LCD_CN_LEN(xiang_bei), color, WHITE);
    }

    Inf_LCD_WriteAsciiString(x, (uint16_t)(y + 4U), 24, (uint8_t *)"--", color, WHITE);
    return (uint16_t)(x + 28U);
}
static void LcdClock_WriteCd32Header(const uint8_t *text, uint8_t len)
{
    uint16_t width = (uint16_t)len * 32U;
    uint16_t x = 0U;

    if (width < DISPLAY_W)
    {
        x = (uint16_t)((DISPLAY_W - width) / 2U);
    }

    (void)LcdClock_WriteCd32Text(x, 4, text, len, WHITE, BLUE);
}

static void LcdClock_FormatSeconds(uint32_t seconds, char *buffer, uint8_t size)
{
    uint32_t hour = seconds / 3600U;
    uint32_t minute = (seconds % 3600U) / 60U;
    uint32_t second = seconds % 60U;

    snprintf(buffer, size, "%02lu:%02lu:%02lu",
             (unsigned long)hour,
             (unsigned long)minute,
             (unsigned long)second);
}

/* 将计时设置页的时分秒合成为秒数，供倒计时状态机使用。 */
static uint32_t LcdClock_TimerSetTotalSeconds(void)
{
    return ((uint32_t)timer_set_hour * 3600U) +
           ((uint32_t)timer_set_minute * 60U) +
           (uint32_t)timer_set_second;
}

static void LcdClock_ResetTimerSetValue(void)
{
    timer_set_hour = 0U;
    timer_set_minute = 0U;
    timer_set_second = 0U;
    timer_edit_field = LCD_EDIT_HOUR;
}

/* 设置页按 SW4/SW5 修改当前选中的时、分或秒，最大 23:59:59。 */
static void LcdClock_AdjustTimerSet(int8_t step)
{
    if (timer_edit_field == LCD_EDIT_HOUR)
    {
        timer_set_hour = LcdClock_AdjustWrapU8(timer_set_hour, 0U, 23U, step);
    }
    else if (timer_edit_field == LCD_EDIT_MINUTE)
    {
        timer_set_minute = LcdClock_AdjustWrapU8(timer_set_minute, 0U, 59U, step);
    }
    else
    {
        timer_set_second = LcdClock_AdjustWrapU8(timer_set_second, 0U, 59U, step);
    }
}

static void LcdClock_NextTimerField(void)
{
    if (timer_edit_field == LCD_EDIT_HOUR)
    {
        timer_edit_field = LCD_EDIT_MINUTE;
    }
    else if (timer_edit_field == LCD_EDIT_MINUTE)
    {
        timer_edit_field = LCD_EDIT_SECOND;
    }
    else
    {
        timer_edit_field = LCD_EDIT_HOUR;
    }
}

/* 首页和计时状态页共用的中文状态文本：无计时、计时中、已暂停、计时完成。 */
static uint16_t LcdClock_WriteCountdownState32(uint16_t x, uint16_t y, AppCountdown_State_t state, uint16_t color)
{
    static const uint8_t none_text[] = {CD32_WU, CD32_JI, CD32_SHI};
    static const uint8_t running_text[] = {CD32_JI, CD32_SHI, CD32_ZHONG};
    static const uint8_t done_text[] = {CD32_JI, CD32_SHI, CD32_WAN, CD32_CHENG};
    static const uint8_t paused_text[] = {CD32_YI, CD32_ZAN, CD32_TING_PAUSE};

    switch (state)
    {
    case APP_COUNTDOWN_RUNNING:
        return LcdClock_WriteCd32Text(x, y, running_text, LCD_CN_LEN(running_text), color, WHITE);
    case APP_COUNTDOWN_PAUSED:
        return LcdClock_WriteCd32Text(x, y, paused_text, LCD_CN_LEN(paused_text), color, WHITE);
    case APP_COUNTDOWN_DONE:
        return LcdClock_WriteCd32Text(x, y, done_text, LCD_CN_LEN(done_text), color, WHITE);
    default:
        return LcdClock_WriteCd32Text(x, y, none_text, LCD_CN_LEN(none_text), color, WHITE);
    }
}


static void LcdClock_WriteSignedCnValue(uint16_t y, const uint8_t *label, uint8_t label_len, int32_t value, const char *unit)
{
    char text[24];
    const char *sign = "";
    uint32_t abs_value;
    uint16_t x;
    uint16_t ascii_x;

    if (value < 0)
    {
        sign = "-";
        abs_value = (uint32_t)(-value);
    }
    else
    {
        abs_value = (uint32_t)value;
    }

    x = LcdClock_WriteCnLabel(y, label, label_len, BLACK);
    ascii_x = (uint16_t)(x + 4U);

    if ((unit[0] == 'C') && (unit[1] == '\0'))
    {
        snprintf(text, sizeof(text), ": %s%lu.%lu ",
                 sign,
                 (unsigned long)(abs_value / 10U),
                 (unsigned long)(abs_value % 10U));
        Inf_LCD_WriteAsciiString(ascii_x, (uint16_t)(y + 4U), 24, (uint8_t *)text, BLACK, WHITE);
        Inf_LCD_WriteChineseChar16((uint16_t)(ascii_x + strlen(text) * 12U), (uint16_t)(y + 8U), CN16_DEG_C, BLACK, WHITE);
        return;
    }

    snprintf(text, sizeof(text), ": %s%lu.%lu %s",
             sign,
             (unsigned long)(abs_value / 10U),
             (unsigned long)(abs_value % 10U),
             unit);
    Inf_LCD_WriteAsciiString(ascii_x, (uint16_t)(y + 4U), 24, (uint8_t *)text, BLACK, WHITE);
}

static const uint8_t LCD_TEXT_HOME[] = {CN_ZHU, CN_YE};
static const uint8_t LCD_TEXT_DATE_EDIT[] = {CN_RI, CN_QI, CN_SHE, CN_ZHI};
static const uint8_t LCD_TEXT_TIME_EDIT[] = {CN_SHI, CN_JIAN, CN_SHE, CN_ZHI};
static const uint8_t LCD_TEXT_WEATHER[] = {CN_TIAN, CN_TIAN_QI};
static const uint8_t LCD_TEXT_WEATHER_DETAIL[] = {CN_TIAN, CN_TIAN_QI, CN_XIANG_QING, CN_QING_DETAIL};
static const uint8_t LCD_TEXT_STATUS[] = {CN_ZHUANG, CN_TAI};
static const uint8_t LCD_TEXT_NETWORK[] = {CN_WANG, CN_LUO};
static const uint8_t LCD_TEXT_CONNECTED[] = {CN_YI, CN_LIAN, CN_JIE};
static const uint8_t LCD_TEXT_DISCONNECTED[] = {CN_WEI, CN_LIAN, CN_JIE};
static const uint8_t LCD_TEXT_DATE[] = {CN_RI, CN_QI};
static const uint8_t LCD_TEXT_TIME[] = {CN_SHI, CN_JIAN};
static const uint8_t LCD_TEXT_FIELD[] = {CN_XUAN, CN_XIANG};
static const uint8_t LCD_TEXT_SAVE[] = {CN_BAO, CN_CUN};
static const uint8_t LCD_TEXT_BACK_HOME[] = {CN_FAN, CN_HUI, CN_ZHU, CN_YE};
static const uint8_t LCD_TEXT_NO_WEATHER[] = {CN_WU, CN_TIAN, CN_TIAN_QI, CN_SHU, CN_JU};
static const uint8_t LCD_TEXT_NEED_NETWORK[] = {CN_XU, CN_YAO, CN_WANG, CN_LUO};
static const uint8_t LCD_TEXT_TEMP[] = {CN_WEN, CN_DU};
static const uint8_t LCD_TEXT_HUMIDITY[] = {CN_SHI_WET, CN_DU};
static const uint8_t LCD_TEXT_READY[] = {CN_JIU, CN_XU_READY};
static const uint8_t LCD_TEXT_ERROR[] = {CN_CUO, CN_WU_ERR};
static const uint8_t LCD_TEXT_IDLE[] = {CN_KONG, CN_XIAN};
static const uint8_t LCD_TEXT_VALID[] = {CN_YOU, CN_XIAO};
static const uint8_t LCD_TEXT_UNKNOWN[] = {CN_WEI_UNKNOWN, CN_ZHI_KNOW};
static const uint8_t LCD_TEXT_NEXT_PAGE[] = {CN_XIA, CN_YI_ONE, CN_YE_PAGE};
static const uint8_t LCD_TEXT_16_REFRESH[] = {CN16_SHUA, CN16_XIN};
static const uint8_t LCD_TEXT_MODIFY_DATE[] = {CN16_XIU, CN16_GAI, CN16_RI, CN16_QI_DATE};
static const uint8_t LCD_TEXT_DISPLAY_WEATHER[] = {CN16_XIAN_SHOW, CN16_SHI_SHOW, CN16_TIAN, CN16_QI_WEATHER};
static const uint8_t LCD_TEXT_MODIFY_TIME[] = {CN16_XIU, CN16_GAI, CN16_SHI_TIME, CN16_JIAN_TIME};

static const uint8_t LCD_TIMER_TITLE_STATUS[] = {CD32_JI, CD32_SHI, CD32_ZHUANG, CD32_TAI};
static const uint8_t LCD_TIMER_TITLE_SET[] = {CD32_JI, CD32_SHI, CD32_SHE, CD32_ZHI};
static const uint8_t LCD_TIMER_TITLE_CONFIRM[] = {CD32_QUE_RECOGNIZE, CD32_REN, CD32_JI, CD32_SHI};
static const uint8_t LCD_TIMER_LABEL[] = {CD32_JI, CD32_SHI};
static const uint8_t LCD_TIMER_REMAIN[] = {CD32_SHENG, CD32_YU};
static const uint8_t LCD_TIMER_STATE[] = {CD32_ZHUANG, CD32_TAI};
static const uint8_t LCD_TIMER_SET_LABEL[] = {CD32_SHE, CD32_ZHI};
static const uint8_t LCD_TIMER_OPTION[] = {CD32_XUAN, CD32_XIANG};
static const uint8_t LCD_TIMER_CONFIRM_QUESTION[] = {CD32_QUE_CONFIRM, CD32_DING, CD32_YAO, CD32_JI, CD32_SHI};
static const uint8_t LCD_TIMER_FIELD_HOUR[] = {CD32_SHI};
static const uint8_t LCD_TIMER_FIELD_MINUTE[] = {CD32_FEN};
static const uint8_t LCD_TIMER_FIELD_SECOND[] = {CD32_MIAO};
static const uint8_t LCD_TIMER16_COUNTDOWN[] = {CD16_JI, CD16_SHI};
static const uint8_t LCD_TIMER16_OPTION[] = {CD16_XUAN, CD16_XIANG};
static const uint8_t LCD_TIMER16_CONFIRM[] = {CD16_QUE, CD16_REN};
static const uint8_t LCD_TIMER16_START[] = {CD16_KAI, CD16_SHI_START};
static const uint8_t LCD_TIMER16_BACK[] = {CD16_FAN, CD16_HUI};
static const uint8_t LCD_TIMER16_STOP[] = {CD16_TING, CD16_ZHI_STOP};
static const uint8_t LCD_TIMER16_CONTINUE[] = {CD16_JI_CONTINUE, CD16_XU};
static const uint8_t LCD_TIMER16_CANCEL[] = {CD16_QU, CD16_XIAO};
static const uint8_t LCD_TIMER16_CLOSE[] = {CD16_GUAN, CD16_BI};

static void LcdClock_WriteHeaderTitle(const uint8_t *text, uint8_t len)
{
    uint16_t width = (uint16_t)len * 32U;
    uint16_t x = 0U;

    if (width < DISPLAY_W)
    {
        x = (uint16_t)((DISPLAY_W - width) / 2U);
    }

    (void)LcdClock_WriteCnText(x, 4, text, len, WHITE, BLUE);
}

static void LcdClock_DrawHeader(void)
{
    switch (current_page)
    {
    case LCD_PAGE_HOME:
        LcdClock_WriteHeaderTitle(LCD_TEXT_HOME, LCD_CN_LEN(LCD_TEXT_HOME));
        break;
    case LCD_PAGE_DATE_EDIT:
        LcdClock_WriteHeaderTitle(LCD_TEXT_DATE_EDIT, LCD_CN_LEN(LCD_TEXT_DATE_EDIT));
        break;
    case LCD_PAGE_TIME_EDIT:
        LcdClock_WriteHeaderTitle(LCD_TEXT_TIME_EDIT, LCD_CN_LEN(LCD_TEXT_TIME_EDIT));
        break;
    case LCD_PAGE_WEATHER:
        LcdClock_WriteHeaderTitle(LCD_TEXT_WEATHER, LCD_CN_LEN(LCD_TEXT_WEATHER));
        break;
    case LCD_PAGE_WEATHER_DETAIL:
        LcdClock_WriteHeaderTitle(LCD_TEXT_WEATHER_DETAIL, LCD_CN_LEN(LCD_TEXT_WEATHER_DETAIL));
        break;
    case LCD_PAGE_STATUS:
        LcdClock_WriteHeaderTitle(LCD_TEXT_STATUS, LCD_CN_LEN(LCD_TEXT_STATUS));
        break;
    case LCD_PAGE_TIMER_STATUS:
        LcdClock_WriteCd32Header(LCD_TIMER_TITLE_STATUS, LCD_CN_LEN(LCD_TIMER_TITLE_STATUS));
        break;
    case LCD_PAGE_TIMER_SET:
        LcdClock_WriteCd32Header(LCD_TIMER_TITLE_SET, LCD_CN_LEN(LCD_TIMER_TITLE_SET));
        break;
    case LCD_PAGE_TIMER_CONFIRM:
        LcdClock_WriteCd32Header(LCD_TIMER_TITLE_CONFIRM, LCD_CN_LEN(LCD_TIMER_TITLE_CONFIRM));
        break;
    default:
        Inf_LCD_WriteAsciiString(112, 8, 24, (uint8_t *)"UNKNOWN", WHITE, BLUE);
        break;
    }
}

static uint16_t LcdClock_WriteWeatherNameCn(uint16_t x, uint16_t y, uint16_t code, uint16_t color)
{
    static const uint8_t clear_text[] = {CN_QING_CLEAR};
    static const uint8_t cloudy_text[] = {CN_DUO, CN_YUN};
    static const uint8_t fog_text[] = {CN_WU_FOG};
    static const uint8_t drizzle_text[] = {CN_MAO, CN_MAO, CN_YU};
    static const uint8_t rain_text[] = {CN_YU};
    static const uint8_t snow_text[] = {CN_XUE};
    static const uint8_t thunder_text[] = {CN_LEI, CN_YU};

    if (code == 0U)
    {
        return LcdClock_WriteCnText(x, y, clear_text, LCD_CN_LEN(clear_text), color, WHITE);
    }
    if ((code >= 1U) && (code <= 3U))
    {
        return LcdClock_WriteCnText(x, y, cloudy_text, LCD_CN_LEN(cloudy_text), color, WHITE);
    }
    if ((code == 45U) || (code == 48U))
    {
        return LcdClock_WriteCnText(x, y, fog_text, LCD_CN_LEN(fog_text), color, WHITE);
    }
    if ((code >= 51U) && (code <= 57U))
    {
        return LcdClock_WriteCnText(x, y, drizzle_text, LCD_CN_LEN(drizzle_text), color, WHITE);
    }
    if (((code >= 61U) && (code <= 67U)) || ((code >= 80U) && (code <= 82U)))
    {
        return LcdClock_WriteCnText(x, y, rain_text, LCD_CN_LEN(rain_text), color, WHITE);
    }
    if ((code >= 71U) && (code <= 77U))
    {
        return LcdClock_WriteCnText(x, y, snow_text, LCD_CN_LEN(snow_text), color, WHITE);
    }
    if ((code >= 95U) && (code <= 99U))
    {
        return LcdClock_WriteCnText(x, y, thunder_text, LCD_CN_LEN(thunder_text), color, WHITE);
    }

    return LcdClock_WriteCnText(x, y, LCD_TEXT_UNKNOWN, LCD_CN_LEN(LCD_TEXT_UNKNOWN), color, WHITE);
}

static uint16_t LcdClock_WriteEditFieldCn(uint16_t x, uint16_t y, uint16_t color)
{
    static const uint8_t year_text[] = {CN_NIAN};
    static const uint8_t month_text[] = {CN_YUE};
    static const uint8_t day_text[] = {CN_RI};
    static const uint8_t hour_text[] = {CN_SHI};
    static const uint8_t minute_text[] = {CN_FEN};
    static const uint8_t second_text[] = {CN_MIAO};

    switch (edit_field)
    {
    case LCD_EDIT_YEAR:
        return LcdClock_WriteCn16Text(x, y, year_text, LCD_CN_LEN(year_text), color, WHITE);
    case LCD_EDIT_MONTH:
        return LcdClock_WriteCn16Text(x, y, month_text, LCD_CN_LEN(month_text), color, WHITE);
    case LCD_EDIT_DAY:
        return LcdClock_WriteCn16Text(x, y, day_text, LCD_CN_LEN(day_text), color, WHITE);
    case LCD_EDIT_HOUR:
        return LcdClock_WriteCn16Text(x, y, hour_text, LCD_CN_LEN(hour_text), color, WHITE);
    case LCD_EDIT_MINUTE:
        return LcdClock_WriteCn16Text(x, y, minute_text, LCD_CN_LEN(minute_text), color, WHITE);
    case LCD_EDIT_SECOND:
        return LcdClock_WriteCn16Text(x, y, second_text, LCD_CN_LEN(second_text), color, WHITE);
    default:
        return x;
    }
}

/* 首页只显示计时状态摘要，具体暂停/继续/取消操作放在计时状态页完成。 */
static void LcdClock_DrawHome(void)
{
    DateTime_t now;
    char text[48];

    App_GetRtcDateTime(&now);

    snprintf(text, sizeof(text), "%04u-%02u-%02u", now.year, now.month, now.day);
    Inf_LCD_WriteAsciiString(80, 48, 32, (uint8_t *)text, BLACK, WHITE);

    snprintf(text, sizeof(text), "%02u:%02u:%02u", now.hour, now.minute, now.second);
    Inf_LCD_WriteAsciiString(96, 94, 32, (uint8_t *)text, BLUE, WHITE);

    if (home_static_area_cleared == 0U)
    {
        LcdClock_FillRect(0, 134, DISPLAY_W, 146, WHITE);
        home_static_area_cleared = 1U;
    }

    if (home_wifi_connected != 0U)
    {
        uint16_t x = LcdClock_WriteCnText(80, 136, LCD_TEXT_NETWORK, LCD_CN_LEN(LCD_TEXT_NETWORK), GREEN, WHITE);
        (void)LcdClock_WriteCnText(x, 136, LCD_TEXT_CONNECTED, LCD_CN_LEN(LCD_TEXT_CONNECTED), GREEN, WHITE);
    }
    else
    {
        uint16_t x = LcdClock_WriteCnText(80, 136, LCD_TEXT_NETWORK, LCD_CN_LEN(LCD_TEXT_NETWORK), RED, WHITE);
        (void)LcdClock_WriteCnText(x, 136, LCD_TEXT_DISCONNECTED, LCD_CN_LEN(LCD_TEXT_DISCONNECTED), RED, WHITE);
    }

    (void)LcdClock_WriteCd32Text(72, 176, LCD_TIMER_LABEL, LCD_CN_LEN(LCD_TIMER_LABEL), BLUE, WHITE);
    Inf_LCD_WriteAsciiString(136, 180, 24, (uint8_t *)":", BLUE, WHITE);
    LcdClock_FillRect(152, 176, 168, 34, WHITE);
    (void)LcdClock_WriteCountdownState32(168, 176, AppCountdown_GetState(), BLUE);

    LcdClock_WriteKeyHint16(32, 224, "SW3", LCD_TEXT_MODIFY_DATE, LCD_CN_LEN(LCD_TEXT_MODIFY_DATE), BLACK);
    LcdClock_WriteKeyHint16(176, 224, "SW4", LCD_TEXT_DISPLAY_WEATHER, LCD_CN_LEN(LCD_TEXT_DISPLAY_WEATHER), BLACK);
    LcdClock_WriteKeyHint16(32, 252, "SW5", LCD_TEXT_MODIFY_TIME, LCD_CN_LEN(LCD_TEXT_MODIFY_TIME), BLACK);
    LcdClock_WriteCdKeyHint16(176, 252, "SW6", LCD_TIMER16_COUNTDOWN, LCD_CN_LEN(LCD_TIMER16_COUNTDOWN), BLACK);

    LcdClock_DrawCat();
}

static void LcdClock_DrawDateEdit(void)
{
    char text[48];
    uint16_t x;

    x = LcdClock_WriteCn16Label(64, LCD_TEXT_FIELD, LCD_CN_LEN(LCD_TEXT_FIELD), RED);
    (void)LcdClock_WriteEditFieldCn(x, 64, RED);

    snprintf(text, sizeof(text), ": %04u-%02u-%02u", edit_time.year, edit_time.month, edit_time.day);
    x = LcdClock_WriteCnLabel(104, LCD_TEXT_DATE, LCD_CN_LEN(LCD_TEXT_DATE), BLACK);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 108, 24, (uint8_t *)text, BLACK, WHITE);

    LcdClock_WriteKeyHint16(16, 154, "SW3", LCD_TEXT_FIELD, LCD_CN_LEN(LCD_TEXT_FIELD), BLUE);
    Inf_LCD_WriteAsciiString(16, 184, 16, (uint8_t *)"SW4: +   SW5: -", BLUE, WHITE);
    LcdClock_WriteKeyHint16(16, 216, "SW6", LCD_TEXT_SAVE, LCD_CN_LEN(LCD_TEXT_SAVE), BLUE);
}

static void LcdClock_DrawTimeEdit(void)
{
    char text[48];
    uint16_t x;

    x = LcdClock_WriteCn16Label(64, LCD_TEXT_FIELD, LCD_CN_LEN(LCD_TEXT_FIELD), RED);
    (void)LcdClock_WriteEditFieldCn(x, 64, RED);

    snprintf(text, sizeof(text), ": %02u:%02u:%02u", edit_time.hour, edit_time.minute, edit_time.second);
    x = LcdClock_WriteCnLabel(104, LCD_TEXT_TIME, LCD_CN_LEN(LCD_TEXT_TIME), BLACK);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 108, 24, (uint8_t *)text, BLACK, WHITE);

    LcdClock_WriteKeyHint16(16, 154, "SW3", LCD_TEXT_FIELD, LCD_CN_LEN(LCD_TEXT_FIELD), BLUE);
    Inf_LCD_WriteAsciiString(16, 184, 16, (uint8_t *)"SW4: +   SW5: -", BLUE, WHITE);
    LcdClock_WriteKeyHint16(16, 216, "SW6", LCD_TEXT_SAVE, LCD_CN_LEN(LCD_TEXT_SAVE), BLUE);
}

static void LcdClock_DrawWeather(void)
{
    const AppWeather_Data_t *weather = AppWeather_GetData();
    char text[56];
    uint16_t x;

    if (!weather->valid)
    {
        LcdClock_WriteCnLine(56, LCD_TEXT_NO_WEATHER, LCD_CN_LEN(LCD_TEXT_NO_WEATHER), RED);
        LcdClock_WriteKeyHint16(16, 100, "SW6", LCD_TEXT_16_REFRESH, LCD_CN_LEN(LCD_TEXT_16_REFRESH), BLUE);
        LcdClock_WriteCnLine(136, LCD_TEXT_NEED_NETWORK, LCD_CN_LEN(LCD_TEXT_NEED_NETWORK), GRAY);
        return;
    }

    snprintf(text, sizeof(text), "Time: %.16s", weather->time);
    LcdClock_WriteLine(48, text, BLACK);

    LcdClock_WriteSignedCnValue(84, LCD_TEXT_TEMP, LCD_CN_LEN(LCD_TEXT_TEMP), weather->temperature_x10, "C");

    x = LcdClock_WriteCnLabel(120, LCD_TEXT_WEATHER, LCD_CN_LEN(LCD_TEXT_WEATHER), BLACK);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 124, 24, (uint8_t *)":", BLACK, WHITE);
    (void)LcdClock_WriteWeatherNameCn((uint16_t)(x + 20U), 120, weather->weather_code, BLACK);

    snprintf(text, sizeof(text), ": %u%%", weather->humidity);
    x = LcdClock_WriteCnLabel(156, LCD_TEXT_HUMIDITY, LCD_CN_LEN(LCD_TEXT_HUMIDITY), BLACK);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 160, 24, (uint8_t *)text, BLACK, WHITE);

    LcdClock_WriteKeyHint16(16, 214, "SW3", LCD_TEXT_NEXT_PAGE, LCD_CN_LEN(LCD_TEXT_NEXT_PAGE), BLUE);
    LcdClock_WriteKeyHint16(16, 252, "SW6", LCD_TEXT_16_REFRESH, LCD_CN_LEN(LCD_TEXT_16_REFRESH), BLUE);
}

static void LcdClock_DrawWeatherDetail(void)
{
    const AppWeather_Data_t *weather = AppWeather_GetData();
    static const uint8_t wind_direction_label[] = {WEATHER_WIND32_FENG, WEATHER_WIND32_XIANG};
    static const uint8_t wind_power_label[] = {WEATHER_WIND32_FENG, WEATHER_WIND32_LI};
    char text[56];
    uint16_t x;

    if (!weather->valid)
    {
        LcdClock_WriteCnLine(56, LCD_TEXT_NO_WEATHER, LCD_CN_LEN(LCD_TEXT_NO_WEATHER), RED);
        LcdClock_WriteKeyHint16(16, 100, "SW5", LCD_TEXT_16_REFRESH, LCD_CN_LEN(LCD_TEXT_16_REFRESH), BLUE);
        LcdClock_WriteKeyHint16(16, 132, "SW6", LCD_TEXT_BACK_HOME, LCD_CN_LEN(LCD_TEXT_BACK_HOME), BLUE);
        return;
    }

    LcdClock_WriteSignedCnValue(44, LCD_TEXT_TEMP, LCD_CN_LEN(LCD_TEXT_TEMP), weather->temperature_x10, "C");

    x = LcdClock_WriteCnLabel(80, LCD_TEXT_WEATHER, LCD_CN_LEN(LCD_TEXT_WEATHER), BLACK);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 84, 24, (uint8_t *)":", BLACK, WHITE);
    (void)LcdClock_WriteWeatherNameCn((uint16_t)(x + 20U), 80, weather->weather_code, BLACK);

    x = LcdClock_WriteCnLabel(116, LCD_TEXT_HUMIDITY, LCD_CN_LEN(LCD_TEXT_HUMIDITY), BLACK);
    snprintf(text, sizeof(text), ": %u%%", weather->humidity);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 120, 24, (uint8_t *)text, BLACK, WHITE);

    x = LcdClock_WriteWeatherWind32Text(16, 152, wind_direction_label, LCD_CN_LEN(wind_direction_label), BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 156, 24, (uint8_t *)":", BLACK, WHITE);
    (void)LcdClock_WriteWeatherWindDirection32((uint16_t)(x + 20U), 152, weather->wind_direction_text, BLACK);

    x = LcdClock_WriteWeatherWind32Text(16, 188, wind_power_label, LCD_CN_LEN(wind_power_label), BLACK, WHITE);
    snprintf(text, sizeof(text), ": %s", (weather->wind_power_text[0] != '\0') ? weather->wind_power_text : "--");
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 192, 24, (uint8_t *)text, BLACK, WHITE);

    LcdClock_WriteKeyHint16(16, 236, "SW5", LCD_TEXT_16_REFRESH, LCD_CN_LEN(LCD_TEXT_16_REFRESH), BLUE);
    LcdClock_WriteKeyHint16(16, 264, "SW6", LCD_TEXT_BACK_HOME, LCD_CN_LEN(LCD_TEXT_BACK_HOME), BLUE);
}

static void LcdClock_DrawStatus(void)
{
    const AppWeather_Data_t *weather = AppWeather_GetData();
    uint16_t x;

    switch (AppWifi_GetState())
    {
    case APP_WIFI_STATE_READY:
        x = LcdClock_WriteCnLabel(56, LCD_TEXT_NETWORK, LCD_CN_LEN(LCD_TEXT_NETWORK), BLACK);
        (void)LcdClock_WriteCnText(x, 56, LCD_TEXT_READY, LCD_CN_LEN(LCD_TEXT_READY), BLACK, WHITE);
        break;
    case APP_WIFI_STATE_ERROR:
        x = LcdClock_WriteCnLabel(56, LCD_TEXT_NETWORK, LCD_CN_LEN(LCD_TEXT_NETWORK), RED);
        (void)LcdClock_WriteCnText(x, 56, LCD_TEXT_ERROR, LCD_CN_LEN(LCD_TEXT_ERROR), RED, WHITE);
        break;
    default:
        x = LcdClock_WriteCnLabel(56, LCD_TEXT_NETWORK, LCD_CN_LEN(LCD_TEXT_NETWORK), GRAY);
        (void)LcdClock_WriteCnText(x, 56, LCD_TEXT_IDLE, LCD_CN_LEN(LCD_TEXT_IDLE), GRAY, WHITE);
        break;
    }

    x = LcdClock_WriteCnLabel(96, LCD_TEXT_WEATHER, LCD_CN_LEN(LCD_TEXT_WEATHER), weather->valid ? BLACK : RED);
    if (weather->valid)
    {
        (void)LcdClock_WriteCnText(x, 96, LCD_TEXT_VALID, LCD_CN_LEN(LCD_TEXT_VALID), BLACK, WHITE);
    }
    else
    {
        (void)LcdClock_WriteCnText(x, 96, LCD_TEXT_NO_WEATHER, LCD_CN_LEN(LCD_TEXT_NO_WEATHER), RED, WHITE);
    }

    LcdClock_WriteKeyHint16(16, 150, "SW6", LCD_TEXT_16_REFRESH, LCD_CN_LEN(LCD_TEXT_16_REFRESH), BLUE);
    (void)LcdClock_WriteCn16Text(112, 150, LCD_TEXT_BACK_HOME, LCD_CN_LEN(LCD_TEXT_BACK_HOME), BLUE, WHITE);
}

static uint16_t LcdClock_WriteTimerField32(uint16_t x, uint16_t y, uint16_t color)
{
    if (timer_edit_field == LCD_EDIT_HOUR)
    {
        return LcdClock_WriteCd32Text(x, y, LCD_TIMER_FIELD_HOUR, LCD_CN_LEN(LCD_TIMER_FIELD_HOUR), color, WHITE);
    }
    if (timer_edit_field == LCD_EDIT_MINUTE)
    {
        return LcdClock_WriteCd32Text(x, y, LCD_TIMER_FIELD_MINUTE, LCD_CN_LEN(LCD_TIMER_FIELD_MINUTE), color, WHITE);
    }
    return LcdClock_WriteCd32Text(x, y, LCD_TIMER_FIELD_SECOND, LCD_CN_LEN(LCD_TIMER_FIELD_SECOND), color, WHITE);
}

/* 计时设置页：SW3 切换字段，SW4/SW5 调整数值，SW6 进入确认页。 */
static void LcdClock_DrawTimerSet(void)
{
    char text[24];
    uint16_t x;

    snprintf(text, sizeof(text), "%02u:%02u:%02u", timer_set_hour, timer_set_minute, timer_set_second);

    x = LcdClock_WriteCd32Text(48, 64, LCD_TIMER_SET_LABEL, LCD_CN_LEN(LCD_TIMER_SET_LABEL), BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 68, 24, (uint8_t *)":", BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 24U), 64, 32, (uint8_t *)text, BLACK, WHITE);

    x = LcdClock_WriteCd32Text(48, 116, LCD_TIMER_OPTION, LCD_CN_LEN(LCD_TIMER_OPTION), RED, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 120, 24, (uint8_t *)":", RED, WHITE);
    (void)LcdClock_WriteTimerField32((uint16_t)(x + 24U), 116, RED);

    LcdClock_WriteCdKeyHint16(40, 184, "SW3", LCD_TIMER16_OPTION, LCD_CN_LEN(LCD_TIMER16_OPTION), BLUE);
    Inf_LCD_WriteAsciiString(184, 184, 16, (uint8_t *)"SW4: +", BLUE, WHITE);
    Inf_LCD_WriteAsciiString(40, 216, 16, (uint8_t *)"SW5: -", BLUE, WHITE);
    LcdClock_WriteCdKeyHint16(184, 216, "SW6", LCD_TIMER16_CONFIRM, LCD_CN_LEN(LCD_TIMER16_CONFIRM), BLUE);
}

/* 计时确认页：SW5 开始计时，SW6 放弃并返回首页。 */
static void LcdClock_DrawTimerConfirm(void)
{
    char text[24];

    snprintf(text, sizeof(text), "%02u:%02u:%02u", timer_set_hour, timer_set_minute, timer_set_second);

    (void)LcdClock_WriteCd32Text(64, 76, LCD_TIMER_CONFIRM_QUESTION, LCD_CN_LEN(LCD_TIMER_CONFIRM_QUESTION), BLACK, WHITE);
    Inf_LCD_WriteAsciiString(224, 80, 24, (uint8_t *)"?", BLACK, WHITE);
    Inf_LCD_WriteAsciiString(96, 140, 32, (uint8_t *)text, BLUE, WHITE);

    LcdClock_WriteCdKeyHint16(48, 220, "SW5", LCD_TIMER16_START, LCD_CN_LEN(LCD_TIMER16_START), BLUE);
    LcdClock_WriteCdKeyHint16(184, 220, "SW6", LCD_TIMER16_BACK, LCD_CN_LEN(LCD_TIMER16_BACK), BLUE);
}

/* 计时状态页：运行中可暂停/继续/取消；完成后 SW5/SW6 都会关闭报警并回首页。 */
static void LcdClock_DrawTimerStatus(void)
{
    AppCountdown_State_t state = AppCountdown_GetState();
    char text[24];
    uint16_t x;

    LcdClock_FormatSeconds(AppCountdown_GetRemainingSeconds(), text, (uint8_t)sizeof(text));

    x = LcdClock_WriteCd32Text(32, 56, LCD_TIMER_REMAIN, LCD_CN_LEN(LCD_TIMER_REMAIN), BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 60, 24, (uint8_t *)":", BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 24U), 56, 32, (uint8_t *)text, BLUE, WHITE);

    x = LcdClock_WriteCd32Text(32, 110, LCD_TIMER_STATE, LCD_CN_LEN(LCD_TIMER_STATE), BLACK, WHITE);
    Inf_LCD_WriteAsciiString((uint16_t)(x + 4U), 114, 24, (uint8_t *)":", BLACK, WHITE);
    (void)LcdClock_WriteCountdownState32((uint16_t)(x + 24U), 110, state, (state == APP_COUNTDOWN_DONE) ? RED : BLACK);

    if (state == APP_COUNTDOWN_DONE)
    {
        LcdClock_WriteCdKeyHint16(48, 236, "SW5", LCD_TIMER16_CLOSE, LCD_CN_LEN(LCD_TIMER16_CLOSE), BLUE);
        LcdClock_WriteCdKeyHint16(184, 236, "SW6", LCD_TIMER16_CLOSE, LCD_CN_LEN(LCD_TIMER16_CLOSE), BLUE);
        return;
    }

    LcdClock_WriteCdKeyHint16(40, 200, "SW3", LCD_TIMER16_STOP, LCD_CN_LEN(LCD_TIMER16_STOP), BLUE);
    LcdClock_WriteCdKeyHint16(184, 200, "SW4", LCD_TIMER16_CONTINUE, LCD_CN_LEN(LCD_TIMER16_CONTINUE), BLUE);
    LcdClock_WriteCdKeyHint16(40, 232, "SW5", LCD_TIMER16_CANCEL, LCD_CN_LEN(LCD_TIMER16_CANCEL), BLUE);
    LcdClock_WriteCdKeyHint16(184, 232, "SW6", LCD_TIMER16_BACK, LCD_CN_LEN(LCD_TIMER16_BACK), BLUE);
}

static void LcdClock_DrawScreen(void)
{
    if (lcd_need_full_redraw)
    {
        Inf_LCD_ClearAll(WHITE);
        LcdClock_DrawHeader();
        lcd_need_full_redraw = 0U;
    }

    switch (current_page)
    {
    case LCD_PAGE_HOME:
        LcdClock_DrawHome();
        break;
    case LCD_PAGE_DATE_EDIT:
        LcdClock_DrawDateEdit();
        break;
    case LCD_PAGE_TIME_EDIT:
        LcdClock_DrawTimeEdit();
        break;
    case LCD_PAGE_WEATHER:
        LcdClock_DrawWeather();
        break;
    case LCD_PAGE_WEATHER_DETAIL:
        LcdClock_DrawWeatherDetail();
        break;
    case LCD_PAGE_STATUS:
        LcdClock_DrawStatus();
        break;
    case LCD_PAGE_TIMER_STATUS:
        LcdClock_DrawTimerStatus();
        break;
    case LCD_PAGE_TIMER_SET:
        LcdClock_DrawTimerSet();
        break;
    case LCD_PAGE_TIMER_CONFIRM:
        LcdClock_DrawTimerConfirm();
        break;
    default:
        break;
    }
}

static void LcdClock_EnterPage(LcdPage_t page)
{
    current_page = page;
    lcd_need_full_redraw = 1U;
    home_static_area_cleared = 0U;

    if (page == LCD_PAGE_DATE_EDIT)
    {
        App_GetRtcDateTime(&edit_time);
        edit_field = LCD_EDIT_YEAR;
        edit_dirty = 0U;
    }
    else if (page == LCD_PAGE_TIME_EDIT)
    {
        App_GetRtcDateTime(&edit_time);
        edit_field = LCD_EDIT_HOUR;
        edit_dirty = 0U;
    }
    else if (page == LCD_PAGE_TIMER_SET)
    {
        LcdClock_ResetTimerSetValue();
    }
}
static void LcdClock_NextField(void)
{
    if (current_page == LCD_PAGE_DATE_EDIT)
    {
        if (edit_field == LCD_EDIT_YEAR)
        {
            edit_field = LCD_EDIT_MONTH;
        }
        else if (edit_field == LCD_EDIT_MONTH)
        {
            edit_field = LCD_EDIT_DAY;
        }
        else
        {
            edit_field = LCD_EDIT_YEAR;
        }
    }
    else if (current_page == LCD_PAGE_TIME_EDIT)
    {
        if (edit_field == LCD_EDIT_HOUR)
        {
            edit_field = LCD_EDIT_MINUTE;
        }
        else if (edit_field == LCD_EDIT_MINUTE)
        {
            edit_field = LCD_EDIT_SECOND;
        }
        else
        {
            edit_field = LCD_EDIT_HOUR;
        }
    }
}

static void LcdClock_SaveEdit(void)
{
    DateTime_t now;

    if ((current_page == LCD_PAGE_TIME_EDIT) && (edit_dirty == 0U))
    {
        LcdClock_SendDiag("lcd time edit canceled\r\n");
        LcdClock_EnterPage(LCD_PAGE_HOME);
        return;
    }

    if (current_page == LCD_PAGE_DATE_EDIT)
    {
        App_GetRtcDateTime(&now);
        edit_time.hour = now.hour;
        edit_time.minute = now.minute;
        edit_time.second = now.second;
    }

    App_SetRtcDateTime(&edit_time);
    LcdClock_SendDiag("lcd edit saved\r\n");
    LcdClock_EnterPage(LCD_PAGE_HOME);
}

static void LcdClock_ReportKeyAction(uint16_t pin)
{
    char diag[80];

    snprintf(diag, sizeof(diag), "key action: %s page=%s field=%s\r\n",
             LcdClock_KeyName(pin),
             LcdClock_PageName(),
             LcdClock_EditFieldName());
    LcdClock_SendDiag(diag);
}

/* 手动或自动刷新天气时，先确保 WiFi 可用，再请求天气数据。 */
static uint8_t LcdClock_UpdateWeatherStatus(void)
{
    uint8_t wifi_ok;
    uint8_t weather_ok;

    wifi_ok = AppWifi_Join();
    if (wifi_ok != 0U)
    {
        home_wifi_connected = 1U;
        LcdClock_SendDiag("lcd wifi status CONNECT\r\n");
    }
    else
    {
        home_wifi_connected = 0U;
        LcdClock_SendDiag("lcd wifi status DISCONNECT\r\n");
        return 0U;
    }

    weather_ok = AppWeather_Update();
    if (weather_ok != 0U)
    {
        LcdClock_SendDiag("lcd weather refresh OK\r\n");
        return 1U;
    }

    LcdClock_SendDiag("lcd weather refresh failed, keep wifi CONNECT\r\n");
    return 0U;
}

/* 四个实体按键在不同页面复用，所有页面跳转和参数修改都集中在这里。 */
static void LcdClock_DoKeyAction(uint16_t pin)
{
    AppCountdown_State_t timer_state = AppCountdown_GetState();

    switch (pin)
    {
    case LCD_KEY_SELECT_PIN:
        if (current_page == LCD_PAGE_HOME)
        {
            LcdClock_EnterPage(LCD_PAGE_DATE_EDIT);
        }
        else if ((current_page == LCD_PAGE_DATE_EDIT) || (current_page == LCD_PAGE_TIME_EDIT))
        {
            LcdClock_NextField();
            lcd_need_full_redraw = 1U;
        }
        else if (current_page == LCD_PAGE_TIMER_SET)
        {
            LcdClock_NextTimerField();
            lcd_need_full_redraw = 1U;
        }
        else if (current_page == LCD_PAGE_TIMER_STATUS)
        {
            AppCountdown_Pause();
            lcd_need_full_redraw = 1U;
        }
        break;

    case LCD_KEY_INC_PIN:
        if (current_page == LCD_PAGE_HOME)
        {
            LcdClock_EnterPage(LCD_PAGE_WEATHER_DETAIL);
        }
        else if ((current_page == LCD_PAGE_DATE_EDIT) || (current_page == LCD_PAGE_TIME_EDIT))
        {
            LcdClock_AdjustSelected(1);
        }
        else if (current_page == LCD_PAGE_TIMER_SET)
        {
            LcdClock_AdjustTimerSet(1);
        }
        else if (current_page == LCD_PAGE_TIMER_STATUS)
        {
            AppCountdown_Resume();
            lcd_need_full_redraw = 1U;
        }
        break;

    case LCD_KEY_DEC_PIN:
        if (current_page == LCD_PAGE_HOME)
        {
            LcdClock_EnterPage(LCD_PAGE_TIME_EDIT);
        }
        else if (current_page == LCD_PAGE_WEATHER_DETAIL)
        {
            LcdClock_SendDiag("lcd weather detail refresh\r\n");
            (void)LcdClock_UpdateWeatherStatus();
            lcd_need_full_redraw = 1U;
        }
        else if ((current_page == LCD_PAGE_DATE_EDIT) || (current_page == LCD_PAGE_TIME_EDIT))
        {
            LcdClock_AdjustSelected(-1);
        }
        else if (current_page == LCD_PAGE_TIMER_SET)
        {
            LcdClock_AdjustTimerSet(-1);
        }
        else if (current_page == LCD_PAGE_TIMER_STATUS)
        {
            AppCountdown_Cancel();
            LcdClock_EnterPage(LCD_PAGE_HOME);
        }
        else if (current_page == LCD_PAGE_TIMER_CONFIRM)
        {
            AppCountdown_Start(LcdClock_TimerSetTotalSeconds());
            LcdClock_EnterPage(LCD_PAGE_TIMER_STATUS);
        }
        break;

    case LCD_KEY_SAVE_PIN:
        if ((current_page == LCD_PAGE_DATE_EDIT) || (current_page == LCD_PAGE_TIME_EDIT))
        {
            LcdClock_SaveEdit();
        }
        else if (current_page == LCD_PAGE_WEATHER_DETAIL)
        {
            LcdClock_EnterPage(LCD_PAGE_HOME);
        }
        else if (current_page == LCD_PAGE_HOME)
        {
            if (timer_state == APP_COUNTDOWN_NONE)
            {
                LcdClock_EnterPage(LCD_PAGE_TIMER_SET);
            }
            else
            {
                LcdClock_EnterPage(LCD_PAGE_TIMER_STATUS);
            }
        }
        else if (current_page == LCD_PAGE_TIMER_SET)
        {
            if (LcdClock_TimerSetTotalSeconds() == 0U)
            {
                LcdClock_EnterPage(LCD_PAGE_HOME);
            }
            else
            {
                LcdClock_EnterPage(LCD_PAGE_TIMER_CONFIRM);
            }
        }
        else if (current_page == LCD_PAGE_TIMER_CONFIRM)
        {
            LcdClock_EnterPage(LCD_PAGE_HOME);
        }
        else if (current_page == LCD_PAGE_TIMER_STATUS)
        {
            if (timer_state == APP_COUNTDOWN_DONE)
            {
                AppCountdown_CloseDone();
            }
            LcdClock_EnterPage(LCD_PAGE_HOME);
        }
        else if ((current_page == LCD_PAGE_WEATHER) ||
                 (current_page == LCD_PAGE_STATUS))
        {
            LcdClock_SendDiag("lcd weather refresh\r\n");
            (void)LcdClock_UpdateWeatherStatus();
            LcdClock_EnterPage(LCD_PAGE_HOME);
        }
        break;

    default:
        return;
    }

    last_key_ms = HAL_GetTick();
    LcdClock_ReportKeyAction(pin);
    LcdClock_DrawScreen();
    last_display_ms = HAL_GetTick();
}
static void LcdClock_HandlePendingKeys(void)
{
    uint16_t pending;
    uint32_t now = HAL_GetTick();

    if (key_pending_pins == 0U)
    {
        return;
    }

    if ((now - key_last_edge_ms) < LCD_KEY_DEBOUNCE_MS)
    {
        return;
    }

    if ((now - last_key_ms) < LCD_KEY_GUARD_MS)
    {
        return;
    }

    __disable_irq();
    pending = key_pending_pins;
    key_pending_pins = 0U;
    __enable_irq();

    if ((pending & LCD_KEY_SELECT_PIN) != 0U)
    {
        LcdClock_DoKeyAction(LCD_KEY_SELECT_PIN);
    }
    else if ((pending & LCD_KEY_INC_PIN) != 0U)
    {
        LcdClock_DoKeyAction(LCD_KEY_INC_PIN);
    }
    else if ((pending & LCD_KEY_DEC_PIN) != 0U)
    {
        LcdClock_DoKeyAction(LCD_KEY_DEC_PIN);
    }
    else if ((pending & LCD_KEY_SAVE_PIN) != 0U)
    {
        LcdClock_DoKeyAction(LCD_KEY_SAVE_PIN);
    }
}

/* 倒计时完成后无论当前在哪个页面，都强制跳到计时状态页提醒用户处理报警。 */
static uint8_t LcdClock_HandleCountdownDoneJump(void)
{
    AppCountdown_State_t state = AppCountdown_GetState();

    if ((last_countdown_state != APP_COUNTDOWN_DONE) && (state == APP_COUNTDOWN_DONE))
    {
        last_countdown_state = state;
        LcdClock_SendDiag("lcd timer done jump\r\n");
        LcdClock_EnterPage(LCD_PAGE_TIMER_STATUS);
        LcdClock_DrawScreen();
        last_display_ms = HAL_GetTick();
        return 1U;
    }

    last_countdown_state = state;
    return 0U;
}
/* 首页 10 秒检查一次 ESP32 是否仍连着 WiFi，掉线则尝试主动重连。 */
static void LcdClock_HandleHomeWifiCheck(void)
{
    uint32_t now;

    if ((lcd_initialized == 0U) || (current_page != LCD_PAGE_HOME))
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - last_home_wifi_check_ms) < LCD_HOME_WIFI_CHECK_MS)
    {
        return;
    }

    last_home_wifi_check_ms = now;
    if (AppWifi_CheckConnected() != 0U)
    {
        home_wifi_connected = 1U;
        LcdClock_SendDiag("lcd wifi check CONNECT\r\n");
    }
    else
    {
        LcdClock_SendDiag("lcd wifi check DISCONNECT, reconnect\r\n");
        if (AppWifi_Join() != 0U)
        {
            home_wifi_connected = 1U;
            LcdClock_SendDiag("lcd wifi reconnect OK\r\n");
        }
        else
        {
            home_wifi_connected = 0U;
            LcdClock_SendDiag("lcd wifi reconnect failed\r\n");
        }
    }

    LcdClock_DrawScreen();
    last_display_ms = HAL_GetTick();
}

/* 首页天气自动刷新：复位后约 5 秒第一次请求，之后 15 分钟一次。 */
static void LcdClock_HandleHomeAutoWeather(void)
{
    uint32_t now;

    if ((lcd_initialized == 0U) || (current_page != LCD_PAGE_HOME))
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - last_home_weather_refresh_ms) < LCD_HOME_WEATHER_REFRESH_MS)
    {
        return;
    }

    last_home_weather_refresh_ms = now;
    LcdClock_SendDiag("lcd home auto weather refresh\r\n");
    (void)LcdClock_UpdateWeatherStatus();
    LcdClock_DrawScreen();
    last_display_ms = HAL_GetTick();
    last_home_weather_refresh_ms = last_display_ms;
}

void AppLcdClock_Init(void)
{
    LcdClock_SendDiag("lcd init start\r\n");
    Inf_LCD_Init();
    LcdClock_PrintId();

    LcdClock_EnterPage(LCD_PAGE_HOME);
    last_home_weather_refresh_ms = HAL_GetTick() - (LCD_HOME_WEATHER_REFRESH_MS - LCD_HOME_WEATHER_FIRST_REFRESH_MS);
    LcdClock_DrawScreen();
    last_display_ms = HAL_GetTick();
    lcd_initialized = 1U;
}

void AppLcdClock_Task(void)
{
    if (LcdClock_HandleCountdownDoneJump() != 0U)
    {
        return;
    }

    LcdClock_HandlePendingKeys();
    LcdClock_HandleHomeWifiCheck();
    LcdClock_HandleHomeAutoWeather();

    if ((HAL_GetTick() - last_display_ms) >= 1000U)
    {
        LcdClock_DrawScreen();
        last_display_ms += 1000U;
    }
}

void BspEspAt_WaitHook(void)
{
    uint32_t now;

    if ((lcd_initialized == 0U) ||
        (lcd_wait_hook_active != 0U) ||
        (current_page != LCD_PAGE_HOME))
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - last_display_ms) < 1000U)
    {
        return;
    }

    lcd_wait_hook_active = 1U;
    LcdClock_DrawScreen();
    last_display_ms = now;
    lcd_wait_hook_active = 0U;
}

uint8_t AppLcdClock_ProcessCommand(const char *command)
{
    if (strcmp(command, "LCD_ID") == 0)
    {
        LcdClock_PrintId();
        return 1U;
    }
    if (strcmp(command, "LCD_TEST") == 0)
    {
        LcdClock_SendDiag("lcd color test\r\n");
        LcdClock_ShowColorProbe();
        lcd_need_full_redraw = 1U;
        LcdClock_DrawScreen();
        return 1U;
    }
    if (strcmp(command, "LCD_BL_HIGH") == 0)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        LcdClock_SendDiag("lcd bl high\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_BL_LOW") == 0)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        LcdClock_SendDiag("lcd bl low\r\n");
        return 1U;
    }
    if (strcmp(command, "KEY_STATE") == 0)
    {
        LcdClock_PrintKeyState();
        return 1U;
    }
    if (strcmp(command, "LCD_HOME") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_HOME);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page home\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_DATE") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_DATE_EDIT);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page date edit\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_TIME") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_TIME_EDIT);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page time edit\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_WEATHER") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_WEATHER);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page weather\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_DETAIL") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_WEATHER_DETAIL);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page weather detail\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_STATUS") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_STATUS);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page status\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_TIMER_SET") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_TIMER_SET);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page timer set\r\n");
        return 1U;
    }
    if (strcmp(command, "LCD_TIMER_STATUS") == 0)
    {
        LcdClock_EnterPage(LCD_PAGE_TIMER_STATUS);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("lcd page timer status\r\n");
        return 1U;
    }
    if (strcmp(command, "TIMER_5S") == 0)
    {
        AppCountdown_Start(5U);
        LcdClock_EnterPage(LCD_PAGE_TIMER_STATUS);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("timer start 5s\r\n");
        return 1U;
    }
    if (strcmp(command, "TIMER_CANCEL") == 0)
    {
        AppCountdown_Cancel();
        LcdClock_EnterPage(LCD_PAGE_HOME);
        LcdClock_DrawScreen();
        LcdClock_SendDiag("timer canceled\r\n");
        return 1U;
    }

    return 0U;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    int8_t index = LcdClock_KeyIndex(GPIO_Pin);

    if (index < 0)
    {
        return;
    }

    key_pending_pins |= GPIO_Pin;
    key_last_edge_ms = HAL_GetTick();
    key_irq_counts[index]++;
}


#include "app_weather.h"
#include "app_debug_uart.h"
#include "app_wifi.h"
#include "bsp_esp_at.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define APP_WEATHER_HOST      "restapi.amap.com"
#define APP_WEATHER_KEY       "YOUR_AMAP_WEB_SERVICE_KEY"
#define APP_WEATHER_AUTO_IP_LOCATION 1U
/* IP 定位失败时使用上海市编码，避免把个人地址经纬度写死在开源代码里。 */
#define APP_WEATHER_FALLBACK_CITY    "310000"
#define APP_WEATHER_IP_PATH          "/v3/ip?key=" APP_WEATHER_KEY "&output=JSON"
#define APP_WEATHER_RESP_SIZE 1800U

static char weather_resp[APP_WEATHER_RESP_SIZE];
static char weather_req[480];
static char weather_path[192];
static char weather_city[16] = APP_WEATHER_FALLBACK_CITY;
static AppWeather_Data_t weather_data;

static uint8_t AppWeather_RequestRaw(uint8_t print_preview);
static uint8_t AppWeather_RequestHttpPathRaw(const char *path, uint8_t print_preview);
static uint8_t AppWeather_UpdateCityByIp(void);
static uint8_t AppWeather_ParseCurrent(const char *response, AppWeather_Data_t *data);
static const char *AppWeather_FindKeyInCurrent(const char *current_start, const char *current_end, const char *key);
static uint8_t AppWeather_ReadString(const char *value_start, char *out, uint16_t out_size);
static uint8_t AppWeather_ReadScaledNumber(const char *value_start, int32_t scale, int32_t *out);
static uint8_t AppWeather_ReadUnsigned(const char *value_start, uint16_t *out);
static uint8_t AppWeather_ReadFirstUnsigned(const char *value_start, uint16_t *out);
static void AppWeather_NormalizeWindPower(const char *raw, char *out, uint16_t out_size);
static uint8_t AppWeather_ContainsUtf8(const char *text, const char *needle);
static uint16_t AppWeather_MapAmapWeatherCode(const char *weather_name);
static const char *AppWeather_CodeName(uint16_t code);
static void AppWeather_SendString(const char *text);
static void AppWeather_PrintPreview(const char *response);
static void AppWeather_PrintSignedScaled(const char *name, int32_t value, uint16_t scale, const char *unit);
static void AppWeather_PrintUnsignedScaled(const char *name, uint32_t value, uint16_t scale, const char *unit);

/**
  * @brief 通过高德天气接口拉取原始 HTTP/JSON 响应。
  */
uint8_t AppWeather_FetchRaw(void)
{
    return AppWeather_RequestRaw(1U);
}

/**
  * @brief 获取天气并解析实况对象。
  *
  * 每次天气请求前先走一次高德 IP 定位，定位失败则回落到兜底城市。
  */
uint8_t AppWeather_Update(void)
{
    uint8_t attempt;

#if APP_WEATHER_AUTO_IP_LOCATION
    (void)AppWeather_UpdateCityByIp();
#endif

    for (attempt = 1U; attempt <= 2U; attempt++)
    {
        if (!AppWeather_RequestRaw(0U))
        {
            weather_data.valid = 0U;
            return 0U;
        }

        if (AppWeather_ParseCurrent(weather_resp, &weather_data))
        {
            weather_data.valid = 1U;
            AppWeather_SendString("[WEATHER] parse OK\r\n");
            AppWeather_PrintData();
            return 1U;
        }

        AppWeather_SendString("[WEATHER] parse failed, retry\r\n");
    }

    weather_data.valid = 0U;
    AppWeather_SendString("[WEATHER] parse failed\r\n");
    return 0U;
}

void AppWeather_PrintData(void)
{
    char line[96];

    if (!weather_data.valid)
    {
        AppWeather_SendString("[WEATHER] no valid data\r\n");
        return;
    }

    AppWeather_SendString("[WEATHER] data:\r\n");
    snprintf(line, sizeof(line), "  city adcode: %s\r\n", weather_city);
    AppWeather_SendString(line);
    snprintf(line, sizeof(line), "  time: %s\r\n", weather_data.time);
    AppWeather_SendString(line);
    AppWeather_PrintSignedScaled("  temp", weather_data.temperature_x10, 10U, "C");
    AppWeather_PrintSignedScaled("  feel", weather_data.apparent_temperature_x10, 10U, "C");
    snprintf(line, sizeof(line), "  humidity: %u%%\r\n", weather_data.humidity);
    AppWeather_SendString(line);
    AppWeather_PrintUnsignedScaled("  rain", weather_data.precipitation_x100, 100U, "mm");
    snprintf(line, sizeof(line),
             "  code: %u (%s)\r\n",
             weather_data.weather_code,
             AppWeather_CodeName(weather_data.weather_code));
    AppWeather_SendString(line);
    AppWeather_PrintUnsignedScaled("  wind speed", weather_data.wind_speed_x10, 10U, "km/h");
    snprintf(line, sizeof(line), "  wind dir: %s\r\n", weather_data.wind_direction_text);
    AppWeather_SendString(line);
    snprintf(line, sizeof(line), "  wind power: %s\r\n", weather_data.wind_power_text);
    AppWeather_SendString(line);
}

const AppWeather_Data_t *AppWeather_GetData(void)
{
    return &weather_data;
}

static uint8_t AppWeather_RequestRaw(uint8_t print_preview)
{
    uint16_t path_len;

    /* weather_city 可能来自本次 IP 定位，也可能是兜底城市编码。 */
    path_len = (uint16_t)snprintf(weather_path,
                                  sizeof(weather_path),
                                  "/v3/weather/weatherInfo?key=%s&city=%s&extensions=base&output=JSON",
                                  APP_WEATHER_KEY,
                                  weather_city);
    if ((path_len == 0U) || (path_len >= sizeof(weather_path)))
    {
        AppWeather_SendString("[WEATHER] weather path too long\r\n");
        return 0U;
    }

    return AppWeather_RequestHttpPathRaw(weather_path, print_preview);
}

static uint8_t AppWeather_RequestHttpPathRaw(const char *path, uint8_t print_preview)
{
    int ret;
    uint16_t req_len;
    char cmd[48];

    if ((path == NULL) || (path[0] == '\0'))
    {
        return 0U;
    }

    AppWeather_SendString("\r\n[WEATHER] raw fetch start\r\n");

    if (AppWifi_GetState() != APP_WIFI_STATE_READY)
    {
        AppWeather_SendString("[WEATHER] WiFi not ready, join first\r\n");
        if (!AppWifi_Join())
        {
            AppWeather_SendString("[WEATHER] WiFi join failed\r\n");
            return 0U;
        }
    }

    /* 先清理旧 TCP 连接，避免 ESP32-C3 仍处在上一轮 CLOSED/CONNECTING 状态。 */
    (void)BspEspAt_SendCmd("AT+CIPCLOSE\r\n", "OK", weather_resp, sizeof(weather_resp), 1000U);
    (void)BspEspAt_SendCmd("AT+CIPMUX=0\r\n", "OK", weather_resp, sizeof(weather_resp), 1000U);

    ret = BspEspAt_SendCmd("AT+CIPSTART=\"TCP\",\"" APP_WEATHER_HOST "\",80\r\n",
                           "CONNECT",
                           weather_resp,
                           sizeof(weather_resp),
                           15000U);
    if (ret != BSP_ESP_AT_OK)
    {
        AppWeather_SendString("[WEATHER] TCP connect failed\r\n");
        if (weather_resp[0] != '\0')
        {
            AppWeather_SendString(weather_resp);
            AppWeather_SendString("\r\n");
        }
        return 0U;
    }
    AppWeather_SendString("[WEATHER] TCP connect OK\r\n");

    req_len = (uint16_t)snprintf(weather_req,
                                 sizeof(weather_req),
                                 "GET %s HTTP/1.1\r\n"
                                 "Host: " APP_WEATHER_HOST "\r\n"
                                 "User-Agent: stm32-weather-clock\r\n"
                                 "Accept: application/json\r\n"
                                 "Connection: close\r\n"
                                 "\r\n",
                                 path);
    if ((req_len == 0U) || (req_len >= sizeof(weather_req)))
    {
        AppWeather_SendString("[WEATHER] request too long\r\n");
        return 0U;
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", req_len);
    ret = BspEspAt_SendCmd(cmd, ">", weather_resp, sizeof(weather_resp), 3000U);
    if (ret != BSP_ESP_AT_OK)
    {
        AppWeather_SendString("[WEATHER] CIPSEND prompt failed\r\n");
        if (weather_resp[0] != '\0')
        {
            AppWeather_SendString(weather_resp);
            AppWeather_SendString("\r\n");
        }
        return 0U;
    }

    ret = BspEspAt_SendRaw((uint8_t *)weather_req, req_len);
    if (ret != BSP_ESP_AT_OK)
    {
        AppWeather_SendString("[WEATHER] HTTP request send failed\r\n");
        return 0U;
    }

    ret = BspEspAt_ReadUntil("CLOSED", weather_resp, sizeof(weather_resp), 15000U);
    if ((ret != BSP_ESP_AT_OK) && (weather_resp[0] == '\0'))
    {
        AppWeather_SendString("[WEATHER] no HTTP response\r\n");
        return 0U;
    }

    if (strstr(weather_resp, "HTTP/1.1 200") != NULL)
    {
        AppWeather_SendString("[WEATHER] HTTP 200 OK\r\n");
        if (print_preview)
        {
            AppWeather_PrintPreview(weather_resp);
        }
        return 1U;
    }

    AppWeather_SendString("[WEATHER] HTTP response not 200\r\n");
    AppWeather_PrintPreview(weather_resp);
    return 0U;
}

static uint8_t AppWeather_UpdateCityByIp(void)
{
    const char *value;
    char adcode[16];
    char line[64];

    AppWeather_SendString("[WEATHER] IP locate start\r\n");
    if (!AppWeather_RequestHttpPathRaw(APP_WEATHER_IP_PATH, 0U))
    {
        strncpy(weather_city, APP_WEATHER_FALLBACK_CITY, sizeof(weather_city) - 1U);
        weather_city[sizeof(weather_city) - 1U] = '\0';
        AppWeather_SendString("[WEATHER] IP locate request failed, use fallback city\r\n");
        return 0U;
    }

    if (strstr(weather_resp, "\"status\":\"1\"") == NULL)
    {
        strncpy(weather_city, APP_WEATHER_FALLBACK_CITY, sizeof(weather_city) - 1U);
        weather_city[sizeof(weather_city) - 1U] = '\0';
        AppWeather_SendString("[WEATHER] IP locate status failed, use fallback city\r\n");
        AppWeather_PrintPreview(weather_resp);
        return 0U;
    }

    value = strstr(weather_resp, "\"adcode\":\"");
    if ((value == NULL) ||
        !AppWeather_ReadString(value + strlen("\"adcode\":\""), adcode, sizeof(adcode)) ||
        (adcode[0] == '\0'))
    {
        strncpy(weather_city, APP_WEATHER_FALLBACK_CITY, sizeof(weather_city) - 1U);
        weather_city[sizeof(weather_city) - 1U] = '\0';
        AppWeather_SendString("[WEATHER] IP locate no adcode, use fallback city\r\n");
        AppWeather_PrintPreview(weather_resp);
        return 0U;
    }

    strncpy(weather_city, adcode, sizeof(weather_city) - 1U);
    weather_city[sizeof(weather_city) - 1U] = '\0';

    snprintf(line, sizeof(line), "[WEATHER] IP locate city adcode: %s\r\n", weather_city);
    AppWeather_SendString(line);
    return 1U;
}

static uint8_t AppWeather_ParseCurrent(const char *response, AppWeather_Data_t *data)
{
    const char *live_start;
    const char *live_end;
    const char *value;
    char weather_name[32];
    int32_t scaled;
    uint16_t unsigned_value;

    if ((response == NULL) || (data == NULL))
    {
        return 0U;
    }

    memset(data, 0, sizeof(*data));

    /* 高德 extensions=base 返回 lives[0]，本项目只解析 LCD 会显示的实况字段。 */
    if (strstr(response, "\"status\":\"1\"") == NULL)
    {
        return 0U;
    }

    live_start = strstr(response, "\"lives\":[{");
    if (live_start == NULL)
    {
        return 0U;
    }

    live_end = strchr(live_start, '}');
    if (live_end == NULL)
    {
        return 0U;
    }

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"reporttime\":\"");
    if ((value != NULL) && AppWeather_ReadString(value, data->time, sizeof(data->time)))
    {
        /* reporttime is kept for serial diagnostics; LCD detail page does not show it. */
    }

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"temperature\":\"");
    if ((value == NULL) || !AppWeather_ReadScaledNumber(value, 10, &scaled))
    {
        return 0U;
    }
    data->temperature_x10 = (int16_t)scaled;
    data->apparent_temperature_x10 = data->temperature_x10;

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"humidity\":\"");
    if ((value == NULL) || !AppWeather_ReadUnsigned(value, &unsigned_value))
    {
        return 0U;
    }
    data->humidity = (uint8_t)unsigned_value;

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"weather\":\"");
    if ((value == NULL) || !AppWeather_ReadString(value, weather_name, sizeof(weather_name)))
    {
        return 0U;
    }
    data->weather_code = AppWeather_MapAmapWeatherCode(weather_name);

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"winddirection\":\"");
    if (value != NULL)
    {
        (void)AppWeather_ReadString(value, data->wind_direction_text, sizeof(data->wind_direction_text));
    }

    value = AppWeather_FindKeyInCurrent(live_start, live_end, "\"windpower\":\"");
    if (value != NULL)
    {
        char raw_wind_power[16];

        if (AppWeather_ReadString(value, raw_wind_power, sizeof(raw_wind_power)))
        {
            AppWeather_NormalizeWindPower(raw_wind_power, data->wind_power_text, sizeof(data->wind_power_text));
        }
    }

    if ((value != NULL) && AppWeather_ReadFirstUnsigned(value, &unsigned_value))
    {
        data->wind_speed_x10 = (uint16_t)(unsigned_value * 10U);
    }

    data->precipitation_x100 = 0U;
    data->wind_direction = 0U;

    return 1U;
}

static const char *AppWeather_FindKeyInCurrent(const char *current_start, const char *current_end, const char *key)
{
    const char *found = strstr(current_start, key);

    if ((found == NULL) || (found >= current_end))
    {
        return NULL;
    }

    return found + strlen(key);
}

static uint8_t AppWeather_ReadString(const char *value_start, char *out, uint16_t out_size)
{
    uint16_t len = 0U;

    if ((value_start == NULL) || (out == NULL) || (out_size == 0U))
    {
        return 0U;
    }

    while ((value_start[len] != '\0') && (value_start[len] != '"') && (len < (uint16_t)(out_size - 1U)))
    {
        out[len] = value_start[len];
        len++;
    }

    out[len] = '\0';
    return (value_start[len] == '"') ? 1U : 0U;
}

static uint8_t AppWeather_ReadScaledNumber(const char *value_start, int32_t scale, int32_t *out)
{
    int32_t sign = 1;
    int32_t integer = 0;
    int32_t fraction = 0;
    int32_t fraction_scale = scale / 10;
    const char *p = value_start;

    if ((value_start == NULL) || (out == NULL) || (scale <= 0))
    {
        return 0U;
    }

    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }

    if (*p == '-')
    {
        sign = -1;
        p++;
    }

    if ((*p < '0') || (*p > '9'))
    {
        return 0U;
    }

    while ((*p >= '0') && (*p <= '9'))
    {
        integer = (integer * 10) + (*p - '0');
        p++;
    }

    if (*p == '.')
    {
        p++;
        while ((*p >= '0') && (*p <= '9') && (fraction_scale > 0))
        {
            fraction += (*p - '0') * fraction_scale;
            fraction_scale /= 10;
            p++;
        }
    }

    *out = sign * ((integer * scale) + fraction);
    return 1U;
}

static uint8_t AppWeather_ReadUnsigned(const char *value_start, uint16_t *out)
{
    uint32_t value = 0U;
    const char *p = value_start;

    if ((value_start == NULL) || (out == NULL))
    {
        return 0U;
    }

    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }

    if ((*p < '0') || (*p > '9'))
    {
        return 0U;
    }

    while ((*p >= '0') && (*p <= '9'))
    {
        value = (value * 10U) + (uint32_t)(*p - '0');
        p++;
    }

    if (value > 65535U)
    {
        return 0U;
    }

    *out = (uint16_t)value;
    return 1U;
}

static uint8_t AppWeather_ReadFirstUnsigned(const char *value_start, uint16_t *out)
{
    const char *p = value_start;

    if ((value_start == NULL) || (out == NULL))
    {
        return 0U;
    }

    while ((*p != '\0') && ((*p < '0') || (*p > '9')))
    {
        if (*p == '"')
        {
            return 0U;
        }
        p++;
    }

    return AppWeather_ReadUnsigned(p, out);
}

static void AppWeather_NormalizeWindPower(const char *raw, char *out, uint16_t out_size)
{
    static const char utf8_le[] = "\xE2\x89\xA4";
    uint16_t i = 0U;
    uint16_t o = 0U;

    if ((raw == NULL) || (out == NULL) || (out_size == 0U))
    {
        return;
    }

    while ((raw[i] != '\0') && (o < (uint16_t)(out_size - 1U)))
    {
        /* 高德可能返回 UTF-8 的“≤”，LCD 侧按 ASCII 的 <= 显示。 */
        if (strncmp(&raw[i], utf8_le, 3U) == 0)
        {
            if (o < (uint16_t)(out_size - 2U))
            {
                out[o++] = '<';
                out[o++] = '=';
            }
            i = (uint16_t)(i + 3U);
            continue;
        }

        if (((raw[i] >= '0') && (raw[i] <= '9')) ||
            (raw[i] == '<') ||
            (raw[i] == '=') ||
            (raw[i] == '-') ||
            (raw[i] == '+'))
        {
            out[o++] = raw[i];
        }

        i++;
    }

    out[o] = '\0';
}

static uint8_t AppWeather_ContainsUtf8(const char *text, const char *needle)
{
    if ((text == NULL) || (needle == NULL))
    {
        return 0U;
    }

    return (strstr(text, needle) != NULL) ? 1U : 0U;
}

static uint16_t AppWeather_MapAmapWeatherCode(const char *weather_name)
{
    static const char utf8_clear[] = "\xE6\x99\xB4";
    static const char utf8_cloud[] = "\xE4\xBA\x91";
    static const char utf8_overcast[] = "\xE9\x98\xB4";
    static const char utf8_rain[] = "\xE9\x9B\xA8";
    static const char utf8_snow[] = "\xE9\x9B\xAA";
    static const char utf8_fog[] = "\xE9\x9B\xBE";
    static const char utf8_haze[] = "\xE9\x9C\xBE";
    static const char utf8_thunder[] = "\xE9\x9B\xB7";
    static const char utf8_sand[] = "\xE6\xB2\x99";
    static const char utf8_dust[] = "\xE5\xB0\x98";

    /* LCD 只准备了少量天气图标文字，这里把高德中文天气归并到通用类别。 */
    if (AppWeather_ContainsUtf8(weather_name, utf8_thunder))
    {
        return 95U;
    }
    if (AppWeather_ContainsUtf8(weather_name, utf8_snow))
    {
        return 71U;
    }
    if (AppWeather_ContainsUtf8(weather_name, utf8_rain))
    {
        return 61U;
    }
    if (AppWeather_ContainsUtf8(weather_name, utf8_fog) ||
        AppWeather_ContainsUtf8(weather_name, utf8_haze) ||
        AppWeather_ContainsUtf8(weather_name, utf8_sand) ||
        AppWeather_ContainsUtf8(weather_name, utf8_dust))
    {
        return 45U;
    }
    if (AppWeather_ContainsUtf8(weather_name, utf8_cloud) ||
        AppWeather_ContainsUtf8(weather_name, utf8_overcast))
    {
        return 2U;
    }
    if (AppWeather_ContainsUtf8(weather_name, utf8_clear))
    {
        return 0U;
    }

    return 999U;
}

static const char *AppWeather_CodeName(uint16_t code)
{
    if (code == 0U)
    {
        return "Clear";
    }
    if ((code >= 1U) && (code <= 3U))
    {
        return "Cloudy";
    }
    if ((code == 45U) || (code == 48U))
    {
        return "Fog";
    }
    if (((code >= 51U) && (code <= 57U)))
    {
        return "Drizzle";
    }
    if (((code >= 61U) && (code <= 67U)) || ((code >= 80U) && (code <= 82U)))
    {
        return "Rain";
    }
    if (((code >= 71U) && (code <= 77U)))
    {
        return "Snow";
    }
    if ((code >= 95U) && (code <= 99U))
    {
        return "Thunder";
    }
    return "Unknown";
}

static void AppWeather_SendString(const char *text)
{
    AppDebugUart_SendString(text);
}

static void AppWeather_PrintPreview(const char *response)
{
    const char *body = strstr(response, "{\"status\"");
    uint16_t count = 0U;

    AppWeather_SendString("[WEATHER] preview:\r\n");

    if (body == NULL)
    {
        body = response;
    }

    while ((body[count] != '\0') && (count < 900U))
    {
        AppDebugUart_SendBytes((const uint8_t *)&body[count], 1U);
        count++;
    }

    AppWeather_SendString("\r\n[WEATHER] raw fetch done\r\n");
}

static void AppWeather_PrintSignedScaled(const char *name, int32_t value, uint16_t scale, const char *unit)
{
    char line[64];
    const char *sign = "";
    uint32_t abs_value;

    if (value < 0)
    {
        sign = "-";
        abs_value = (uint32_t)(-value);
    }
    else
    {
        abs_value = (uint32_t)value;
    }

    if (scale == 10U)
    {
        snprintf(line, sizeof(line), "%s: %s%lu.%lu %s\r\n",
                 name,
                 sign,
                 (unsigned long)(abs_value / 10U),
                 (unsigned long)(abs_value % 10U),
                 unit);
    }
    else
    {
        snprintf(line, sizeof(line), "%s: %s%lu %s\r\n",
                 name,
                 sign,
                 (unsigned long)abs_value,
                 unit);
    }
    AppWeather_SendString(line);
}

static void AppWeather_PrintUnsignedScaled(const char *name, uint32_t value, uint16_t scale, const char *unit)
{
    char line[64];

    if (scale == 10U)
    {
        snprintf(line, sizeof(line), "%s: %lu.%lu %s\r\n",
                 name,
                 (unsigned long)(value / 10U),
                 (unsigned long)(value % 10U),
                 unit);
    }
    else if (scale == 100U)
    {
        snprintf(line, sizeof(line), "%s: %lu.%02lu %s\r\n",
                 name,
                 (unsigned long)(value / 100U),
                 (unsigned long)(value % 100U),
                 unit);
    }
    else
    {
        snprintf(line, sizeof(line), "%s: %lu %s\r\n", name, (unsigned long)value, unit);
    }
    AppWeather_SendString(line);
}


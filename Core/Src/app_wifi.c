#include "app_wifi.h"
#include "app_debug_uart.h"
#include "bsp_esp_at.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define APP_WIFI_SSID      "your ssid"
#define APP_WIFI_PASSWORD  "your password"
#define APP_WIFI_RESP_SIZE 512U

static AppWifi_State_t wifi_state = APP_WIFI_STATE_IDLE;
static char wifi_ip[32] = "0.0.0.0";
static char wifi_resp[APP_WIFI_RESP_SIZE];

static void AppWifi_SendString(const char *text);
static void AppWifi_ExtractIp(const char *response);

void AppWifi_Init(void)
{
    wifi_state = APP_WIFI_STATE_IDLE;
    strncpy(wifi_ip, "0.0.0.0", sizeof(wifi_ip));
    wifi_ip[sizeof(wifi_ip) - 1U] = '\0';
}

/**
  * @brief 连接配置在程序中的 WiFi 热点。
  *
  * 串口日志只打印 SSID，不打印密码；连接成功后再读取 ESP32 的 STA IP。
  */
uint8_t AppWifi_Join(void)
{
    int ret;
    char cmd[96];

    AppWifi_SendString("\r\n[WIFI] join start\r\n");
    AppWifi_SendString("[WIFI] ssid: " APP_WIFI_SSID "\r\n");

    ret = BspEspAt_SendCmd("AT+CWMODE=1\r\n", "OK", wifi_resp, sizeof(wifi_resp), 2000U);
    if (ret != BSP_ESP_AT_OK)
    {
        wifi_state = APP_WIFI_STATE_ERROR;
        AppWifi_SendString("[WIFI] CWMODE failed\r\n");
        if (wifi_resp[0] != '\0')
        {
            AppWifi_SendString(wifi_resp);
            AppWifi_SendString("\r\n");
        }
        return 0U;
    }
    AppWifi_SendString("[WIFI] CWMODE OK\r\n");

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", APP_WIFI_SSID, APP_WIFI_PASSWORD);
    ret = BspEspAt_SendCmd(cmd, "OK", wifi_resp, sizeof(wifi_resp), 20000U);
    if (ret != BSP_ESP_AT_OK)
    {
        wifi_state = APP_WIFI_STATE_ERROR;
        AppWifi_SendString("[WIFI] CWJAP failed\r\n");
        if (wifi_resp[0] != '\0')
        {
            AppWifi_SendString(wifi_resp);
            AppWifi_SendString("\r\n");
        }
        return 0U;
    }
    AppWifi_SendString("[WIFI] CWJAP OK\r\n");
    AppWifi_SendString(wifi_resp);
    AppWifi_SendString("\r\n");

    ret = BspEspAt_SendCmd("AT+CIPSTA?\r\n", "OK", wifi_resp, sizeof(wifi_resp), 3000U);
    if (ret == BSP_ESP_AT_OK)
    {
        AppWifi_ExtractIp(wifi_resp);
        wifi_state = APP_WIFI_STATE_READY;
        AppWifi_SendString("[WIFI] IP: ");
        AppWifi_SendString(wifi_ip);
        AppWifi_SendString("\r\n");
        return 1U;
    }

    wifi_state = APP_WIFI_STATE_ERROR;
    AppWifi_SendString("[WIFI] CIPSTA failed\r\n");
    if (wifi_resp[0] != '\0')
    {
        AppWifi_SendString(wifi_resp);
        AppWifi_SendString("\r\n");
    }
    return 0U;
}

/**
  * @brief 查询 ESP32 当前是否仍连接目标 WiFi。
  *
  * 首页 10 秒周期调用；失败时只更新状态，主动重连由 LCD 应用层决定。
  */
uint8_t AppWifi_CheckConnected(void)
{
    int ret;

    ret = BspEspAt_SendCmd("AT+CWJAP?\r\n", "OK", wifi_resp, sizeof(wifi_resp), 3000U);
    if ((ret == BSP_ESP_AT_OK) &&
        (strstr(wifi_resp, "+CWJAP:\"") != NULL) &&
        (strstr(wifi_resp, APP_WIFI_SSID) != NULL))
    {
        wifi_state = APP_WIFI_STATE_READY;
        return 1U;
    }

    wifi_state = APP_WIFI_STATE_ERROR;
    strncpy(wifi_ip, "0.0.0.0", sizeof(wifi_ip));
    wifi_ip[sizeof(wifi_ip) - 1U] = '\0';
    return 0U;
}

void AppWifi_PrintState(void)
{
    AppWifi_SendString("[WIFI] state: ");
    if (wifi_state == APP_WIFI_STATE_READY)
    {
        AppWifi_SendString("READY, ip=");
        AppWifi_SendString(wifi_ip);
        AppWifi_SendString("\r\n");
    }
    else if (wifi_state == APP_WIFI_STATE_ERROR)
    {
        AppWifi_SendString("ERROR\r\n");
    }
    else
    {
        AppWifi_SendString("IDLE\r\n");
    }
}

AppWifi_State_t AppWifi_GetState(void)
{
    return wifi_state;
}

static void AppWifi_SendString(const char *text)
{
    AppDebugUart_SendString(text);
}

/* 从 AT+CIPSTA? 响应中提取 STA IP，用于串口诊断显示。 */
static void AppWifi_ExtractIp(const char *response)
{
    const char *ip_start = strstr(response, "+CIPSTA:ip:\"");
    const char *ip_end;
    uint16_t len;

    if (ip_start == NULL)
    {
        strncpy(wifi_ip, "0.0.0.0", sizeof(wifi_ip));
        wifi_ip[sizeof(wifi_ip) - 1U] = '\0';
        return;
    }

    ip_start += strlen("+CIPSTA:ip:\"");
    ip_end = strchr(ip_start, '"');
    if (ip_end == NULL)
    {
        return;
    }

    len = (uint16_t)(ip_end - ip_start);
    if (len >= sizeof(wifi_ip))
    {
        len = (uint16_t)(sizeof(wifi_ip) - 1U);
    }

    memcpy(wifi_ip, ip_start, len);
    wifi_ip[len] = '\0';
}

#ifndef __APP_WEATHER_H__
#define __APP_WEATHER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef struct {
    char time[24];
    int16_t temperature_x10;
    uint8_t humidity;
    int16_t apparent_temperature_x10;
    uint16_t precipitation_x100;
    uint16_t weather_code;
    uint16_t wind_speed_x10;
    uint16_t wind_direction;
    char wind_direction_text[24];
    char wind_power_text[16];
    uint8_t valid;
} AppWeather_Data_t;

uint8_t AppWeather_FetchRaw(void);
uint8_t AppWeather_Update(void);
void AppWeather_PrintData(void);
const AppWeather_Data_t *AppWeather_GetData(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEATHER_H__ */

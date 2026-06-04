#ifndef APP_DATETIME_H
#define APP_DATETIME_H

#include "main.h"

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DateTime_t;

void App_GetRtcDateTime(DateTime_t *dt);
void App_SetRtcDateTime(const DateTime_t *dt);

#endif /* APP_DATETIME_H */

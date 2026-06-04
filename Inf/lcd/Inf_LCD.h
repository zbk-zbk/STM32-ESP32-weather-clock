#ifndef __INF_LCD_H
#define __INF_LCD_H

#include "math.h"
#include "main.h"

#define SRAM_BANK4 0x6C000000
#define LCD_ADDR_CMD ((volatile uint16_t *)SRAM_BANK4)
#define LCD_ADDR_DATA ((volatile uint16_t *)(SRAM_BANK4 + (1 << 11)))

#define DISPLAY_W 320
#define DISPLAY_H 480

/* 常见颜色 */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40 // 棕色
#define BRRED 0XFC07 // 棕红色
#define GRAY 0X8430  // 灰色

void Inf_LCD_WriteData(uint16_t data);
uint16_t Inf_LCD_ReadData(void);

void Inf_LCD_RegConfig(void);

void Inf_LCD_Init(void);
uint32_t Inf_LCD_ReadId(void);
void Inf_LCD_WriteAsciiChar(uint16_t x, uint16_t y, uint16_t heigh, uint8_t c, uint16_t fColor, uint16_t bColor);
void Inf_LCD_SetArea(int16_t x, uint16_t y, uint16_t w, uint16_t h);
void Inf_LCD_ClearAll(uint16_t color);
void Inf_LCD_WriteAsciiString(uint16_t x, uint16_t y, uint16_t heigh, uint8_t *c, uint16_t fColor, uint16_t bColor);
void Inf_LCD_WriteChineseChar(uint16_t x, uint16_t y, uint8_t index, uint16_t fColor, uint16_t bColor);
void Inf_LCD_WriteChineseChar16(uint16_t x, uint16_t y, uint8_t index, uint16_t fColor, uint16_t bColor);
void Inf_LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void Inf_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t color);
void Inf_LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t w, uint16_t color);
void Inf_LCD_DrawCircle(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color);
void Inf_LCD_DrawCircle_1(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t color);
void Inf_LCD_DrawCircleFill(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t BColor, uint16_t FColor);
void Inf_LCD_DrawCircleFill_1(uint16_t xCenter, uint16_t yCenter, uint16_t r, uint16_t w, uint16_t BColor, uint16_t FColor);
// void Inf_LCD_WriteAtguiguLogo(uint16_t x, uint16_t y);

#endif


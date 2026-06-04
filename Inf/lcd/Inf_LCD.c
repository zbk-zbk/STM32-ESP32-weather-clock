#include "Inf_LCD.h"
#include "Inf_LCD_Font.h"

void Inf_LCD_Reset(void)
{
    GPIOG->ODR &= ~GPIO_ODR_ODR15;
    HAL_Delay(100);
    GPIOG->ODR |= GPIO_ODR_ODR15;
    HAL_Delay(100);
}

/**
 * @description: 给LCD开器背光
 * @return {*}
 */
void Inf_LCD_BKOpen()
{
    GPIOB->ODR |= GPIO_ODR_ODR0;
}

/**
 * @description: 关闭LCD的背光
 * @return {*}
 */
void Inf_LCD_BKClose()
{
    GPIOB->ODR &= ~GPIO_ODR_ODR0;
}

void Inf_LCD_WriteCmd(uint16_t cmd)
{
    *LCD_ADDR_CMD = cmd;
}

void Inf_LCD_WriteData(uint16_t data)
{
    *LCD_ADDR_DATA = data;
}

uint16_t Inf_LCD_ReadData(void)
{
    return *LCD_ADDR_DATA;
}

/* 初始化寄存器的值 */
void Inf_LCD_RegConfig(void)
{
    /* 1. 设置灰阶电压以调整TFT面板的伽马特性， 正校准。一般出厂就设置好了 */
    Inf_LCD_WriteCmd(0xE0);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x07);
    Inf_LCD_WriteData(0x10);
    Inf_LCD_WriteData(0x09);
    Inf_LCD_WriteData(0x17);
    Inf_LCD_WriteData(0x0B);
    Inf_LCD_WriteData(0x41);
    Inf_LCD_WriteData(0x89);
    Inf_LCD_WriteData(0x4B);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x0C);
    Inf_LCD_WriteData(0x0E);
    Inf_LCD_WriteData(0x18);
    Inf_LCD_WriteData(0x1B);
    Inf_LCD_WriteData(0x0F);

    /* 2. 设置灰阶电压以调整TFT面板的伽马特性，负校准 */
    Inf_LCD_WriteCmd(0XE1);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x17);
    Inf_LCD_WriteData(0x1A);
    Inf_LCD_WriteData(0x04);
    Inf_LCD_WriteData(0x0E);
    Inf_LCD_WriteData(0x06);
    Inf_LCD_WriteData(0x2F);
    Inf_LCD_WriteData(0x45);
    Inf_LCD_WriteData(0x43);
    Inf_LCD_WriteData(0x02);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x09);
    Inf_LCD_WriteData(0x32);
    Inf_LCD_WriteData(0x36);
    Inf_LCD_WriteData(0x0F);

    /* 3.  Adjust Control 3 (F7h)  */
    /*LCD_WriteCmd(0XF7);
   Inf_LCD_WriteData(0xA9);
   Inf_LCD_WriteData(0x51);
   Inf_LCD_WriteData(0x2C);
   Inf_LCD_WriteData(0x82);*/
    /* DSI write DCS command, use loose packet RGB 666 */

    /* 4. 电源控制1*/
    Inf_LCD_WriteCmd(0xC0);
    Inf_LCD_WriteData(0x11); /* 正伽马电压 */
    Inf_LCD_WriteData(0x09); /* 负伽马电压 */

    /* 5. 电源控制2 */
    Inf_LCD_WriteCmd(0xC1);
    Inf_LCD_WriteData(0x02);
    Inf_LCD_WriteData(0x03);

    /* 6. VCOM控制 */
    Inf_LCD_WriteCmd(0XC5);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x80);

    /* 7. Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
    Inf_LCD_WriteCmd(0xB1);
    Inf_LCD_WriteData(0xB0);
    Inf_LCD_WriteData(0x11);

    /* 8.  Display Inversion Control (B4h) （正负电压反转，减少电磁干扰）*/
    Inf_LCD_WriteCmd(0xB4);
    Inf_LCD_WriteData(0x02);

    /* 9.  Display Function Control (B6h)  */
    Inf_LCD_WriteCmd(0xB6);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0xA2);

    /* 10. Entry Mode Set (B7h)  */
    Inf_LCD_WriteCmd(0xB7);
    Inf_LCD_WriteData(0xc6);

    /* 11. HS Lanes Control (BEh) */
    Inf_LCD_WriteCmd(0xBE);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x04);

    /* 12.  Interface Pixel Format (3Ah) */
    Inf_LCD_WriteCmd(0x3A);
    Inf_LCD_WriteData(0x55); /* 0x55 : 16 bits/pixel  */

    /* 13. Sleep Out (11h) 关闭休眠模式 */
    Inf_LCD_WriteCmd(0x11);

    /* 14. 设置屏幕方向和RGB */
    Inf_LCD_WriteCmd(0x36);
    Inf_LCD_WriteData(0x08);

    HAL_Delay(120);

    /* 14. display on */
    Inf_LCD_WriteCmd(0x29);
}

/**
 * @description: 初始化LCD
 * @return {*}
 */
 void Inf_LCD_Init(void)
{
    /* 1. 初始化FSMC */
    //Driver_FSMC_Init();
    /* 3. 重置LCD */
    Inf_LCD_Reset();
    /* 4. 开启背光 */
    Inf_LCD_BKOpen();
    /* 5. 对LCD做一些基本配置 */
    Inf_LCD_RegConfig();
}

/**
 * @description: 读取id, 一般用来验证驱动是否正常
 */
uint32_t Inf_LCD_ReadId(void)
{
    uint32_t id = 0;
    /* 首先发送读取ID的指令 */
    Inf_LCD_WriteCmd(0x04);
    Inf_LCD_ReadData();
    id |= (Inf_LCD_ReadData() & 0xff) << 16;
    id |= (Inf_LCD_ReadData() & 0xff) << 8;
    id |= (Inf_LCD_ReadData() & 0xff);
    return id;
}
/**
 * @description:
 * @param {uint16_t} x 字符的x坐标
 * @param {uint16_t} y 字符的y坐标
 * @param {uint16_t} heigh 字符的高度. 宽度是高度的一半
 * @param {uint8_t} c 要显示的字符
 * @param {uint16_t} fColor 字符颜色
 * @param {uint16_t} bColor 字符的背景色
 */
void Inf_LCD_WriteAsciiChar(uint16_t x,
                            uint16_t y,
                            uint16_t heigh,
                            uint8_t c,
                            uint16_t fColor,
                            uint16_t bColor)
{
    /* 1. 先确定区域 */
    Inf_LCD_SetArea(x, y, heigh / 2, heigh);
    /* 2. 显示字符 */
    // 发送显示颜色的指令
    Inf_LCD_WriteCmd(0x2C);
    /* 2.1 计算出字符在对应的数组中的下标 */
    uint8_t index = c - ' ';
    /* 2.2 找到这个字符的编码 */
    if (heigh == 16 || heigh == 12)
    {
        for (uint8_t i = 0; i < heigh; i++)
        {
            uint8_t tmp = heigh == 16 ? ascii_1608[index][i] : ascii_1206[index][i];
            /* 遍历字节中的每一位 */
            for (uint8_t j = 0; j < heigh / 2; j++)
            {
                if (tmp & 0x01) // 如果是1显示前景色
                {
                    Inf_LCD_WriteData(fColor);
                }
                else // 显示背景色
                {
                    Inf_LCD_WriteData(bColor);
                }
                tmp >>= 1;
            }
        }
    }
    else if (heigh == 24)
    {
        for (uint8_t i = 0; i < 48; i++)
        {
            uint8_t tmp = ascii_2412[index][i];
            uint8_t jCount = i % 2 ? 4 : 8; /* 奇数下标的时候,只需要遍历低4位 */
            for (uint8_t j = 0; j < jCount; j++)
            {
                if (tmp & 0x01) // 如果是1显示前景色
                {
                    Inf_LCD_WriteData(fColor);
                }
                else // 显示背景色
                {
                    Inf_LCD_WriteData(bColor);
                }
                tmp >>= 1;
            }
        }
    }
    else if (heigh == 32)
    {
        for (uint8_t i = 0; i < 64; i++)
        {
            uint8_t tmp = ascii_3216[index][i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (tmp & 0x01) // 如果是1显示前景色
                {
                    Inf_LCD_WriteData(fColor);
                }
                else // 显示背景色
                {
                    Inf_LCD_WriteData(bColor);
                }
                tmp >>= 1;
            }
        }
    }
}

/**
 * @description: 设置要写的字符的区域
 * @param {int16_t} x
 * @param {uint16_t} y
 * @return {*}
 */
void Inf_LCD_SetArea(int16_t x,
                     uint16_t y,
                     uint16_t w,
                     uint16_t h)
{
    /* 1. 设置列 */
    Inf_LCD_WriteCmd(0x2a);
    /* 1.1 开始列 */
    Inf_LCD_WriteData(x >> 8);   /* 列的高位 */
    Inf_LCD_WriteData(x & 0xff); /* 列低位 */
    /* 1.2 结束列 */
    Inf_LCD_WriteData((x + w - 1) >> 8);
    Inf_LCD_WriteData((x + w - 1) & 0xff);

    /* 2. 设置行 */
    Inf_LCD_WriteCmd(0x2b);
    /* 2.1 开始行 */
    Inf_LCD_WriteData(y >> 8);
    Inf_LCD_WriteData(y & 0xff);
    /* 2.2 结束行 */
    Inf_LCD_WriteData((y + h - 1) >> 8);
    Inf_LCD_WriteData((y + h - 1) & 0xff);
}

/**
 * @description: 把屏幕清除为指定的颜色
 * @param {uint16_t} color
 */
void Inf_LCD_ClearAll(uint16_t color)
{
    Inf_LCD_SetArea(0, 0, 320, 480);
    Inf_LCD_WriteCmd(0x2C);
    for (uint32_t i = 0, count = 320 * 480; i < count; i++)
    {
        Inf_LCD_WriteData(color);
    }
}

void Inf_LCD_WriteAsciiString(uint16_t x,
                              uint16_t y,
                              uint16_t heigh,
                              uint8_t *c,
                              uint16_t fColor,
                              uint16_t bColor) // "abc"
{
    uint8_t i = 0;
    while (c[i] != '\0')
    {
        if (c[i] != '\n')
        {
            if (x + heigh / 2 > DISPLAY_W)
            {
                x = 0;
                y += heigh;
            }
            Inf_LCD_WriteAsciiChar(x, y, heigh, c[i], fColor, bColor);
            x += heigh / 2;
        }
        else
        {
            x = 0;
            y += heigh;
        }

        i++;
    }
}

void Inf_LCD_WriteChineseChar(uint16_t x,
                              uint16_t y,
                              uint8_t index,
                              uint16_t fColor,
                              uint16_t bColor)
{
    Inf_LCD_SetArea(x, y, 32, 32);

    Inf_LCD_WriteCmd(0x2C);

    for (uint16_t i = 0; i < 128; i++)
    {
        uint8_t tmp = chinese[index][i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (tmp & 0x01) // 如果是1显示前景色
            {
                Inf_LCD_WriteData(fColor);
            }
            else // 显示背景色
            {
                Inf_LCD_WriteData(bColor);
            }
            tmp >>= 1;
        }
    }
}

static uint8_t Inf_LCD_GetChinese32Pixel(uint8_t index, uint8_t x, uint8_t y)
{
    uint8_t data = chinese[index][(uint16_t)y * 4U + (x / 8U)];
    return (uint8_t)((data >> (x % 8U)) & 0x01U);
}

void Inf_LCD_WriteChineseChar16(uint16_t x,
                                uint16_t y,
                                uint8_t index,
                                uint16_t fColor,
                                uint16_t bColor)
{
    Inf_LCD_SetArea(x, y, 16, 16);
    Inf_LCD_WriteCmd(0x2C);

    for (uint8_t row = 0U; row < 16U; row++)
    {
        for (uint8_t col = 0U; col < 16U; col++)
        {
            uint8_t pixel;

            if (index >= 128U)
            {
                uint8_t data = chinese_1616_extra[index - 128U][(uint16_t)row * 2U + (col / 8U)];
                pixel = (uint8_t)((data >> (col % 8U)) & 0x01U);
            }
            else
            {
                uint8_t src_x = (uint8_t)(col * 2U);
                uint8_t src_y = (uint8_t)(row * 2U);
                pixel = (uint8_t)(Inf_LCD_GetChinese32Pixel(index, src_x, src_y) |
                                  Inf_LCD_GetChinese32Pixel(index, (uint8_t)(src_x + 1U), src_y) |
                                  Inf_LCD_GetChinese32Pixel(index, src_x, (uint8_t)(src_y + 1U)) |
                                  Inf_LCD_GetChinese32Pixel(index, (uint8_t)(src_x + 1U), (uint8_t)(src_y + 1U)));
            }

            Inf_LCD_WriteData(pixel ? fColor : bColor);
        }
    }
}
/*
void Inf_LCD_WriteAtguiguLogo(uint16_t x, uint16_t y)
{
    Inf_LCD_SetArea(x, y, 206, 54);

    Inf_LCD_WriteCmd(0x2C);
    uint16_t len = sizeof(gImage_atguigu);
    for (uint16_t i = 0; i < len; i += 2)
    {
        uint16_t p = gImage_atguigu[i]  + (gImage_atguigu[i + 1] << 8);
        Inf_LCD_WriteData(p);
    }
}
*/
/*
void Inf_LCD_WriteAtguiguLogo(uint16_t x, uint16_t y)
{
    Inf_LCD_SetArea(x, y, DISPLAY_W, DISPLAY_H);

    Inf_LCD_WriteCmd(0x2C);
    uint32_t len = sizeof(gImage_a);
    for (uint32_t i = 0; i < len; i += 2)
    {
        uint16_t p = gImage_a[i]  + (gImage_a[i + 1] << 8);
        Inf_LCD_WriteData(p);
    }
}
*/

void Inf_LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    Inf_LCD_SetArea(x, y, w, w);
    Inf_LCD_WriteCmd(0x2C);
    uint16_t count = w * w;
    for (uint16_t i = 0; i < count; i++)
    {
        Inf_LCD_WriteData(color);
    }
}
#include "stdio.h"

void Inf_LCD_DrawLine(uint16_t x1,
                      uint16_t y1,
                      uint16_t x2,
                      uint16_t y2,
                      uint16_t w,
                      uint16_t color)
{
    if (x1 == x2)
    {
        for (uint16_t y = y1; y <= y2; y++)
        {
            Inf_LCD_DrawPoint(x1, y, w, color);
        }
        return;
    }

    /*
        y = kx + b
            k = (y2 - y1)/(x2 - x1);
            b = y1 - k * x1
    */
    double k = 1.0 * (y2 - y1) / (x2 - x1);
    double b = y1 - k * x1;

    //printf("%d,%d,%d,%d\r\n", x1, y1,x2,y2);
    //printf("k=%f,d=%f\r\n", k, b);
    

    for (uint16_t x = x1; x <= x2; x++)
    {
        uint16_t y = (uint16_t)(k * x + b);
        Inf_LCD_DrawPoint(x, y, w, color);
    }
}

void Inf_LCD_DrawRectangle(uint16_t x1,
                           uint16_t y1,
                           uint16_t x2,
                           uint16_t y2,
                           uint16_t w,
                           uint16_t color)
{
    Inf_LCD_DrawLine(x1, y1, x2, y1, w, color);
    Inf_LCD_DrawLine(x2, y1, x2, y2, w, color);
    Inf_LCD_DrawLine(x1, y1, x1, y2, w, color);
    Inf_LCD_DrawLine(x1, y2, x2, y2, w, color);
}

void Inf_LCD_DrawCircle(uint16_t xCenter,
                        uint16_t yCenter,
                        uint16_t r,
                        uint16_t w,
                        uint16_t color)
{
    for (uint16_t angle = 0; angle < 360; angle++)
    {
        uint16_t x = xCenter + r * sin(angle * 3.14 / 180);
        uint16_t y = yCenter + r * cos(angle * 3.14 / 180);

        Inf_LCD_DrawPoint(x, y, w, color);
    }
}

void Inf_LCD_DrawCircle_1(uint16_t xCenter,
                          uint16_t yCenter,
                          uint16_t r,
                          uint16_t w,
                          uint16_t color)
{
    for (uint16_t angle = 0; angle < 90; angle++)
    {
        uint16_t s = r * sin(angle * 3.14 / 180);
        uint16_t c = r * cos(angle * 3.14 / 180);

        uint16_t x = xCenter + s;
        uint16_t y = yCenter + c;
        Inf_LCD_DrawPoint(x, y, w, color);

        x = xCenter - s;
        y = yCenter + c;
        Inf_LCD_DrawPoint(x, y, w, color);

        x = xCenter + s;
        y = yCenter - c;
        Inf_LCD_DrawPoint(x, y, w, color);

        x = xCenter - s;
        y = yCenter - c;
        Inf_LCD_DrawPoint(x, y, w, color);
    }
}

void Inf_LCD_DrawCircleFill(uint16_t xCenter,
                            uint16_t yCenter,
                            uint16_t r,
                            uint16_t w,
                            uint16_t BColor,
                            uint16_t FColor)
{
    for (uint16_t i = 0; i <= r; i++)
    {
        for (uint16_t angle = 0; angle < 360; angle++)
        {
            uint16_t x = xCenter + i * sin(angle * 3.14 / 180);
            uint16_t y = yCenter + i * cos(angle * 3.14 / 180);
            if (i == r)
            {
                Inf_LCD_DrawPoint(x, y, w, BColor);
            }
            else
            {
                Inf_LCD_DrawPoint(x, y, w, FColor);
            }
        }
    }
}

void Inf_LCD_DrawCircleFill_1(uint16_t xCenter,
                              uint16_t yCenter,
                              uint16_t r,
                              uint16_t w,
                              uint16_t BColor,
                              uint16_t FColor)
{

    for (uint16_t angle = 0; angle < 90; angle++)
    {
        uint16_t s = r * sin(angle * 3.14 / 180);
        uint16_t c = r * cos(angle * 3.14 / 180);

        uint16_t x1 = xCenter + s;
        uint16_t y1 = yCenter + c;
        Inf_LCD_DrawPoint(x1, y1, w, BColor);

        uint16_t x2 = xCenter - s;
        uint16_t y2 = yCenter + c;
        Inf_LCD_DrawPoint(x2, y2, w, BColor);

        Inf_LCD_DrawLine(x2 + w, y1, x1 - w, y2, w, FColor);

        x1 = xCenter + s;
        y1 = yCenter - c;
        Inf_LCD_DrawPoint(x1, y1, w, BColor);

        x2 = xCenter - s;
        y2 = yCenter - c;
        Inf_LCD_DrawPoint(x2, y2, w, BColor);

        Inf_LCD_DrawLine(x2 + w, y1, x1 - w, y2, w, FColor);
    }
}

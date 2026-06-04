#include "Inf_W25Q32.h"
/*
一共64块(0-63) 每块16个扇区(0-15)  每个扇区16页(0-15)  每页256个字节
*/
void Inf_W25Q32_Init(void)
{
    Driver_SPI_Init();
}

void Inf_W25Q32_ReadId(uint8_t *mid, uint16_t *did)
{

    Driver_SPI_Start();

    /* 1. 发送 Jedec id指令 */
    Driver_SPI_SwapByte(0x9f);

    /* 2. 获取厂商id (发送的数据随意)*/
    *mid = Driver_SPI_SwapByte(0xff);

    /* 3. 获取设备id */
    *did = 0;
    *did |= Driver_SPI_SwapByte(0xff) << 8;
    *did |= Driver_SPI_SwapByte(0xff) & 0xff;

    Driver_SPI_Stop();
}

/**
 * @description: 读取内部寄存器的busy,一直等到不忙再结束函数
 * @return {*}
 */
void Inf_W25Q32_WaiteNotBusy(void)
{
    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x05);

    while (Driver_SPI_SwapByte(0xff) & 0x01)
        ;
    Driver_SPI_Stop();
}

/**
 * @description: FLASH写使能  0x06
 * @return {*}
 */
void Inf_W25q32_WiteEnable(void)
{
    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x06);
    Driver_SPI_Stop();
}

/**
 * @description: FLASH写失能  0x04
 * @return {*}
 */
void Inf_W25q32_WiteDisanable(void)
{
    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x04);
    Driver_SPI_Stop();
}

/**
 * @description: 擦除指定块内的指定扇区
 *  一共64块(0-63) 每块16个扇区(0-15)  每个扇区16页(0-15)  每页256个字节
 * @param {uint8_t} block
 * @param {uint8_t} sector
 */
void Inf_W25Q32_EraseSector(uint8_t block, uint8_t sector)
{
    Inf_W25Q32_WaiteNotBusy();

    Inf_W25q32_WiteEnable();
    /* 计算出要擦除的扇区的首地址 */
    uint32_t sectorAddr = block * 0x010000 + sector * 0x001000;

    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x20);
    // 0000 0000 0000 0000 0000 0000
    Driver_SPI_SwapByte(sectorAddr >> 16 & 0xff);
    Driver_SPI_SwapByte(sectorAddr >> 8 & 0xff);
    Driver_SPI_SwapByte(sectorAddr & 0xff);
    Driver_SPI_Stop();

    Inf_W25q32_WiteDisanable();
}

/**
 * @description: 执行页写入 写入时,从指定页的首地址开始写入.
 * @param {uint8_t} block
 * @param {uint8_t} sector
 * @param {uint8_t} page
 * @param {uint8_t*} data
 * @param {uint16_t} len
 */
void Inf_W25Q32_WritePage(uint8_t block,
                          uint8_t sector,
                          uint8_t page,
                          uint8_t *data,
                          uint16_t len)
{
    Inf_W25Q32_WaiteNotBusy();

    Inf_W25q32_WiteEnable();
    /* 计算出要写入的数据的页的首地址 */
    uint32_t pageAddr = block * 0x010000 + sector * 0x001000 + page * 0x000100;

    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x02);

    Driver_SPI_SwapByte(pageAddr >> 16 & 0xff);
    Driver_SPI_SwapByte(pageAddr >> 8 & 0xff);
    Driver_SPI_SwapByte(pageAddr & 0xff);

    for (uint16_t i = 0; i < len; i++)
    {
        Driver_SPI_SwapByte(data[i]);
    }

    Driver_SPI_Stop();

    Inf_W25q32_WiteDisanable();
}

/**
 * @description: 从flash读取数据
 * @return {*}
 */
void Inf_W25Q32_Read(uint8_t block,
                     uint8_t sector,
                     uint8_t page,
                     uint8_t *data,
                     uint16_t len)
{
    Inf_W25Q32_WaiteNotBusy();
    
    uint32_t pageAddr = block * 0x010000 + sector * 0x001000 + page * 0x000100;

    Driver_SPI_Start();
    Driver_SPI_SwapByte(0x03);

    Driver_SPI_SwapByte(pageAddr >> 16 & 0xff);
    Driver_SPI_SwapByte(pageAddr >> 8 & 0xff);
    Driver_SPI_SwapByte(pageAddr & 0xff);
    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Driver_SPI_SwapByte(0xff);
    }

    Driver_SPI_Stop();
}
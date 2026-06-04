#ifndef __INF_W25Q32_
#define __INF_W25Q32_

#include "Driver_SPI.h"

void Inf_W25Q32_Init(void);

void Inf_W25Q32_ReadId(uint8_t *mid, uint16_t *did);

void Inf_W25q32_WiteEnable(void);

void Inf_W25q32_WiteDisanable(void);

void Inf_W25Q32_EraseSector(uint8_t block, uint8_t sector);

void Inf_W25Q32_WritePage(uint8_t block, uint8_t sector, uint8_t page, uint8_t *data, uint16_t len);

void Inf_W25Q32_Read(uint8_t block, uint8_t sector, uint8_t page, uint8_t *data, uint16_t len);

#endif


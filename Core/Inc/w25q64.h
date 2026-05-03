#ifndef __W25Q64_H__
#define __W25Q64_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "quadspi.h"

#define W25Q64_SECTOR_SIZE      4096U
#define W25Q64_BLOCK_SIZE        65536U
#define W25Q64_PAGE_SIZE         256U
#define W25Q64_CAPACITY          8388608UL   /* 8 MB */
#define W25Q64_SECTOR_COUNT      (W25Q64_CAPACITY / W25Q64_SECTOR_SIZE)
#define W25Q64_MANUFACTURER_ID   0xEF
#define W25Q64_DEVICE_ID         0x4017

bool       W25Q64_Init(void);
bool       W25Q64_Reset(void);
uint32_t   W25Q64_ReadID(void);
bool       W25Q64_IsPresent(void);
uint32_t   W25Q64_GetCapacity(void);
const char* W25Q64_GetName(void);

bool W25Q64_EraseSector(uint32_t addr);
bool W25Q64_EraseBlock(uint32_t addr);
bool W25Q64_ChipErase(void);

bool W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);
bool W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q64_H__ */

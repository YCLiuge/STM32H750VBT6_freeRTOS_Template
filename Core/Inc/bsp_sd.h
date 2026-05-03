#ifndef __BSP_SD_H__
#define __BSP_SD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "sdmmc.h"

#define BSP_ERROR_NONE     0
#define BSP_ERROR_PERIPH   (-1)

typedef HAL_SD_CardInfoTypeDef BSP_SD_CardInfo;

int32_t BSP_SD_Init(uint32_t Instance);
int32_t BSP_SD_GetCardState(uint32_t Instance);
int32_t BSP_SD_ReadBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks);
int32_t BSP_SD_WriteBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks);
int32_t BSP_SD_GetCardInfo(uint32_t Instance, HAL_SD_CardInfoTypeDef *CardInfo);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SD_H__ */

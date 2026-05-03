#include "bsp_sd.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"

void MX_FATFS_Init(void)
{
  FATFS_LinkDriver(&SD_Driver, "0:");
}

int32_t BSP_SD_Init(uint32_t Instance)
{
  (void)Instance;

  if (HAL_SD_Init(&hsd1) != HAL_OK)
  {
    return BSP_ERROR_PERIPH;
  }

  return BSP_ERROR_NONE;
}

int32_t BSP_SD_GetCardState(uint32_t Instance)
{
  (void)Instance;

  if (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
  {
    return BSP_ERROR_PERIPH;
  }

  return BSP_ERROR_NONE;
}

int32_t BSP_SD_ReadBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks)
{
  (void)Instance;

  if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)pData, BlockAddr, NumOfBlocks, 5000) != HAL_OK)
  {
    return BSP_ERROR_PERIPH;
  }

  return BSP_ERROR_NONE;
}

int32_t BSP_SD_WriteBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks)
{
  (void)Instance;

  if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)pData, BlockAddr, NumOfBlocks, 5000) != HAL_OK)
  {
    return BSP_ERROR_PERIPH;
  }

  return BSP_ERROR_NONE;
}

int32_t BSP_SD_GetCardInfo(uint32_t Instance, HAL_SD_CardInfoTypeDef *CardInfo)
{
  (void)Instance;

  if (CardInfo == NULL)
  {
    return BSP_ERROR_PERIPH;
  }

  if (HAL_SD_GetCardInfo(&hsd1, CardInfo) != HAL_OK)
  {
    return BSP_ERROR_PERIPH;
  }

  return BSP_ERROR_NONE;
}
